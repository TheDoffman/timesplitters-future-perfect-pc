"""Inventory and scan non-PAK files without extraction or catalog promotion.

Negative results exclude only the tested signatures. Executable-generated
textures, decoded video surfaces and other encodings remain outside that proof.
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
import struct


spec = importlib.util.spec_from_file_location('embedded_scan', Path(__file__).with_name('texture-embedded-scan.py'))
embedded = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embedded)

SIGNATURES = {
    'dds': b'DDS ', 'png': b'\x89PNG\r\n\x1a\n',
    'jpeg': b'\xff\xd8\xff', 'xpr0': b'XPR0', 'xpr1': b'XPR1',
}


def dds_candidate(data, offset):
    """Bounded DDS base-level check; not a decoder or runtime identity proof."""
    if offset + 128 > len(data):
        return {'status': 'truncated_header'}
    size, flags, height, width = struct.unpack_from('<4I', data, offset + 4)
    pf_size = struct.unpack_from('<I', data, offset + 76)[0]
    if size != 124 or pf_size != 32 or not (0 < width <= 16384 and 0 < height <= 16384):
        return {'status': 'invalid_header'}
    fourcc = bytes(data[offset+84:offset+88])
    bpp = struct.unpack_from('<I', data, offset+88)[0]
    if fourcc in (b'DXT1', b'DXT3', b'DXT5'):
        required = ((width+3)//4)*((height+3)//4)*(8 if fourcc == b'DXT1' else 16)
    elif fourcc == bytes(4) and bpp in (8, 16, 24, 32):
        required = width*height*(bpp//8)
    else:
        return dict(status='unverified_encoding', width=width, height=height,
                    fourcc_hex=fourcc.hex(), bits_per_pixel=bpp)
    return dict(status='bounded_base_candidate' if offset+128+required <= len(data)
                else 'truncated_base', width=width, height=height,
                fourcc_hex=fourcc.hex(), bits_per_pixel=bpp, base_bytes=required)


def scan_bytes(data):
    headers = [dict(offset=offset, **row) for offset, row in embedded.find_headers(data, 0, len(data))]
    signatures = {}
    for name, magic in SIGNATURES.items():
        count = 0
        samples = []
        offset = data.find(magic)
        while offset >= 0:
            count += 1
            if len(samples) < 32:
                row = dict(offset=offset, status='signature_only_not_verified')
                if name == 'dds':
                    row.update(dds_candidate(data, offset))
                samples.append(row)
            offset = data.find(magic, offset+1)
        if count:
            signatures[name] = dict(count=count, samples=samples, samples_truncated=count > len(samples))
    return dict(verified_header_shape_hits=headers, signatures=signatures)


def scan(game):
    rows = []
    totals = Counter()
    extensions = Counter()
    for path in sorted(game.rglob('*')):
        if not path.is_file() or path.suffix.lower() == '.pak':
            continue
        rel = path.relative_to(game).as_posix()
        size = path.stat().st_size
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            while chunk := stream.read(1024*1024):
                digest.update(chunk)
            if size:
                with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
                    result = scan_bytes(data)
                    prefix = bytes(data[:32]).hex()
            else:
                result = scan_bytes(b'')
                prefix = ''
        suffix = path.suffix.lower()
        role = {'.json': 'analysis sidecar; no catalog promotion',
                '.xmv': 'movie extension; compressed frames not decoded',
                '.xbe': 'Xbox executable extension; procedural images not analyzed'}.get(suffix, 'unknown; investigation required')
        rows.append(dict(path=rel, bytes=size, sha256=digest.hexdigest(),
                         prefix_hex=prefix, role=role, **result))
        totals['files_scanned'] += 1
        totals['bytes_scanned'] += size
        totals['verified_header_shape_hits'] += len(result['verified_header_shape_hits'])
        totals['signature_hits'] += sum(s['count'] for s in result['signatures'].values())
        extensions[suffix] += 1
    return dict(schema=1, complete=False, catalog_changes=0, totals=dict(totals),
                extensions=dict(extensions), files=rows,
                method='Read-only SHA256 and every-byte signature search in every non-PAK file, including analysis sidecars. No payload extraction.',
                limitations=['A header-shaped match is not proof of texture use or runtime identity.',
                             'DDS checks bound only the base level; PNG/JPEG/XPR hits are signatures only.',
                             'Other encodings, compressed movie frames and procedural textures are not excluded.',
                             'No files are excluded from overhaul scope or marked complete by this scan.'])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path, default=Path('private/game'))
    ap.add_argument('--output', type=Path, default=Path('private/texture-remaster/nonpak-scan.json'))
    args = ap.parse_args()
    report = scan(args.game)
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k != 'files'}, indent=2))
    for row in report['files']:
        print(row['path'], 'headers', len(row['verified_header_shape_hits']),
              'signatures', {k:v['count'] for k,v in row['signatures'].items()})


if __name__ == '__main__':
    main()
