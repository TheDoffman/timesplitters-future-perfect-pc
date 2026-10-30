"""Convert the runtime's frame dumps to PNG, so they can be looked at.

RECOMP_HLE_D3D8_DUMP writes 24-bit bottom-up BMPs, which most viewers open but
few tools ingest, and this machine has no Pillow. Rather than add a dependency
to look at a picture, write the PNG by hand: it is a signature, three chunks
and zlib, all of which are in the standard library.

Looking at the frames matters more than it sounds. Dino Crisis 3 was called
frozen on the strength of a draw counter that stopped moving, while it was in
fact playing a full-screen sequence; the counters could not tell those apart
and one glance at the images could.

    py -3 scripts/bmp_to_png.py games/_pipeline/_matrix/frames/dinocrisis3*.bmp
    py -3 scripts/bmp_to_png.py --scale 2 shot.bmp -o /tmp/out
"""

import argparse
import glob
import struct
import sys
import zlib
from pathlib import Path


def read_bmp(path):
    """(width, height, rows) with rows top-down, each a bytes of RGB triples."""
    data = Path(path).read_bytes()
    if data[:2] != b"BM":
        raise ValueError("not a BMP")
    pixel_off = struct.unpack("<I", data[10:14])[0]
    width, height = struct.unpack("<ii", data[18:26])
    bpp = struct.unpack("<H", data[28:30])[0]
    if bpp != 24:
        raise ValueError("only 24-bit BMPs, got %d" % bpp)
    bottom_up = height > 0
    height = abs(height)
    stride = (width * 3 + 3) & ~3          # rows are padded to 4 bytes
    rows = []
    for y in range(height):
        start = pixel_off + y * stride
        row = data[start:start + width * 3]
        if len(row) < width * 3:
            row = row + b"\x00" * (width * 3 - len(row))
        rows.append(_bgr_to_rgb(row))   # BMP stores BGR, PNG wants RGB
    if bottom_up:
        rows.reverse()
    return width, height, rows


def _bgr_to_rgb(row):
    out = bytearray(len(row))
    out[0::3] = row[2::3]
    out[1::3] = row[1::3]
    out[2::3] = row[0::3]
    return bytes(out)


def write_png(path, width, height, rows, scale=1):
    if scale > 1:
        wide = []
        for row in rows:
            r = bytearray()
            for x in range(width):
                r += row[x * 3:x * 3 + 3] * scale
            wide.append(bytes(r))
        rows = [r for r in wide for _ in range(scale)]
        width *= scale
        height *= scale

    raw = b"".join(b"\x00" + r for r in rows)   # filter byte 0 per scanline

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6))
    png += chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bmp", nargs="+", help="input BMPs (globs are expanded)")
    ap.add_argument("-o", "--out-dir", help="where to write (default: beside)")
    ap.add_argument("--scale", type=int, default=1,
                    help="nearest-neighbour upscale")
    args = ap.parse_args(argv)

    paths = []
    for pattern in args.bmp:
        hits = sorted(glob.glob(pattern))
        paths.extend(hits if hits else [pattern])

    for p in paths:
        src = Path(p)
        if not src.is_file():
            print("missing: %s" % src, file=sys.stderr)
            continue
        try:
            w, h, rows = read_bmp(src)
        except (OSError, ValueError, struct.error) as exc:
            print("%s: %s" % (src.name, exc), file=sys.stderr)
            continue
        dest_dir = Path(args.out_dir) if args.out_dir else src.parent
        dest_dir.mkdir(parents=True, exist_ok=True)
        dest = dest_dir / (src.stem + ".png")
        write_png(dest, w, h, rows, args.scale)
        print("%s -> %s  (%dx%d)" % (src.name, dest, w, h))
    return 0


if __name__ == "__main__":
    sys.exit(main())
