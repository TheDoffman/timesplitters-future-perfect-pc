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
    return summarize_rows(rows)


def summarize_clock_window(path, start_ns, end_ns):
    """Full intervals within a host-clock window; require evidence on both sides.

    The caller must use the recorder's clock domain (Windows RHI: QPC).
    Raw timestamps avoid guessing process-start offsets. They are not scanout.
    """
    if not isinstance(start_ns,int) or not isinstance(end_ns,int) or not 0<=start_ns<end_ns:
        raise ValueError('Invalid host-clock window')
    with Path(path).open() as source:
        rows=list(csv.DictReader(source))
    if len(rows)<2 or any('clock_ns' not in row for row in rows):
        raise ValueError('Missing host-clock evidence')
    stamps=[int(row['clock_ns']) for row in rows]
    for i in range(1,len(rows)):
        interval=(stamps[i]-stamps[i-1])/1e6
        if (interval<=0 or int(rows[i]['frame'])!=int(rows[i-1]['frame'])+1 or
                not math.isfinite(float(rows[i]['frame_ms'])) or
                abs(interval-float(rows[i]['frame_ms']))>.001):
            raise ValueError('Invalid or missing consecutive clock evidence')
    if stamps[0]>start_ns or stamps[-1]<end_ns:
        raise ValueError('Profile does not cover the requested clock window')
    indices=[i for i in range(1,len(rows)) if stamps[i-1]>=start_ns and stamps[i]<=end_ns]
    result=summarize_rows([rows[i] for i in indices])
    result.update(window_start_ns=start_ns,window_end_ns=end_ns,
                  first_interval_start_ns=stamps[indices[0]-1],last_interval_end_ns=stamps[indices[-1]],
                  scope='Complete successful-present intervals within the host-clock window; not GPU completion or scanout')
    return result


def summarize_rows(rows):
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
    parser.add_argument('--clock-start-ns',type=int)
    parser.add_argument('--clock-end-ns',type=int)
    args = parser.parse_args()
    if args.clock_start_ns is not None or args.clock_end_ns is not None:
        if args.clock_start_ns is None or args.clock_end_ns is None:
            parser.error('Both host-clock window endpoints are required')
        result=summarize_clock_window(args.csv,args.clock_start_ns,args.clock_end_ns)
    else:result=summarize(args.csv,args.start,args.end)
    print(json.dumps(result,indent=2))
