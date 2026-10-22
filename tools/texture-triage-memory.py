"""Render verified original RGB/alpha inventory sheets to JSON on stdout.

Creates no raster files and changes no artwork, pack, classification or settings.
The caller displays each sheet's base64 JPEG in memory, then retains only its
hash and per-key review evidence. Use --keys to reproduce an earlier selection.
Classification and visual acceptance remain human review decisions.
"""
import sys

sys.dont_write_bytecode = True

import argparse
import base64
import hashlib
import importlib.util
import io
import json
from pathlib import Path

from PIL import Image, ImageDraw, __version__ as pillow_version


def digest(data):
    return hashlib.sha256(data).hexdigest()


def render(root, game, limit=128, keys=None):
    if not 1 <= limit <= 128:
        raise ValueError("Limit must be between 1 and 128")
    catalog_path = root / "catalog.json"
    catalog_data = catalog_path.read_bytes()
    entries = {}
    for entry in json.loads(catalog_data)["textures"]:
        if "key" in entry:
            entries.setdefault(entry["key"], entry)
    worklist = json.loads((root / "worklist.json").read_text(encoding="utf-8"))
    queue = {row["id"]: row for row in worklist["textures"]}
    if keys is None:
        classification = json.loads(
            (root / "visual-classification.json").read_text(encoding="utf-8")
        )
        reviewed = {row["key"] for row in classification["records"]}
        candidates = sorted(
            (row for row in queue.values()
             if row["id"] in entries and row["id"] not in reviewed),
            key=lambda row: (-row["occurrences"], row["id"]),
        )
        keys = [row["id"] for row in candidates[:limit]]
    if not keys or len(keys) > 128 or len(keys) != len(set(keys)):
        raise ValueError("Select between 1 and 128 distinct supported keys")
    if any(key not in entries or key not in queue for key in keys):
        raise ValueError("Selection contains an unknown catalog/worklist key")

    variants_path = Path(__file__).with_name("texture-variants.py")
    spec = importlib.util.spec_from_file_location("variants", variants_path)
    variants = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(variants)
    records, sheets = [], []
    for page, start in enumerate(range(0, len(keys), 16), 1):
        images = {channel: Image.new("RGB", (1024, 1000), (30, 30, 30))
                  for channel in ("rgb", "alpha")}
        drawers = {channel: ImageDraw.Draw(image)
                   for channel, image in images.items()}
        for offset, key in enumerate(keys[start:start + 16]):
            entry = entries[key]
            # original() verifies the catalog span's payload hash before decoding.
            original = variants.original(game, entry)
            bounds = list(original.getchannel("A").getextrema())
            index = start + offset
            x, y = offset % 4 * 256, offset // 4 * 250
            scale = min(244 / original.width, 205 / original.height)
            size = (max(1, round(original.width * scale)),
                    max(1, round(original.height * scale)))
            label = (f"{index} {key}\n{original.width}x{original.height} "
                     f"A:{bounds[0]}-{bounds[1]}")
            for channel in images:
                drawers[channel].text((x + 4, y + 3), label, fill="white")
                plane = original if channel == "rgb" else original.getchannel("A")
                tile = plane.convert("RGB").resize(size, Image.Resampling.NEAREST)
                images[channel].paste(tile, (x + 4, y + 39))
            records.append(dict(
                key=key, candidate_index=index, original_size=list(original.size),
                alpha_range=bounds, catalog_sha256=entry["sha256"],
                decoded_rgba_sha256=digest(original.tobytes()),
                occurrences=queue[key]["occurrences"], page=page,
            ))
        for channel, image in images.items():
            stream = io.BytesIO()
            image.save(stream, format="JPEG", quality=90)
            data = stream.getvalue()
            sheets.append(dict(
                page=page, channel=channel, size=list(image.size),
                sha256=digest(data), bytes=len(data),
                base64=base64.b64encode(data).decode("ascii"),
            ))
    return dict(
        schema=1, raster_storage="memory only; stdout payload is not a saved file",
        catalog_file_sha256=digest(catalog_data),
        renderer_sha256=digest(Path(__file__).read_bytes()),
        variants_sha256=digest(variants_path.read_bytes()),
        pillow_version=pillow_version, jpeg_quality=90,
        display_resampling="aspect-preserving nearest-neighbor thumbnails",
        keys=keys, records=records, sheets=sheets,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--limit", type=int, default=128)
    parser.add_argument("--keys", nargs="+")
    args = parser.parse_args()
    try:
        result = render(args.root, args.game, args.limit, args.keys)
    except (ValueError, KeyError, OSError) as exc:
        parser.exit(2, f"{exc}\n")
    print(json.dumps(result, separators=(",", ":")))


if __name__ == "__main__":
    main()
