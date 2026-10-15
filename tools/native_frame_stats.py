#!/usr/bin/env python3
"""Summarize a selected interval of the native host-present timing CSV."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics


def summarize(path, start=0, end=math.inf):
    with Path(path).open() as source:
        rows = [row for row in csv.DictReader(source)
                if start * 1000 <= float(row['elapsed_ms']) <= end * 1000]
    if not rows:
        raise ValueError('No frame samples in the requested interval')
    times = sorted(float(row['frame_ms']) for row in rows)
    if any(not math.isfinite(t) or t <= 0 for t in times):
        raise ValueError('Frame intervals must be finite and positive')
    def percentile(p):
        return times[max(0, math.ceil(len(times) * p) - 1)]
    result = dict(samples=len(times),
                first_sample_seconds=float(rows[0]['elapsed_ms']) / 1000,
                last_sample_seconds=float(rows[-1]['elapsed_ms']) / 1000,
                fps=1000 / statistics.mean(times),
                median_ms=statistics.median(times), p95_ms=percentile(.95),
                p99_ms=percentile(.99), worst_ms=max(times),
                frames_over_33_33_ms=sum(t > 1000/30 for t in times),
                frames_over_50_ms=sum(t > 50 for t in times),
                frames_over_100_ms=sum(t > 100 for t in times),
                mean_swap_ms=statistics.mean(float(row['swap_ms']) for row in rows))
    if 'kind' in rows[0]:
        seconds=sum(times)/1000
        for kind in ('guest','interpolated'):
            count=sum(row['kind']==kind for row in rows)
            result[kind+'_presents']=count
            result[kind+'_fps']=count/seconds
    return result



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv')
    parser.add_argument('--start', type=float, default=0, help='Seconds after first host present')
    parser.add_argument('--end', type=float, default=math.inf)
    args = parser.parse_args()
    print(json.dumps(summarize(args.csv, args.start, args.end), indent=2))
