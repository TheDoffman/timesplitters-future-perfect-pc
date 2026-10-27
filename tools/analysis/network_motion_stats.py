"""Summarize asynchronously observed actor motion, not rendered-frame smoothness."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import csv
import json
import math
from pathlib import Path

from network_movement_stats import labels, summarize as settled_summary


def percentile(values, fraction):
    values = sorted(values)
    return values[min(len(values) - 1, math.ceil(fraction * len(values)) - 1)]


def read_samples(folder):
    with (Path(folder) / 'run0-network-motion.csv').open(newline='') as stream:
        return [{key: float(value) if value else math.nan for key, value in row.items()}
                for row in csv.DictReader(stream)]


def measure(rows, slot, start, end, scene, expected_fps):
    rows = [row for row in rows if start <= row['begin'] and row['end'] <= end]
    if len(rows) < 10 or end - start < .5:
        raise ValueError('Insufficient motion observations')
    if not all(row['scene'] == scene and row['network'] == 15 and row['match'] == 7 for row in rows):
        raise ValueError('Motion samples do not belong to the active network scene')
    times = [(row['begin'] + row['end']) * .5 for row in rows]
    positions = [[row[f'{axis}{slot}'] for axis in 'xyz'] for row in rows]
    if not all(math.isfinite(x) for p in positions for x in p):
        raise ValueError('Invalid actor position')
    if not all(row['end'] >= row['begin'] for row in rows) or any(b <= a for a, b in zip(times, times[1:])):
        raise ValueError('Invalid observation timestamps')
    intervals = [b - a for a, b in zip(times, times[1:])]
    changes = [times[i] for i in range(1, len(rows)) if math.dist(positions[i-1], positions[i]) > 1e-6]
    displacement = math.dist(positions[0], positions[-1])
    if displacement < .01 or len(changes) < 2:
        raise ValueError('Stationary or insufficiently changing actor')
    change_gaps = [b - a for a, b in zip(changes, changes[1:])]
    coverage = times[-1] - times[0]
    return dict(samples=len(rows), seconds=coverage, displacement=displacement,
                observed_pose_changes=len(changes), observed_changes_per_second=len(changes)/coverage,
                observation_gap_p99_ms=1000*percentile(intervals, .99),
                observation_gap_max_ms=1000*max(intervals),
                read_duration_max_ms=1000*max(row['end']-row['begin'] for row in rows),
                changed_pose_gap_p50_ms=1000*percentile(change_gaps, .5),
                changed_pose_gap_p99_ms=1000*percentile(change_gaps, .99),
                longest_observed_unchanged_ms=1000*max([changes[0]-times[0], times[-1]-changes[-1], *change_gaps]),
                # An observation gap can hide updates; even dense reads do not
                # establish rendered frame identity, scanout, or packet latency.
                observation_gaps_exceed_frame=sum(gap > 1/expected_fps for gap in intervals),
                expected_fps=expected_fps)


def summarize(host, client):
    folders = [Path(host), Path(client)]
    provenance = [json.loads((folder/'provenance.json').read_text()) for folder in folders]
    if provenance[0]['executable_sha256'] != provenance[1]['executable_sha256']:
        raise ValueError('Peers use different executables')
    if (not provenance[0].get('observation_clock') or
            provenance[0]['observation_clock'] != provenance[1].get('observation_clock') or
            any(not 0 < item.get('clock_resolution', 1) < .001 for item in provenance)):
        raise ValueError('Peers require the same high-resolution observation clock')
    result = settled_summary(host, client)
    result['scope'] = ('Read-only actor observations during trimmed input holds. Asynchronous memory reads '
                       'can miss updates; these are not render/scanout frames, latency or speed-parity measurements.')
    rows = [read_samples(folder) for folder in folders]
    result['motion'] = {}
    for mover, label in enumerate(('host-walk-settled', 'client-walk-settled')):
        held = labels(folders[mover])[label]['hold_samples']
        origin = provenance[mover]['started_monotonic']
        start, end = origin + held[0]['seconds'] + .2, origin + held[-1]['seconds'] - .2
        result['motion']['host' if mover == 0 else 'client'] = {
            'window_monotonic': [start, end],
            **{name: measure(rows[index], mover, start, end, result['scene'],
                            provenance[index]['native_fps'] or 60)
               for name, index in (('local', mover), ('remote', 1-mover))}}
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host', type=Path)
    parser.add_argument('client', type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.host, args.client), indent=2))
