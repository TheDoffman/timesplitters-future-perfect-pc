"""Check settled movement replication; equal stationary endpoints are insufficient."""

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
import json
import math
from pathlib import Path


def labels(folder):
    rows = json.loads((Path(folder) / 'report.json').read_text(encoding='utf-8'))
    return {row['command']['label']: row for row in rows
            if row.get('command', {}).get('label')}


def position(row, slot):
    for actor in row['network']['actors']:
        if actor['slot'] == slot:
            value = actor['position']
            if len(value) == 3 and all(math.isfinite(x) for x in value):
                return value
    raise ValueError(f'Missing finite position for actor slot {slot}')


def compare(before, after, observed, slot, scene):
    if not all(row['scene'] == scene and row['network']['state'] == 15
               and row['match_state'] == 7 for row in (before, after, observed)):
        raise ValueError('Movement snapshots must belong to the same active network scene')
    start, finish, remote = (position(row, slot) for row in (before, after, observed))
    distance, error = math.dist(start, finish), math.dist(finish, remote)
    if distance < .01:
        raise ValueError('No meaningful movement: equal stationary endpoints do not prove replication')
    if error > .01:
        raise ValueError(f'Settled endpoint disagreement: {error:.6f} units')
    return dict(slot=slot, displacement=distance, endpoint_error=error,
                local_position=finish, remote_position=remote)


def summarize(host, client):
    h, c = labels(host), labels(client)
    scene = h['match-start']['scene']
    return dict(scene=scene,
                scope='Settled positions only; not in-motion smoothness, latency or speed parity',
                host=compare(h['match-start'], h['host-walk-settled'],
                             c['host-walk-observed'], 0, scene),
                client=compare(c['host-walk-observed'], c['client-walk-settled'],
                               h['client-walk-observed'], 1, scene))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host', type=Path)
    parser.add_argument('client', type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.host, args.client), indent=2))
