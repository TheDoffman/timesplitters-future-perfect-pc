#!/usr/bin/env python3
"""
Build a small, structurally valid XBE for testing the toolchain.

There is no legally shareable Xbox game binary, so the tools cannot be
exercised against real input in CI or by a new contributor. This produces a
synthetic XBE with a real header, section table, certificate, library version
table and kernel thunk table, so the parsing and survey paths can be tested
without game files.

It contains no game code and is not runnable. It is a fixture, not a title.

Usage:
    python3 scripts/make_test_xbe.py out.xbe
    python3 scripts/make_test_xbe.py out.xbe --title "Test Game" --xdk 5849 --ltcg
"""

import argparse
import struct
import sys
from pathlib import Path

BASE = 0x00010000
HEADER_SIZE = 0x1000

# XBE on-disc encryption keys for the entry point and kernel thunk address.
ENTRY_XOR_RETAIL = 0xA8FC57AB
ENTRY_XOR_DEBUG = 0x94859D4B
THUNK_XOR_RETAIL = 0x5B6D40B6
THUNK_XOR_DEBUG = 0xEFB1F152

SECTION_WRITABLE = 0x00000001
SECTION_PRELOAD = 0x00000002
SECTION_EXECUTABLE = 0x00000004


def build(title="Test Title", title_id=0x00000001, xdk=5849, ltcg=False,
          debug=False, text_size=0x8000, ordinals=(1, 15, 24, 184, 255),
          extra_sections=(), renderware=None):
    """Return the bytes of a synthetic XBE.

    Layout: headers page, then one raw page per section. Everything the
    parser reads lives in the headers page except section payloads.
    """
    # --- plan the layout -------------------------------------------------
    sections = [(".text", SECTION_PRELOAD | SECTION_EXECUTABLE, text_size),
                (".rdata", SECTION_PRELOAD, 0x1000),
                (".data", SECTION_PRELOAD | SECTION_WRITABLE, 0x1000)]
    # Extra sections are deliberately not preloaded, i.e. demand-loaded.
    sections += [(name, 0, 0x1000) for name in extra_sections]

    cert_off = 0x200
    sec_hdr_off = 0x600
    names_off = sec_hdr_off + len(sections) * 56
    lib_off = names_off + 0x100
    thunk_off = lib_off + 0x100

    data = bytearray(HEADER_SIZE)

    # --- section headers and payloads ------------------------------------
    raw_cursor = HEADER_SIZE
    va_cursor = BASE + HEADER_SIZE
    name_cursor = names_off
    payloads = bytearray()

    for i, (name, flags, size) in enumerate(sections):
        off = sec_hdr_off + i * 56
        raw_size = size
        struct.pack_into("<I", data, off + 0, flags)
        struct.pack_into("<I", data, off + 4, va_cursor)
        struct.pack_into("<I", data, off + 8, size)
        struct.pack_into("<I", data, off + 12, raw_cursor)
        struct.pack_into("<I", data, off + 16, raw_size)
        struct.pack_into("<I", data, off + 20, BASE + name_cursor)

        encoded = name.encode("ascii") + b"\x00"
        data[name_cursor:name_cursor + len(encoded)] = encoded
        name_cursor += len(encoded)

        payload = bytearray(raw_size)
        if name == ".rdata" and renderware:
            marker = f"RenderWare Version {renderware}".encode("ascii")
            payload[0:len(marker)] = marker
        payloads += payload

        raw_cursor += raw_size
        va_cursor += (size + 0xFFF) & ~0xFFF

    # --- kernel thunk table ----------------------------------------------
    # Unresolved imports are stored as (0x80000000 | ordinal), zero-terminated.
    for i, ordinal in enumerate(ordinals):
        struct.pack_into("<I", data, thunk_off + i * 4, 0x80000000 | ordinal)
    struct.pack_into("<I", data, thunk_off + len(ordinals) * 4, 0)
    thunk_va = BASE + thunk_off

    # --- library versions -------------------------------------------------
    libs = [("D3D8LTCG" if ltcg else "D3D8", xdk), ("XAPILIB", xdk),
            ("XBOXKRNL", xdk), ("LIBCMT", xdk)]
    for i, (name, build_no) in enumerate(libs):
        off = lib_off + i * 16
        data[off:off + 8] = name.encode("ascii")[:8].ljust(8, b"\x00")
        struct.pack_into("<HHHH", data, off + 8, 1, 0, build_no, 0)

    # --- certificate ------------------------------------------------------
    struct.pack_into("<I", data, cert_off + 0, 0x1D0)
    struct.pack_into("<I", data, cert_off + 8, title_id)
    name_utf16 = title.encode("utf-16-le")[:80].ljust(80, b"\x00")
    data[cert_off + 12:cert_off + 92] = name_utf16
    struct.pack_into("<I", data, cert_off + 156, 0x00000002)  # allowed media
    struct.pack_into("<I", data, cert_off + 160, 0x00000001)  # region: NA

    # --- header -----------------------------------------------------------
    entry_xor = ENTRY_XOR_DEBUG if debug else ENTRY_XOR_RETAIL
    thunk_xor = THUNK_XOR_DEBUG if debug else THUNK_XOR_RETAIL
    image_size = va_cursor - BASE

    data[0:4] = b"XBEH"
    struct.pack_into("<I", data, 0x0104, BASE)
    struct.pack_into("<I", data, 0x0108, HEADER_SIZE)
    struct.pack_into("<I", data, 0x010C, image_size)
    struct.pack_into("<I", data, 0x0110, 0x0178)
    struct.pack_into("<I", data, 0x0118, BASE + cert_off)
    struct.pack_into("<I", data, 0x011C, len(sections))
    struct.pack_into("<I", data, 0x0120, BASE + sec_hdr_off)
    struct.pack_into("<I", data, 0x0128, (BASE + HEADER_SIZE) ^ entry_xor)
    struct.pack_into("<I", data, 0x0158, thunk_va ^ thunk_xor)
    struct.pack_into("<I", data, 0x0160, len(libs))
    struct.pack_into("<I", data, 0x0164, BASE + lib_off)
    struct.pack_into("<I", data, 0x0168, BASE + lib_off + 32)  # XBOXKRNL entry

    return bytes(data) + bytes(payloads)


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("output", help="Path to write the XBE to")
    ap.add_argument("--title", default="Test Title")
    ap.add_argument("--title-id", type=lambda s: int(s, 0), default=0x00000001)
    ap.add_argument("--xdk", type=int, default=5849, help="XDK build number")
    ap.add_argument("--ltcg", action="store_true", help="Link as D3D8LTCG")
    ap.add_argument("--debug", action="store_true", help="Use debug XOR keys")
    ap.add_argument("--text-size", type=lambda s: int(s, 0), default=0x8000)
    ap.add_argument("--renderware", metavar="VERSION",
                    help="Embed a RenderWare version marker, e.g. 3.7.0.0")
    ap.add_argument("--section", action="append", default=[], metavar="NAME",
                    help="Add a demand-loaded section (repeatable)")
    args = ap.parse_args()

    blob = build(title=args.title, title_id=args.title_id, xdk=args.xdk,
                 ltcg=args.ltcg, debug=args.debug, text_size=args.text_size,
                 extra_sections=tuple(args.section), renderware=args.renderware)

    Path(args.output).write_bytes(blob)
    print(f"Wrote {args.output} ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
