"""Carry XDK symbols from a title where they are identified to one where they are not.

The symbol database does not name every library in every image. It finds five
XMV functions in Black and none at all in TimeSplitters: Future Perfect or
Mortal Kombat: Deadly Alliance -- and Black and Future Perfect are both built
against XDK 5849, so it is the *same library code* sitting in both images. The
HLE replacements exist and simply never bind, because nothing tells them where
to bind.

Statically linked library code is identical between two images built against
the same XDK, so the bytes at a named function in one image can be searched for
in the other. That is the whole idea: take the signature from where the name is
known and find it where it is not.

    py -3 -m tools.xdk_port_symbols \\
        --from "games/Black/default.xbe" \\
        --to   "games/Timesplitters - Future Perfect/default.xbe" \\
        --library XMV

Writes config/extra_symbols/<target title id>.json, which tools.xdk_symbols
already merges. Re-run tools.xdk_symbols afterwards.

A match has to be unique to be trustworthy: a signature found twice is not a
signature, and one found nowhere means the builds differ after all. Both are
reported rather than guessed at, and neither is written.
"""

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


def load_xbe(path):
    """(data, [(va, size, raw, rsize)]) for an XBE's sections."""
    data = Path(path).read_bytes()
    if data[:4] != b"XBEH":
        raise ValueError("not an XBE: %s" % path)
    base = int.from_bytes(data[0x0104:0x0108], "little")
    count = int.from_bytes(data[0x011C:0x0120], "little")
    hdr = int.from_bytes(data[0x0120:0x0124], "little") - base
    secs = []
    for i in range(count):
        o = hdr + i * 0x38
        if o + 0x38 > len(data):
            break
        # XBE section header: flags +0x00, virtual address +0x04, virtual
        # size +0x08, raw address +0x0C, raw size +0x10, name pointer +0x14.
        # These were read in the wrong order first time round, which inverted
        # every VA/offset conversion and made a correct hit inside XMV look
        # like a false positive inside XNET.
        va = int.from_bytes(data[o + 0x04:o + 0x08], "little")
        vsz = int.from_bytes(data[o + 0x08:o + 0x0C], "little")
        raw = int.from_bytes(data[o + 0x0C:o + 0x10], "little")
        rsz = int.from_bytes(data[o + 0x10:o + 0x14], "little")
        nameptr = int.from_bytes(data[o + 0x14:o + 0x18], "little") - base
        name = ""
        if 0 <= nameptr < len(data):
            end = data.find(b"\x00", nameptr)
            name = data[nameptr:end].decode("ascii", "replace")
        secs.append((va, vsz, raw, rsz, name))
    return data, secs


def va_to_off(secs, va):
    for sva, vsz, raw, rsz, _name in secs:
        if sva <= va < sva + vsz:
            off = raw + (va - sva)
            return off if va - sva < rsz else None
    return None


def off_to_va(secs, off):
    for sva, vsz, raw, rsz, _name in secs:
        if raw <= off < raw + rsz:
            return sva + (off - raw)
    return None


def title_id(data):
    base = int.from_bytes(data[0x0104:0x0108], "little")
    cert = int.from_bytes(data[0x0118:0x011C], "little") - base
    if cert < 0 or cert + 12 > len(data):
        return None
    return "%08X" % int.from_bytes(data[cert + 8:cert + 12], "little")


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--from", dest="src", required=True,
                    help="XBE whose symbols are known")
    ap.add_argument("--to", dest="dst", required=True,
                    help="XBE to find them in")
    ap.add_argument("--library", action="append",
                    help="only these libraries (repeatable); default all")
    ap.add_argument("--bytes", type=int, default=48, metavar="N",
                    help="signature length (default 48). Long enough to be "
                         "unique, short enough to survive a differing tail")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)

    src_syms = Path(args.src).with_name(Path(args.src).stem + "_xdk_symbols.json")
    if not src_syms.is_file():
        print("error: no symbols beside %s; run tools.xdk_symbols on it first"
              % args.src, file=sys.stderr)
        return 2
    syms = json.loads(src_syms.read_text(encoding="utf-8"))["symbols"]
    if args.library:
        want = set(args.library)
        syms = [s for s in syms if s.get("library") in want]
    syms = [s for s in syms if s.get("kind") == "function"]
    if not syms:
        print("no function symbols to port")
        return 0

    sdata, ssecs = load_xbe(args.src)
    ddata, dsecs = load_xbe(args.dst)

    found, ambiguous, missing = [], [], []
    for s in syms:
        off = va_to_off(ssecs, s["address"])
        if off is None:
            missing.append((s["name"], "not in a raw section"))
            continue
        sig = sdata[off:off + args.bytes]
        if len(sig) < args.bytes:
            missing.append((s["name"], "too close to the end of the image"))
            continue
        # Search only the section the library lives in.
        #
        # Statically linked libraries land in a section named after them, and
        # searching the whole image finds the same bytes elsewhere: the first
        # run of this matched XMVPlaybackDestroy inside XNET, which is a false
        # positive that would have been written to disc as a fact, and matched
        # four others twice each. Scoping to the section makes a hit mean
        # something.
        lib = s.get("library") or ""
        scope = [(raw, raw + rsz) for _va, _vsz, raw, rsz, nm in dsecs
                 if nm == lib]
        if not scope:
            missing.append((s["name"], "target has no %s section" % lib))
            continue
        hits = []
        for lo, hi in scope:
            start = lo
            while True:
                i = ddata.find(sig, start, hi)
                if i < 0:
                    break
                hits.append(i)
                start = i + 1
                if len(hits) > 1:
                    break
        if not hits:
            missing.append((s["name"], "no match"))
        elif len(hits) > 1:
            ambiguous.append((s["name"], len(hits)))
        else:
            va = off_to_va(dsecs, hits[0])
            if va is None:
                missing.append((s["name"], "match outside any section"))
            else:
                found.append({"name": s["name"], "address": va,
                              "library": s.get("library"), "kind": "function"})

    for f in found:
        print("  + %-32s 0x%08X" % (f["name"], f["address"]))
    for n, why in missing:
        print("  - %-32s %s" % (n, why))
    for n, k in ambiguous:
        print("  ? %-32s %d matches, not unique" % (n, k))

    print("%d found, %d missing, %d ambiguous" % (len(found), len(missing),
                                                  len(ambiguous)))
    if not found or args.dry_run:
        return 0

    tid = title_id(ddata)
    if not tid:
        print("error: cannot read a title ID from %s" % args.dst, file=sys.stderr)
        return 2
    out = ROOT / "config" / "extra_symbols" / (tid + ".json")
    out.parent.mkdir(parents=True, exist_ok=True)
    existing = json.loads(out.read_text(encoding="utf-8")) if out.is_file() else []
    by_name = {e["name"]: e for e in existing if isinstance(e, dict)}
    for f in found:
        by_name[f["name"]] = f
    merged = sorted(by_name.values(), key=lambda e: e["address"])
    out.write_text(json.dumps(merged, indent=1), encoding="utf-8")
    print("wrote %s (%d symbols)" % (out.relative_to(ROOT), len(merged)))
    print("now re-run tools.xdk_symbols on the target, then re-lift")
    return 0


if __name__ == "__main__":
    sys.exit(main())
