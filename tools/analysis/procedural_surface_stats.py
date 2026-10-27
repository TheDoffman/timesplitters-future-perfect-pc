"""Validate bounded procedural GPU captures and measure actual image freshness.

The synchronous readback changes workload. No wall-clock FPS/scanout claims.
"""

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
from collections import Counter
import csv
import gzip
import json
import math
from pathlib import Path

IMAGE_BYTES=128*128*4
LIMIT=360


def image_hash(data):
    value=14695981039346656037
    for byte in data:
        value=((value ^ byte)*1099511628211)&0xffffffffffffffff
    return f'{value:016x}'


def measure(rows, pixels):
    if not 2<=len(rows)<=LIMIT or len(pixels)!=len(rows)*IMAGE_BYTES:
        raise ValueError('Incomplete or oversized image capture')
    images=[];typed=[]
    for i,row in enumerate(rows):
        value={key:int(row[key]) for key in ('sample','frame','scene','level_ticks','whole_ticks','native','resource','executed','format')}
        value['delta']=float(row['delta'])
        if value['sample']!=i or value['scene']!=4 or value['executed'] not in (0,1) or value['native'] not in (0,1):
            raise ValueError('Unexpected sample/scene/flags')
        if not math.isfinite(value['delta']) or not 0<=value['delta']<=15:
            raise ValueError('Invalid game delta')
        if i and (value['frame']!=(typed[-1]['frame']+1)&0xffffffff or any(value[k]!=typed[0][k] for k in ('resource','format','native'))):
            raise ValueError('Frame gap or changed resource/format/path')
        image=memoryview(pixels)[i*IMAGE_BYTES:(i+1)*IMAGE_BYTES]
        if image_hash(image)!=row['hash']:raise ValueError('Image hash mismatch')
        images.append(image);typed.append(value)
    changed=unchanged=executed_changed=executed_unchanged=skipped_changed=skipped_unchanged=0
    longest=streak=0
    for i in range(1,len(rows)):
        fresh=images[i]!=images[i-1]
        if fresh:changed+=1;streak=0
        else:unchanged+=1;streak+=1;longest=max(longest,streak)
        if typed[i]['executed']:
            if fresh:executed_changed+=1
            else:executed_unchanged+=1
        elif fresh:skipped_changed+=1
        else:skipped_unchanged+=1
    # Integer histograms avoid float32 accumulation drift over millions of
    # bytes. Describe raw byte channels without assuming renderer channel order.
    channel_statistics=[]
    for channel in range(4):
        counts=Counter(pixels[channel::4]);count=sum(counts.values())
        mean=sum(value*frequency for value,frequency in counts.items())/count
        variance=sum((value-mean)**2*frequency for value,frequency in counts.items())/count
        channel_statistics.append(dict(mean=mean,std=math.sqrt(variance),
                                       minimum=min(counts),maximum=max(counts),distinct=len(counts)))
    return dict(channel_statistics=channel_statistics,samples=len(rows),comparisons=len(rows)-1,changed_images=changed,unchanged_images=unchanged,
                executed_changed=executed_changed,executed_unchanged=executed_unchanged,
                skipped_changed=skipped_changed,skipped_unchanged=skipped_unchanged,
                longest_unchanged_updates=longest,game_ticks=sum(r['delta'] for r in typed[1:]),
                native=typed[0]['native'],scope='GPU readback image changes; not FPS, scanout or mechanics parity')


def measure_segments(rows, pixels):
    """Validate separately bounded rate segments; never count across a gap."""
    if not 2 <= len(rows) <= LIMIT or len(pixels) != len(rows) * IMAGE_BYTES:
        raise ValueError('Incomplete or oversized segmented capture')
    groups = []
    for i, row in enumerate(rows):
        segment, rate = int(row['segment']), int(row['rate'])
        if int(row['sample']) != i or rate not in (60, 120, 144, 180, 240) or not float(row['delta']) > 0:
            raise ValueError('Invalid segmented sample/rate/time')
        if not groups or segment != groups[-1]['segment']:
            if segment != len(groups) or segment >= 4:
                raise ValueError('Missing or excess rate segment')
            groups.append(dict(segment=segment, rate=rate, start=i, rows=[]))
        group = groups[-1]
        if rate != group['rate'] or len(group['rows']) >= 90:
            raise ValueError('Mixed rate or oversized segment')
        if rate > 60 and int(row['native']) != 1:
            raise ValueError('High-rate segment without native updates')
        group['rows'].append(dict(row, sample=len(group['rows'])))
    results = []
    for group in groups:
        first = group['start'] * IMAGE_BYTES
        result = measure(group['rows'], pixels[first:first + len(group['rows']) * IMAGE_BYTES])
        results.append(dict(segment=group['segment'], rate=group['rate'], **result))
    return dict(segments=results, samples=len(rows),
                scope='Consecutive GPU images within each rate segment; gaps and pauses excluded, not pacing')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv',type=Path)
    parser.add_argument('--segments',action='store_true')
    parser.add_argument('--switch-cycle',action='store_true',
                        help='Require the complete 240/180/60/120 cycle, 90 images per segment')
    args=parser.parse_args()
    rows,pixels=load_capture(args.csv)
    if args.switch_cycle:
        validate_switch_cycle(rows)
    print(json.dumps((measure_segments if args.segments or args.switch_cycle else measure)(rows,pixels),indent=2))


def validate_switch_cycle(rows, rates=(240,180,60,120)):
    """Fail closed on partial navigation; ordinary segmented analysis allows it."""
    if len(rates)!=4 or any(rate not in (60,120,144,180,240) for rate in rates):
        raise ValueError('Invalid expected FPS cycle')
    if len(rows)!=360:
        raise ValueError('Incomplete FPS switch cycle: expected four sets of 90 images')
    for i,row in enumerate(rows):
        segment=i//90
        rate=rates[segment]
        if (int(row['segment'])!=segment or int(row['rate'])!=rate or
                int(row['native'])!=int(rate>60)):
            raise ValueError('Unexpected FPS switch cycle order or timing path')


def load_capture(path):
    if path.stat().st_size>128*1024:raise ValueError('CSV exceeds bound')
    pixels_path=Path(str(path)+'.rgba')
    compressed=not pixels_path.exists()
    if compressed:pixels_path=Path(str(pixels_path)+'.gz')
    limit=LIMIT*IMAGE_BYTES
    if pixels_path.stat().st_size>limit:raise ValueError('Pixels exceed bound')
    with (gzip.open(pixels_path,'rb') if compressed else pixels_path.open('rb')) as source:
        pixels=source.read(limit+1)
    if len(pixels)>limit:raise ValueError('Expanded pixels exceed bound')
    with path.open(newline='') as source:rows=list(csv.DictReader(source))
    return rows,pixels



if __name__=='__main__':main()
