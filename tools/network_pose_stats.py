"""Measure completed gameplay updates; these are not GPU or scanout samples."""
import argparse
import csv
import json
import math
from pathlib import Path

from network_motion_stats import percentile
from network_movement_stats import labels


def read_poses(folder):
    with (Path(folder)/'run0-network-poses.csv').open(newline='') as stream:
        return [{key: float(value) for key, value in row.items()} for row in csv.DictReader(stream)]


def measure(rows, slot, start, end, scene):
    rows = [row for row in rows if start <= row['seconds'] <= end]
    if len(rows) < 10 or rows[-1]['seconds'] - rows[0]['seconds'] < .8*(end-start):
        raise ValueError('Insufficient completed-update coverage')
    if any(row['scene'] != scene or row[f'entity{slot}'] != rows[0][f'entity{slot}'] for row in rows):
        raise ValueError('Scene or actor identity changed')
    if any(((int(b['frame'])-int(a['frame'])) & 0xffffffff) != 1 for a, b in zip(rows, rows[1:])):
        raise ValueError('Repeated or missing completed-update frames')
    times = [row['seconds'] for row in rows]
    positions = [[row[f'{axis}{slot}'] for axis in 'xyz'] for row in rows]
    deltas = [row['game_delta'] for row in rows]
    if not all(math.isfinite(x) for x in times+deltas+[x for p in positions for x in p]):
        raise ValueError('Non-finite pose or clock')
    intervals = [b-a for a, b in zip(times, times[1:])]
    if min(intervals) <= 0 or min(deltas) < 0:
        raise ValueError('Invalid clock ordering or game delta')
    changed = [times[i] for i in range(1, len(rows)) if math.dist(positions[i-1], positions[i]) > 1e-6]
    points = [times[0], *changed, times[-1]]
    seconds = times[-1]-times[0]
    stationary = len(intervals)-len(changed)
    return dict(updates=len(intervals), seconds=seconds, update_hz=len(intervals)/seconds,
                changed_updates=len(changed), unchanged_updates=stationary,
                continuous_motion=stationary == 0 and bool(changed),
                displacement=math.dist(positions[0], positions[-1]),
                changed_updates_per_second=len(changed)/seconds,
                longest_unchanged_ms=1000*max(b-a for a, b in zip(points, points[1:])),
                update_interval_p99_ms=1000*percentile(intervals, .99),
                update_interval_max_ms=1000*max(intervals),
                game_time_ratio=sum(deltas[1:])/60/seconds)


def summarize(host, client):
    folders = [Path(host), Path(client)]
    provenance = [json.loads((folder/'provenance.json').read_text()) for folder in folders]
    if provenance[0]['executable_sha256'] != provenance[1]['executable_sha256']:
        raise ValueError('Peers use different executables')
    if any(not p.get('trace_network_poses') or p.get('clock_resolution', 1) >= .001 for p in provenance):
        raise ValueError('Completed poses and high-resolution hold timing are required')
    poses = [read_poses(folder) for folder in folders]
    result = dict(scope='Completed gameplay-update positions, not GPU/scanout, latency or speed parity', motion={})
    for mover, folder in enumerate(folders):
        for label, row in labels(folder).items():
            if not row.get('hold_samples'): continue
            held = row['hold_samples'];origin = provenance[mover]['started_monotonic']
            start, end = origin+held[0]['seconds']+.2, origin+held[-1]['seconds']-.2
            result['motion'][label] = dict(key=row['command']['keys'], window=[start, end],
                **{name: measure(poses[index], mover, start, end, row['scene'])
                   for name, index in (('local', mover), ('remote', 1-mover))})
    if not result['motion']: raise ValueError('No input holds with pose observations')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host', type=Path);parser.add_argument('client', type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.host, args.client), indent=2))
