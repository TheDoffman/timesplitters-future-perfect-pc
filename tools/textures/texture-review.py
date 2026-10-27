"""Create bounded offline comparisons from originals and actual compiled DDS.

Diagnostic sheets only: never edits artwork or approves it automatically.
Preserves aspect ratios and limits pages to four rows. Metrics describe
brightness and boundary differences, not UV correctness or seamless tiling.
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
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def metrics(image):
    pixels = np.asarray(image.convert('RGBA'), dtype=np.float64)
    rgb = pixels[:, :, :3]
    luminance = rgb @ np.array([.2126, .7152, .0722])
    return dict(
        mean_luminance=float(luminance.mean()),
        alpha_range=[int(pixels[:, :, 3].min()), int(pixels[:, :, 3].max())],
        left_right_edge_mae=float(np.abs(rgb[:, 0] - rgb[:, -1]).mean()),
        top_bottom_edge_mae=float(np.abs(rgb[0] - rgb[-1]).mean()),
        right_edge_mean_luminance=float(luminance[:, -1].mean()),
    )


def sheets(records, repeat=False):
    width, row_height = 792, 416
    sheet = Image.new('RGB', (width, len(records) * row_height + 26), (24, 24, 24))
    draw = ImageDraw.Draw(sheet)
    heading = 'Original left | Compiled DDS right; aspect preserved'
    if repeat:
        heading += ' / 2x2 repetition, not proof of scene adjacency'
    draw.text((8, 7), heading, fill='white')
    for index, (key, original, compiled) in enumerate(records):
        y = 26 + index * row_height
        draw.text((8, y), key, fill='white')
        for column, image in enumerate((original, compiled)):
            # Enlarge low-resolution originals for comparison without stretching.
            limit = 192 if repeat else 384
            scale = min(limit / image.width, limit / image.height)
            size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
            resampling = Image.Resampling.NEAREST if column == 0 else Image.Resampling.LANCZOS
            tile = image.convert('RGB').resize(size, resampling)
            x = 8 + column * 396
            for tile_y in range(2 if repeat else 1):
                for tile_x in range(2 if repeat else 1):
                    sheet.paste(tile, (x + tile_x * size[0], y + 22 + tile_y * size[1]))
    return sheet


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('private/texture-remaster'))
    parser.add_argument('--name', required=True)
    parser.add_argument('keys', nargs='+')
    args = parser.parse_args()
    if len(args.keys) > 16:
        parser.error('Review at most 16 textures per batch to keep memory and output bounded')
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*', args.name):
        parser.error('Name must contain lowercase letters, digits and hyphens only')
    if len(set(args.keys)) != len(args.keys) or any(not re.fullmatch(r'[0-9a-f]{16}', key) for key in args.keys):
        parser.error('Keys must be distinct 16-character lowercase hexadecimal values')
    output = args.root / 'review'
    page_count = (len(args.keys) + 3) // 4
    destinations = [output / f'{args.name}-metrics.json']
    destinations += [output / f'{args.name}{suffix}-{page}.jpg'
                     for page in range(1, page_count + 1) for suffix in ('', '-repeat')]
    if any(path.exists() for path in destinations):
        parser.error('Review already exists; inspect it before explicitly replacing it')
    # Verify and measure every input before producing any output.
    records, rows = [], []
    for key in args.keys:
        with Image.open(args.root / 'originals' / f'{key}.png') as source:
            original = source.convert('RGBA')
        with Image.open(args.root / 'pack' / f'{key}.dds') as source:
            compiled = source.convert('RGBA')
        if original.width * compiled.height != original.height * compiled.width:
            raise ValueError(f'Aspect mismatch for {key}')
        before = metrics(original)
        after = metrics(compiled.resize(original.size, Image.Resampling.LANCZOS))
        change = None if before['mean_luminance'] == 0 else (
            after['mean_luminance'] / before['mean_luminance'] - 1) * 100
        rows.append(dict(key=key, original=before, compiled_at_original_scale=after,
                         compiled_native_alpha_range=list(compiled.getchannel('A').getextrema()),
                         mean_luminance_change_percent=change))
        records.append((key, original, compiled))
    output.mkdir(parents=True, exist_ok=True)
    for index in range(0, len(records), 4):
        page = index // 4 + 1
        for repeat in (False, True):
            suffix = '-repeat' if repeat else ''
            sheets(records[index:index + 4], repeat).save(output / f'{args.name}{suffix}-{page}.jpg', quality=92)
    report = dict(scope='Offline appearance diagnostics only; manual review and scene validation required',
                  visual_approval=False, rows=rows)
    destinations[0].write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(outputs=[str(p) for p in destinations], rows=rows), indent=2))


if __name__ == '__main__':
    main()
