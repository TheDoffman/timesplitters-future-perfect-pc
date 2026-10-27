"""Read-only search for nested instances of the verified Xbox texture header.

This searches every byte offset inside uncompressed P5CK entries without writing
payload copies. A negative result excludes this header signature only; it does
not establish that other encodings, procedural textures or containers are absent.
Candidates are research evidence, never automatically promoted into the catalog.
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
import hashlib
import importlib.util
import json
import mmap
from pathlib import Path
import re
import struct


spec = importlib.util.spec_from_file_location('catalog', Path(__file__).with_name('texture-catalog.py'))
catalog_tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog_tool)

# Valid little-endian powers of two, 1..4096. Backreferences require the two
# duplicated dimensions used by the executable's texture loader.
DIM = rb'(?:[\x01\x02\x04\x08\x10\x20\x40\x80]\x00\x00\x00|\x00[\x01\x02\x04\x08\x10]\x00\x00)'
HEADER = re.compile(rb'(?P<w>'+DIM+rb')(?P<h>'+DIM+rb')(?P=w)(?P=h).{4}(?P<kind>.{4})\xff\xff\xff\xff', re.DOTALL)


def inspect_header(data, offset, end):
    if offset < 0 or offset+128 > end or end > len(data):
        return None
    w,h,w2,h2,unknown,kind,sentinel = struct.unpack_from('<7I', data, offset)
    if (w != w2 or h != h2 or sentinel != 0xffffffff
            or any(n < 1 or n > 4096 or n & (n-1) for n in (w,h))):
        return None
    if kind in (0,1,4):
        base = ((w+3)//4)*((h+3)//4)*(8 if kind == 0 else 16)
    elif kind in (2,3,5,6,7):
        # Kind 3 is an executable-mapped RGB565 hypothesis, not an enabled
        # replacement format. Keep any occurrence visible for investigation.
        base = w*h*{2:4,3:2,5:1,6:1,7:2}[kind]
    else:
        return dict(width=w,height=h,archive_format=kind,status='unknown_format',base_bytes=None)
    return dict(width=w,height=h,archive_format=kind,base_bytes=base,
                status='candidate' if offset+128+base <= end else 'truncated_base')


def find_headers(data, start, end):
    for match in HEADER.finditer(data, start, max(start,end-100)):
        offset=match.start()
        result=inspect_header(data,offset,end)
        if result is not None:
            yield offset,result


def scan(game, root):
    catalog=json.loads((root/'catalog.json').read_text(encoding='utf-8'))
    known={(s['archive'],s['offset']):t for t in catalog['textures'] for s in t['sources'] if not s.get('embedded') and s.get('source_kind')!='xbe_section'}
    totals=Counter();prefixes=Counter();archives=[];candidates=[];rejected=Counter()
    for path in sorted((game/'pak').rglob('*.pak')):
        rel=path.relative_to(game).as_posix();counts=Counter();digest=hashlib.sha256()
        with path.open('rb') as f:
            while chunk:=f.read(1024*1024):digest.update(chunk)
            with mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as data:
                for _,crc,pos,length,compressed in catalog_tool.entries(path):
                    counts['entries']+=1
                    if compressed:
                        counts['compressed_entries_skipped']+=1
                        continue
                    counts['entries_scanned']+=1;counts['entry_bytes_scanned']+=length
                    if (rel,pos) not in known:
                        counts['uncataloged_entries_scanned']+=1
                        prefixes[data[pos:pos+min(length,4)].hex()]+=1
                    for offset,row in find_headers(data,pos,pos+length):
                        if offset==pos and (rel,pos) in known:
                            counts['cataloged_headers_found']+=1
                            continue
                        if row['status']=='truncated_base':
                            rejected['truncated_base']+=1
                            continue
                        row.update(archive=rel,entry_crc=f'{crc:08x}',entry_offset=pos,
                                   entry_length=length,relative_offset=offset-pos,
                                   header_sha256=hashlib.sha256(data[offset:offset+128]).hexdigest())
                        if row['base_bytes'] is not None:
                            raw=data[offset+128:offset+128+row['base_bytes']]
                            row['base_sha256']=hashlib.sha256(raw).hexdigest()
                            row.update(catalog_tool.classify(row['archive_format'],row['width'],row['height'],raw))
                            row['status']='unverified_embedded_candidate'
                            row['runtime_validation']='Candidate identity only; no embedded scene validation'
                        candidates.append(row);counts['candidate_headers']+=1
        archives.append(dict(archive=rel,bytes=path.stat().st_size,sha256=digest.hexdigest(),**counts))
        totals.update(counts)
    if totals['cataloged_headers_found'] != len(known):
        raise ValueError(f"Scanner missed cataloged headers: {totals['cataloged_headers_found']} / {len(known)}")
    return dict(schema=1,complete=False,
        method='Every byte offset in every uncompressed P5CK entry; repeated power-of-two dimensions, sentinel and 128-byte header with bounded base payload.',
        limitations=['Other headers and encodings are not excluded.',
                     'Compressed entries, if present, are not decoded.',
                     'Non-PAK files and procedurally generated textures are outside this scan.',
                     'Candidates require decoder and executable-reference verification before catalog promotion.'],
        totals=dict(totals),rejected=dict(rejected),
        unclassified_prefix_counts=dict(prefixes.most_common()),
        candidates=candidates,archives=archives)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game',type=Path,default=Path('private/game'))
    ap.add_argument('--root',type=Path,default=Path('private/texture-remaster'))
    args=ap.parse_args();report=scan(args.game,args.root)
    (args.root/'embedded-scan.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('archives','candidates','unclassified_prefix_counts')},indent=2))
    print('Candidates:',len(report['candidates']))


if __name__=='__main__':main()
