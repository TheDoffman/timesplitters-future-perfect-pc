"""Summarize CPU update cadence from the bounded native timing CSV.

This does not measure GPU completion, display scanout or input latency.
"""
import argparse
import csv
import json
import math
from pathlib import Path


def percentile(values, fraction):
    ordered = sorted(values)
    index = (len(ordered)-1)*fraction
    low = math.floor(index)
    high = math.ceil(index)
    return ordered[low] + (ordered[high]-ordered[low])*(index-low)


def summarize(rows, start=60.0, scene=2):
    # A running child can be in the middle of its final CSV write. Only an
    # incomplete tail is ignorable; malformed completed records remain errors.
    if rows and any(value is None for value in rows[-1].values()):
        rows=rows[:-1]
    if any(value is None for row in rows for value in row.values()):
        raise ValueError('Incomplete record inside the native trace')
    intervals = []
    raw_ticks = game_ticks = 0.0
    for before, after in zip(rows, rows[1:]):
        if float(before['seconds']) < start:
            continue
        if any(int(row['scene']) != scene or int(row['flags']) & 0x20000000
               or float(row['game_delta']) == 0 for row in (before, after)):
            continue
        duration = float(after['seconds']) - float(before['seconds'])
        if not math.isfinite(duration) or duration <= 0:
            raise ValueError('Native trace timestamps must advance monotonically')
        intervals.append(duration * 1000)
        raw_ticks += float(after['raw_delta'])
        game_ticks += float(after['game_delta'])
    if not intervals:
        raise ValueError(f'No unpaused scene {scene} gameplay intervals in the requested range')
    wall = sum(intervals)/1000
    name={2:'Story',48:'Arcade'}.get(scene,f'scene {scene}')
    return dict(scope=f'unpaused {name} CPU update intervals; no scanout measurement',
                intervals=len(intervals),seconds=round(wall,4),
                mean_update_fps=round(len(intervals)/wall,3),
                frame_ms={name:round(percentile(intervals,q),4)
                          for name,q in [('p50',.5),('p95',.95),('p99',.99)]},
                worst_frame_ms=round(max(intervals),4),
                game_seconds_per_wall_second=round(game_ticks/60/wall,6),
                raw_seconds_per_wall_second=round(raw_ticks/60/wall,6))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path)
    parser.add_argument('--from-seconds',type=float,default=60)
    parser.add_argument('--scene',type=int,default=2,help='Guest scene ID (2 Story, 48 Arcade)')
    args=parser.parse_args()
    with args.trace.open(newline='') as stream:
        rows=list(csv.DictReader(stream))
    print(json.dumps(summarize(rows,args.from_seconds,args.scene),indent=2))


if __name__=='__main__':
    main()
