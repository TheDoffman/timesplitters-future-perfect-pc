"""Read-only P5CK texture inventory and DXT source export for the Xbox US build.

Archive layout reference: https://github.com/OpenRadical/tspak (tspak.h).
The 128-byte texture header detection is verified against this local game dump;
unrecognized assets remain explicitly unclassified, never silently converted.
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
import collections
import hashlib
import io
import importlib.util
import json
from pathlib import Path
import struct

_spec = importlib.util.spec_from_file_location('texture_formats', Path(__file__).with_name('texture-formats.py'))
format_tool = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(format_tool)


def classify(kind, w, h, raw):
    formats = {0:(12,8), 1:(14,16), 4:(15,16)}
    if kind in formats:
        gpu, block = formats[kind]
        n = ((w+3)//4)*((h+3)//4)*block
        validation = '104 original DXT payload matches; scene coverage remains partial'
    elif kind in format_tool.UNCOMPRESSED:
        gpu = format_tool.ARCHIVE_TO_XBOX[kind]
        n = w*h*format_tool.UNCOMPRESSED[kind][1]
        validation = 'binary loader mapping and offline decode verified; runtime scene check pending'
    else:
        return {}
    if len(raw) < n:
        return {}
    return dict(status='source_only', xbox_format=gpu,
                key=texture_key(gpu, w, h, raw[:n]), runtime_validation=validation)


def texture_key(fmt, width, height, data):
    h = 0xcbf29ce484222325
    for b in struct.pack('<III', fmt, width, height) + data:
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return f'{h:016x}'


def dds_header(w, h, fourcc=b'DXT1', levels=1):
    words = [124, 0xA1007, h, w, ((w+3)//4)*((h+3)//4)*(8 if fourcc == b'DXT1' else 16), 0, levels]
    words += [0]*11 + [32, 4, int.from_bytes(fourcc, 'little'), 0, 0, 0, 0, 0]
    words += [0x401008 if levels > 1 else 0x1000, 0, 0, 0, 0]
    return b'DDS ' + struct.pack('<31I', *words)


def entries(path):
    size = path.stat().st_size
    with path.open('rb') as f:
        header = f.read(12)
        if len(header) != 12 or header[:4] != b'P5CK':
            raise ValueError(f'Not P5CK: {path}')
        offset, length = struct.unpack_from('<II', header, 4)
        if length % 16 or offset + length > size:
            raise ValueError(f'Invalid archive index: {path}')
        f.seek(offset)
        table = f.read(length)
        for crc, pos, length, compressed in struct.iter_unpack('<4I', table):
            if pos + (compressed or length) > size:
                raise ValueError(f'Out-of-bounds entry: {path}, {crc:x}')
            yield f, crc, pos, length, compressed


def merge_embedded(game,root,catalog):
    scan_path=root/'embedded-scan.json'
    if not scan_path.exists():
        previous=root/'catalog.json'
        if previous.exists() and any(s.get('embedded') for t in json.loads(previous.read_text(encoding='utf-8'))['textures'] for s in t['sources']):
            raise ValueError('Embedded scan is missing; refusing to drop previously cataloged sources')
        return dict(status='not scanned',occurrences=0)
    spec=importlib.util.spec_from_file_location('embedded_verify',Path(__file__).with_name('texture-embedded-verify.py'))
    verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify)
    proof=verify.verify_code(game/'default.xbe')
    scan=json.loads(scan_path.read_text(encoding='utf-8'))
    # The scan covers all archives: changed, added or removed archives require a
    # new scan rather than silently retaining incomplete embedded coverage.
    live={p.relative_to(game).as_posix():p for p in (game/'pak').rglob('*.pak')}
    if set(live)!={a['archive'] for a in scan['archives']}:
        raise ValueError('Archive set changed; rerun embedded scan')
    for a in scan['archives']:
        digest=hashlib.sha256()
        with live[a['archive']].open('rb') as f:
            while chunk:=f.read(1024*1024):digest.update(chunk)
        if digest.hexdigest()!=a['sha256']:raise ValueError('Archive changed; rerun embedded scan: '+a['archive'])
    grouped=collections.defaultdict(list)
    for row in scan['candidates']:
        grouped[row['archive']].append(row)
    occurrences=0
    for rel,rows in grouped.items():
        index={(f'{crc:08x}',pos,length):compressed for _,crc,pos,length,compressed in entries(live[rel])}
        containers=collections.defaultdict(list)
        for row in rows:containers[(row['entry_crc'],row['entry_offset'],row['entry_length'])].append(row)
        with live[rel].open('rb') as f:
            for parent,candidates in containers.items():
                if parent not in index or index[parent]:raise ValueError('Embedded container missing or compressed')
                f.seek(parent[1]);data=f.read(parent[2])
                for row in candidates:
                    header,raw,reference=verify.verified_payload(data,row)
                    digest=hashlib.sha256(header+raw).hexdigest()
                    source=dict(archive=rel,crc=parent[0],offset=parent[1]+row['relative_offset'],
                                length=128+len(raw),embedded=True,
                                container_offset=parent[1],container_length=parent[2],
                                relative_offset=row['relative_offset'],table_reference=reference,
                                payload_scope='header and base level only')
                    if digest not in catalog:
                        entry=dict(width=row['width'],height=row['height'],archive_format=row['archive_format'],
                                   bytes=len(raw),sha256=digest,sources=[])
                        entry.update(classify(row['archive_format'],row['width'],row['height'],raw))
                        entry['runtime_validation']='Embedded identity and container reference verified offline; scene validation pending'
                        catalog[digest]=entry
                    if source not in catalog[digest]['sources']:catalog[digest]['sources'].append(source)
                    occurrences+=1
    return dict(status='integrated',occurrences=occurrences,executable_proof=proof,
                source_report='embedded-scan.json',limitations=scan['limitations'])


def merge_xbe_sections(game, catalog):
    spec=importlib.util.spec_from_file_location('xbe_sections',Path(__file__).with_name('texture-xbe-sections.py'))
    verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify)
    sections,proof=verify.verified_sections(game/'default.xbe')
    for section in sections:
        payload=section['payload'];digest=section['sha256']
        source=dict(archive='default.xbe',source_kind='xbe_section',section=section['name'],
                    section_va=section['virtual_address'],offset=section['offset'],length=section['length'])
        if digest not in catalog:
            entry=dict(width=512,height=512,archive_format=0,bytes=len(payload)-128,
                       sha256=digest,sources=[])
            entry.update(classify(0,512,512,payload[128:]))
            entry['runtime_validation']='Named XBE section and load/copy/wrapper chain verified offline; scene check pending'
            catalog[digest]=entry
        if source not in catalog[digest]['sources']:catalog[digest]['sources'].append(source)
    return dict(status='integrated',occurrences=len(sections),executable_proof=proof)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path, default=Path('private/game'))
    ap.add_argument('--output', type=Path, default=Path('private/texture-remaster'))
    ap.add_argument('--export-archive', help='Export DXT textures from archive paths containing this string')
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    mapping_proof = format_tool.verify_mapping(args.game/'default.xbe')
    catalog = {}
    counts = collections.Counter()
    from PIL import Image
    for path in sorted((args.game/'pak').rglob('*.pak')):
        rel = path.relative_to(args.game).as_posix()
        counts['archives'] += 1
        for f, crc, pos, length, compressed in entries(path):
            counts['entries'] += 1
            if compressed:
                counts['compressed_unclassified'] += 1
                continue
            f.seek(pos)
            header = f.read(min(length, 128))
            if len(header) != 128:
                continue
            w, h, w2, h2, unknown, kind, sentinel = struct.unpack_from('<7I', header)
            if not (w and h and w <= 4096 and h <= 4096 and w & (w-1) == 0 and h & (h-1) == 0
                    and w == w2 and h == h2 and sentinel == 0xffffffff):
                continue
            counts['texture_occurrences'] += 1
            raw = f.read(length-128)
            digest = hashlib.sha256(header+raw).hexdigest()
            source = {'archive': rel, 'crc': f'{crc:08x}', 'offset': pos, 'length': length}
            if digest not in catalog:
                entry = catalog[digest] = dict(width=w, height=h, archive_format=kind,
                    bytes=len(raw), sha256=digest, sources=[], status='unclassified_format')
                entry.update(classify(kind, w, h, raw))
            entry = catalog[digest]
            entry['sources'].append(source)
            if args.export_archive and args.export_archive in rel and 'key' in entry:
                dest = args.output/'originals'/f"{entry['key']}.png"
                dest.parent.mkdir(exist_ok=True)
                if not dest.exists():
                    if kind in format_tool.UNCOMPRESSED:
                        format_tool.decode(kind,w,h,raw).save(dest)
                    else:
                        fourcc = {12:b'DXT1',14:b'DXT3',15:b'DXT5'}[entry['xbox_format']]
                        n = ((w+3)//4)*((h+3)//4)*(8 if fourcc==b'DXT1' else 16)
                        Image.open(io.BytesIO(dds_header(w,h,fourcc)+raw[:n])).save(dest)
                entry['original_png'] = dest.relative_to(args.output).as_posix()
            if 'key' in entry and (args.output/'originals'/f"{entry['key']}.png").is_file():
                entry['original_png'] = f"originals/{entry['key']}.png"
    embedded=merge_embedded(args.game,args.output,catalog)
    xbe_sections=merge_xbe_sections(args.game,catalog)
    counts['top_level_texture_occurrences']=counts['texture_occurrences']
    counts['embedded_texture_occurrences']=embedded['occurrences']
    counts['texture_occurrences']+=embedded['occurrences']
    counts['xbe_section_texture_occurrences']=xbe_sections['occurrences']
    counts['texture_occurrences']+=xbe_sections['occurrences']
    counts['unique_texture_payloads'] = len(catalog)
    counts['unique_archive_ids'] = len({s['crc'] for e in catalog.values() for s in e['sources'] if 'crc' in s})
    result = dict(schema=3, scope='Top-level and verified nested P5CK textures plus five verified XBE error-screen sections; other encodings and procedural assets remain open',
                  counts=dict(counts), formats=dict(collections.Counter(e['archive_format'] for e in catalog.values())),
                  format_mapping_proof=mapping_proof, embedded_scan=embedded, xbe_sections=xbe_sections, textures=list(catalog.values()))
    (args.output/'catalog.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k != 'textures'}, indent=2))


if __name__ == '__main__':
    main()
