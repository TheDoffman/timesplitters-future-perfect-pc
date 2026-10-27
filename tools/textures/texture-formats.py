"""Verify the US game's format switch and preview uncompressed texels.

Read-only game access. Decoding does not enable runtime replacements or count
these assets as finished. Only small, explicitly bounded previews are exported.
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
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
from PIL import Image, ImageDraw

XBE_SHA256 = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
ARCHIVE_TO_XBOX = (12, 14, 6, 5, 15, 25, 0, 26)
UNCOMPRESSED = {2: ('A8R8G8B8', 4), 5: ('A8', 1), 6: ('L8', 1), 7: ('A8L8', 2)}


def verify_mapping(path):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != XBE_SHA256:
        raise ValueError('Format proof requires the pinned US executable')
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, table = struct.unpack_from('<II', data, 0x11c)
    sections = [struct.unpack_from('<5I', data, table - base + i * 56) for i in range(count)]

    def read(va, size):
        for _, start, _, raw, raw_size in sections:
            if start <= va and va + size <= start + raw_size:
                return data[raw + va - start:raw + va - start + size]
        raise ValueError('Address outside executable sections')

    # mov ebx,[edi+0x14]; cmp ebx,7; ... jmp [ebx*4+0x185b0]
    if read(0x183b4, 6) != bytes.fromhex('8b5f1483fb07') or read(0x183d4, 7) != bytes.fromhex('ff249db0850100'):
        raise ValueError('Texture format dispatch changed')
    cases = []
    for kind, target in enumerate(struct.unpack('<8I', read(0x185b0, 32))):
        code = read(target, 8)
        if code[:4] != bytes.fromhex('c7442408'):
            raise ValueError('Unexpected format case instruction')
        value = struct.unpack_from('<I', code, 4)[0]
        if value != ARCHIVE_TO_XBOX[kind]:
            raise ValueError('Texture format mapping changed')
        cases.append(dict(archive_format=kind, xbox_format=value, instruction_va=hex(target)))
    return dict(xbe_sha256=XBE_SHA256, loader_va='0x18390', switch_table_va='0x185b0',
                header_format_offset=20, payload_offset=128, cases=cases)


def unswizzle(data, width, height, bpp):
    if any(v <= 0 or v > 4096 or v & (v - 1) for v in (width, height)):
        raise ValueError('Expected power-of-two dimensions up to 4096')
    if bpp not in (1, 2, 4) or len(data) < width * height * bpp:
        raise ValueError('Invalid or truncated texels')
    xmask, ymask, bit, dest = 0, 0, 1, 1
    while bit < width or bit < height:
        if bit < width:
            xmask |= dest; dest <<= 1
        if bit < height:
            ymask |= dest; dest <<= 1
        bit <<= 1

    def deposit(value, mask):
        result, bit = 0, 1
        while mask:
            low = mask & -mask
            if value & bit:
                result |= low
            mask &= mask - 1
            bit <<= 1
        return result

    xs = [deposit(x, xmask) for x in range(width)]
    out = bytearray(width * height * bpp)
    for y in range(height):
        row = deposit(y, ymask)
        for x in range(width):
            source = (row | xs[x]) * bpp
            target = (y * width + x) * bpp
            out[target:target+bpp] = data[source:source+bpp]
    return bytes(out)


def decode(kind, width, height, raw):
    if kind not in UNCOMPRESSED:
        raise ValueError('Not a verified uncompressed archive format')
    _, bpp = UNCOMPRESSED[kind]
    data = unswizzle(raw, width, height, bpp)
    if kind == 2:
        return Image.frombytes('RGBA', (width, height), data, 'raw', 'BGRA')
    if kind == 5:
        image = Image.new('RGBA', (width, height), 'white')
        image.putalpha(Image.frombytes('L', (width, height), data))
        return image
    if kind == 6:
        return Image.frombytes('L', (width, height), data).convert('RGBA')
    lum = Image.frombytes('L', (width, height), data[0::2])
    alpha = Image.frombytes('L', (width, height), data[1::2])
    return Image.merge('RGBA', (lum, lum, lum, alpha))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=Path('private/game'))
    parser.add_argument('--root', type=Path, default=Path('private/texture-remaster'))
    parser.add_argument('--samples-per-format', type=int, default=4)
    args = parser.parse_args()
    if not 1 <= args.samples_per_format <= 8:
        parser.error('Use 1 to 8 samples per format to limit temporary output')
    proof = verify_mapping(args.game / 'default.xbe')
    rows = json.loads((args.root / 'catalog.json').read_text())['textures']
    spec = importlib.util.spec_from_file_location('catalog', Path(__file__).with_name('texture-catalog.py'))
    catalog = importlib.util.module_from_spec(spec); spec.loader.exec_module(catalog)
    report = dict(schema=1, binary_proof=proof, this_tool_enables_runtime_support=False,
                  runtime_scene_validation='pending',
                  uncompressed_payloads=sum(t['archive_format'] in UNCOMPRESSED for t in rows),
                  decoded_payloads=[], samples=[])
    # Verify and decode every payload, retaining only hashes and metadata.
    # Candidate identities are not promoted into the runtime-supported catalog.
    for texture in rows:
        kind = texture['archive_format']
        if kind not in UNCOMPRESSED:
            continue
        source = texture['sources'][0]
        with (args.game / source['archive']).open('rb') as f:
            f.seek(source['offset']); data = f.read(source['length'])
        if hashlib.sha256(data).hexdigest() != texture['sha256']:
            raise ValueError('Original texture changed')
        image = decode(kind, texture['width'], texture['height'], data[128:])
        size = texture['width'] * texture['height'] * UNCOMPRESSED[kind][1]
        report['decoded_payloads'].append(dict(sha256=texture['sha256'], archive_format=kind,
            xbox_format=ARCHIVE_TO_XBOX[kind], decoded_rgba_sha256=hashlib.sha256(image.tobytes()).hexdigest(),
            candidate_key=catalog.texture_key(ARCHIVE_TO_XBOX[kind], texture['width'], texture['height'], data[128:128+size]),
            alpha_extrema=image.getchannel('A').getextrema(), original_size=[texture['width'], texture['height']],
            runtime_identity_verified=False))
    report['unique_candidate_keys'] = len({t['candidate_key'] for t in report['decoded_payloads']})
    sheet = Image.new('RGB', (args.samples_per_format * 256, 4 * 288), (30, 30, 30))
    draw = ImageDraw.Draw(sheet)
    for y, kind in enumerate(UNCOMPRESSED):
        candidates = sorted((t for t in rows if t['archive_format'] == kind),
                            key=lambda t: (-len(t['sources']), t['sha256']))
        for x, texture in enumerate(candidates[:args.samples_per_format]):
            source = texture['sources'][0]
            with (args.game / source['archive']).open('rb') as f:
                f.seek(source['offset']); data = f.read(source['length'])
            if hashlib.sha256(data).hexdigest() != texture['sha256']:
                raise ValueError('Original texture changed')
            image = decode(kind, texture['width'], texture['height'], data[128:])
            # Review composite only. The original alpha is measured before compositing.
            alpha = image.getchannel('A').getextrema()
            image.thumbnail((256, 256))
            back = Image.new('RGBA', image.size, (70, 70, 70, 255))
            back.alpha_composite(image)
            sheet.paste(back.convert('RGB'), (x*256, y*288+25))
            draw.text((x*256+3, y*288+3), f'{kind} {UNCOMPRESSED[kind][0]} {texture["sha256"][:8]}', fill='white')
            size = texture['width'] * texture['height'] * UNCOMPRESSED[kind][1]
            report['samples'].append(dict(sha256=texture['sha256'], archive_format=kind,
                candidate_key=catalog.texture_key(ARCHIVE_TO_XBOX[kind], texture['width'], texture['height'], data[128:128+size]),
                original_size=[texture['width'], texture['height']], alpha_extrema=alpha,
                source=source, runtime_identity_verified=False))
    destination = args.root / 'review'; destination.mkdir(exist_ok=True)
    sheet.save(destination / 'uncompressed-formats.jpg', quality=92)
    (args.root / 'format-research.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(verified_mapping=ARCHIVE_TO_XBOX, samples=len(report['samples']),
                         decoded_payloads=len(report['decoded_payloads']),
                         unique_candidate_keys=report['unique_candidate_keys'], runtime_scene_validation='pending')))


if __name__ == '__main__':
    main()
