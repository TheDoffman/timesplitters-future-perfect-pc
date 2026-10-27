"""Verify embedded texture candidates against container tables and US loader code.

Keep one compact report and an optional contact sheet. Never extract full assets
or promote candidates into the production catalog implicitly.
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
from collections import Counter, defaultdict
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
from PIL import Image, ImageDraw

spec=importlib.util.spec_from_file_location('embedded_scan',Path(__file__).with_name('texture-embedded-scan.py'))
scanner=importlib.util.module_from_spec(spec);spec.loader.exec_module(scanner)
cat=scanner.catalog_tool


def table_references(data):
    if len(data)<4:return {}
    table=struct.unpack_from('<I',data)[0]
    if table<4 or table%4 or table+16>len(data):return {}
    refs={}
    for pos in range(table,len(data)-15,16):
        ptr,_,_,flags=struct.unpack_from('<4I',data,pos)
        if ptr==0xffffffff:return refs
        if flags&0x10000000 and ptr not in (0,0x40000000):
            if ptr+128>len(data):return {}
            refs[ptr]=pos
    return {}  # A missing terminator is not a valid table.


def verify_code(path):
    mapping=cat.format_tool.verify_mapping(path)
    data=path.read_bytes();base=struct.unpack_from('<I',data,0x104)[0]
    count,table=struct.unpack_from('<II',data,0x11c)
    sections=[struct.unpack_from('<5I',data,table-base+i*56) for i in range(count)]
    def read(va,n):
        for _,start,_,raw,size in sections:
            if start<=va and va+n<=start+size:return data[raw+va-start:raw+va-start+n]
        raise ValueError('Code address out of bounds')
    # Texture pointer at record+0, embedded flag at +12, stride16, -1 end.
    expected={0x8e3f9:'f7410c00000010',0x8e406:'03c6',
              0x8e40e:'8b421083c21083f8ff',
              0x18a3f:'e84cf9ffff'}
    # Validate the call separately, deriving its target instead of fixing a
    # manually transcribed relative displacement.
    for va,hex_bytes in expected.items():
        want=bytes.fromhex(hex_bytes)
        if read(va,len(want))!=want:raise ValueError(f'Embedded loader proof changed at {va:x}')
    call=read(0x89e5f,5)
    if call[0]!=0xe8 or 0x89e64+struct.unpack_from('<i',call,1)[0]!=0x18a30:
        raise ValueError('Embedded loader call target changed')
    return dict(xbe_sha256=mapping['xbe_sha256'],relocation_va='0x8e3e0',
                texture_call_va='0x89e5f',wrapper_va='0x18a30',loader_va='0x18390',
                record_stride=16,embedded_flag='0x10000000',terminator='0xffffffff')


def decode(row,raw):
    kind,w,h=row['archive_format'],row['width'],row['height']
    if kind in cat.format_tool.UNCOMPRESSED:return cat.format_tool.decode(kind,w,h,raw)
    fourcc={0:b'DXT1',1:b'DXT3',4:b'DXT5'}[kind]
    return Image.open(io.BytesIO(cat.dds_header(w,h,fourcc)+raw)).convert('RGBA')


def verified_payload(data,row):
    """Recheck a scan record against live bounded container bytes."""
    pos=row['relative_offset'];base=row['base_bytes']
    if base is None or 'key' not in row:
        raise ValueError('Candidate format requires a verified decoder')
    header_info=scanner.inspect_header(data,pos,len(data))
    if not header_info or header_info['status']!='candidate':
        raise ValueError('Candidate header is invalid or truncated')
    for field in ('width','height','archive_format','base_bytes'):
        if header_info[field]!=row[field]:raise ValueError('Candidate header metadata changed')
    refs=table_references(data)
    if pos not in refs:raise ValueError('Candidate has no verified container table reference')
    header=data[pos:pos+128];raw=data[pos+128:pos+128+base]
    if hashlib.sha256(header).hexdigest()!=row['header_sha256'] or hashlib.sha256(raw).hexdigest()!=row['base_sha256']:
        raise ValueError('Candidate changed since scan')
    identity=cat.classify(row['archive_format'],row['width'],row['height'],raw)
    if identity.get('key')!=row['key']:raise ValueError('Candidate identity changed')
    return header,raw,refs[pos]


def build(game,root,preview=False):
    proof=verify_code(game/'default.xbe')
    report=json.loads((root/'embedded-scan.json').read_text(encoding='utf-8'))
    known={x['key'] for x in json.loads((root/'catalog.json').read_text(encoding='utf-8'))['textures'] if 'key' in x}
    groups=defaultdict(list)
    for row in report['candidates']:groups[(row['archive'],row['entry_offset'],row['entry_length'])].append(row)
    counts=Counter();unique={};samples=defaultdict(list)
    for (archive,offset,length),rows in groups.items():
        with (game/archive).open('rb') as f:f.seek(offset);data=f.read(length)
        if len(data)!=length:raise ValueError('Container truncated')
        for row in rows:
            row=dict(row);header,raw,reference=verified_payload(data,row)
            counts['table_matched_occurrences']+=1
            if row['key'] not in unique:
                im=decode(row,raw)
                row.update(table_reference=reference,decoded_rgba_sha256=hashlib.sha256(im.tobytes()).hexdigest(),
                           alpha_range=im.getchannel('A').getextrema(),
                           status='decoded_and_container_referenced; catalog integration pending',
                           runtime_validation='pending scene validation',already_cataloged=row['key'] in known)
                unique[row['key']]=row
                if preview and len(samples[row['archive_format']])<6:samples[row['archive_format']].append((row,im))
    if preview:
        selected=[item for kind in sorted(samples) for item in samples[kind]]
        sheet=Image.new('RGB',(1152,((len(selected)+5)//6)*222),(45,45,45));draw=ImageDraw.Draw(sheet)
        for i,(row,im) in enumerate(selected):
            im.thumbnail((192,192));x=i%6*192;y=i//6*222;sheet.paste(im,(x,y),im)
            draw.text((x+3,y+194),row['key'][:12]+' fmt'+str(row['archive_format']),fill='white')
        sheet.save(root/'review/embedded-candidates.jpg',quality=90)
    return dict(schema=1,complete=False,executable_proof=proof,counts=dict(counts),
                unique_decoded_keys=len(unique),new_keys_pending_catalog=sum(k not in known for k in unique),
                potential_total_unique_keys=len(known|unique.keys()),unique=list(unique.values()),
                next_step=('Integrate verified embedded source references into catalog, worklist and inventory without double counting existing keys.'
                           if any(k not in known for k in unique) else 'Author embedded assets and validate their scenes; discovery of other formats remains open.'),
                limitations=report['limitations']+['Container references do not prove every asset is displayed; usage and scene validation remain pending.'])


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game',type=Path,default=Path('private/game'))
    ap.add_argument('--root',type=Path,default=Path('private/texture-remaster'))
    ap.add_argument('--preview',action='store_true')
    a=ap.parse_args();report=build(a.game,a.root,a.preview)
    (a.root/'embedded-decode-research.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='unique'},indent=2))


if __name__=='__main__':main()
