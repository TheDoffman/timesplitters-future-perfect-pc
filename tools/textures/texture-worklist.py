"""Track complete catalog coverage without extracting duplicate game assets.

The plan is a size estimate, not artwork or a claim of a completed remaster.
Installed DDS files are checked against their metadata and original aspect.
Unsupported formats and missing art remain explicitly unfinished.
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
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import struct


TEMPORARY_BUDGET_BYTES = 256 * 1024 * 1024


def temporary_storage(root, files):
    """Count every output directory except authoritative art and the active pack.

    Root-level catalogs/provenance are retained records. Unknown subdirectories
    count conservatively so a new comparison or staging folder cannot disappear
    from the temporary budget merely because it has a different name.
    """
    folders = {}
    for path in files:
        parts = path.relative_to(root).parts
        if len(parts) < 2 or parts[0] in ('authored', 'pack'):
            continue
        row = folders.setdefault(parts[0], dict(files=0, logical_bytes=0, allocated_bytes=0))
        row['files'] += 1
        row['logical_bytes'] += path.stat().st_size
        row['allocated_bytes'] += allocation(path)
    total = sum(row['logical_bytes'] for row in folders.values())
    return dict(temporary_budget_bytes=TEMPORARY_BUDGET_BYTES,
                temporary_logical_bytes=total,
                temporary_allocated_bytes=sum(row['allocated_bytes'] for row in folders.values()),
                temporary_headroom_bytes=TEMPORARY_BUDGET_BYTES-total,
                temporary_budget_compliant=total < TEMPORARY_BUDGET_BYTES,
                temporary_directories=dict(sorted(folders.items())))


def dds_size(width, height, block):
    total, levels = 128, 0
    while True:
        total += width*height*4 if block is None else ((width + 3) // 4) * ((height + 3) // 4) * block
        levels += 1
        if width == height == 1:
            return total, levels
        width, height = max(1, width // 2), max(1, height // 2)


def allocation(path):
    if hasattr(ctypes, 'windll'):
        fn = ctypes.windll.kernel32.GetCompressedFileSizeW
        fn.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_ulong)]
        fn.restype = ctypes.c_ulong
        high = ctypes.c_ulong()
        low = fn(str(path.resolve()), ctypes.byref(high))
        if low == 0xffffffff and ctypes.windll.kernel32.GetLastError():
            raise OSError(f'Cannot measure allocation: {path}')
        return (high.value << 32) | low
    stat = path.stat()
    return getattr(stat, 'st_blocks', 0) * 512 or stat.st_size


def check_asset(dds, meta, original):
    data = dds.read_bytes()
    if len(data) < 128 or data[:4] != b'DDS ':
        raise ValueError(f'Invalid DDS: {dds.name}')
    height, width = struct.unpack_from('<II', data, 12)
    fourcc = data[84:88].decode('ascii')
    rgba8=struct.unpack_from('<7I',data,80)==(0x41,0,32,0xff0000,0xff00,0xff,0xff000000)
    if rgba8:
        fourcc='RGBA8'
    if fourcc not in ('DXT1', 'DXT3', 'DXT5','RGBA8'):
        raise ValueError(f'Unsupported DDS encoding: {dds.name}')
    if any(n <= 0 or n > 4096 or n & (n - 1) for n in (width, height)):
        raise ValueError(f'Invalid dimensions: {dds.name}')
    if width * original['height'] != height * original['width']:
        raise ValueError(f'Changed UV aspect: {dds.name}')
    if width < original['width'] or height < original['height']:
        raise ValueError(f'Replacement below original resolution: {dds.name}')
    size, mips = dds_size(width, height, None if rgba8 else (8 if fourcc == 'DXT1' else 16))
    if len(data) != size or struct.unpack_from('<I', data, 28)[0] != mips:
        raise ValueError(f'Incomplete mip chain: {dds.name}')
    if (meta['key'] != dds.stem or meta['sha256'] != hashlib.sha256(data).hexdigest()
            or meta['output_size'] != [width, height] or meta['bytes'] != size):
        raise ValueError(f'Metadata mismatch: {dds.name}')
    if original['xbox_format'] not in (0, 12) and fourcc == 'DXT1':
        raise ValueError(f'Alpha format lost: {dds.name}')
    return size


def build(root, originals_path=None):
    originals_path = originals_path or _layout_root / 'config/textures/texture-originals.json'
    policy = json.loads(originals_path.read_text(encoding='utf-8')) if originals_path.exists() else {'keys': []}
    retained = set(policy['keys'])
    if len(retained) != len(policy['keys']) or any(
            not isinstance(key, str) or len(key) != 16 or any(c not in '0123456789abcdef' for c in key)
            for key in retained):
        raise ValueError('Invalid or duplicate retained-original texture key')
    catalog = json.loads((root / 'catalog.json').read_text(encoding='utf-8'))
    textures = {}
    for texture in catalog['textures']:
        identity = texture.get('key', 'unsupported-' + texture['sha256'])
        if identity not in textures:
            textures[identity] = dict(texture, sources=list(texture['sources']), payload_count=1)
        else:
            textures[identity]['sources'] += texture['sources']
            textures[identity]['payload_count'] += 1
    pack = root / 'pack'
    unknown = {p.stem for p in pack.glob('*.dds')} - textures.keys()
    if unknown:
        raise ValueError(f'Unknown installed keys: {sorted(unknown)}')
    rows, projected = [], 0
    for identity, texture in sorted(textures.items()):
        sources = sorted({s['archive'] for s in texture['sources']})
        row = dict(id=identity, original_size=[texture['width'], texture['height']],
                   archive_format=texture['archive_format'], archives=sources,
                   catalog_payloads=texture['payload_count'], occurrences=len(texture['sources']))
        row['embedded_occurrences']=sum(bool(s.get('embedded')) for s in texture['sources'])
        if identity in retained:
            if (pack / f'{identity}.dds').exists() or (pack / f'{identity}.json').exists():
                raise ValueError(f'Retained-original texture has a replacement: {identity}')
            row.update(status='retained_original', target_size=None, estimated_dds_bytes=0,
                       reason=policy.get('reason', 'Original artwork requested'))
            rows.append(row)
            continue
        if row['embedded_occurrences']:
            row['embedded_runtime_validation']='pending scene validation'
        if 'key' not in texture:
            row.update(status='unsupported_format', target_size=None, estimated_dds_bytes=None)
        else:
            # Preserve original aspect; modest per-asset targets leave room for
            # reviewed 4K surface exceptions and presently unsupported assets.
            scale = max(1, min(4, 2048 // max(texture['width'], texture['height'])))
            width, height = texture['width'] * scale, texture['height'] * scale
            size, _ = dds_size(width, height, 8 if texture['xbox_format'] in (0, 12) else 16)
            row.update(status='needs_art', target_size=[width, height], estimated_dds_bytes=size)
            row['runtime_validation'] = texture.get('runtime_validation', 'not recorded')
            prompt_file = root / 'authored' / f'{identity}-prompt.json'
            if prompt_file.exists():
                art = json.loads(prompt_file.read_text(encoding='utf-8'))
                if (root / 'authored' / art['source']).is_file():
                    row.update(status='draft_requires_review', art_review=art.get('review'))
            dds = pack / f'{identity}.dds'
            if dds.exists():
                meta = json.loads(dds.with_suffix('.json').read_text(encoding='utf-8'))
                size = check_asset(dds, meta, texture)
                row.update(status='installed', target_size=meta['output_size'],
                           estimated_dds_bytes=size, source_size=meta['source_size'],
                           resampled=meta['resampled'], sha256=meta['sha256'])
            projected += size
        rows.append(row)
    files = [p for p in root.rglob('*') if p.is_file()]
    pack_bytes = sum(p.stat().st_size for p in pack.rglob('*') if p.is_file())
    if pack_bytes > 6144 * 1024 * 1024:
        raise ValueError('Active texture pack exceeds the 6 GiB limit')
    report = dict(schema=1, scope=catalog['scope'], complete=False,
                  counts=dict(Counter(r['status'] for r in rows)),
                  catalog_payloads=len(catalog['textures']),
                  unique_supported_keys=sum('key' in t for t in textures.values()),
                  pack_budget_bytes=6144 * 1024 * 1024, pack_bytes=pack_bytes,
                  planned_supported_dds_bytes=projected,
                  plan_excludes_unsupported_formats=True,
                  texture_workspace_logical_bytes=sum(p.stat().st_size for p in files),
                  texture_workspace_allocated_bytes=sum(allocation(p) for p in files),
                  disk_free_bytes=shutil.disk_usage(root).free,
                  status_note='Installed is not the same as visually validated in every level.',
                  textures=rows)
    report.update(temporary_storage(root, files))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('private/texture-remaster'))
    args = parser.parse_args()
    report = build(args.root)
    (args.root / 'worklist.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'textures'}, indent=2))
    if not report['temporary_budget_compliant']:
        parser.exit(2, 'Temporary texture output is at or above 256 MiB; recycle verified disposable outputs before production.\n')


if __name__ == '__main__':
    main()
