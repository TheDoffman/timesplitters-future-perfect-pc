"""
Name the XDK library functions statically linked into an XBE.

Every Xbox title links D3D8, DirectSound, XAPI and the rest into its own image,
at addresses that move with each XDK build and each link. Replacing those
libraries by name -- the route that took the DOAXBV port to gameplay -- needs
the addresses first, and hard-coding them makes the replacement one game's.

XbSymbolDatabase (Cxbx-Reloaded, MIT licence) finds them by signature for XDK
builds 3911 to 5849, LTCG builds included. This wraps its command-line tool
and writes JSON the rest of the pipeline can read:

    py -3 -m tools.xdk_symbols game/default.xbe [--cli PATH] [--out FILE]

The tool is found from --cli, then the XBSDB_CLI environment variable, then a
build under third_party/XbSymbolDatabase. Build it with:

    cmake -S third_party/XbSymbolDatabase -B third_party/XbSymbolDatabase/build
    cmake --build third_party/XbSymbolDatabase/build --config Release

Checked against a proven port: on DOAXBV (XDK 4928) it reproduces all five D3D8
addresses that port hard-codes -- Direct3D_CreateDevice, D3DDevice_Clear,
D3DDevice_DrawIndexedVertices, D3DDevice_Reset and D3DDevice_Swap.

The output names addresses inside the user's own XBE. It is derived from game
files, so it stays beside the XBE and out of Git, like the analysis JSON.
"""

import argparse
import pathlib
import struct
import collections
import glob
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# `LIB__FUN__call__Name(type arg, ...) = 0xADDR`, `LIB__VAR__Name = 0xADDR`,
# `LIB__UNK__Name = 0xADDR`, and the plain `LIB__Name = 0xADDR` the tool
# prints without -e. The library is at most eight characters: the tool
# prints it with %.8s.
_LINE = re.compile(
    r"^(?P<lib>[A-Za-z0-9]{1,8}?)__(?:(?P<kind>FUN|VAR|UNK)__)?"
    r"(?P<rest>.+?) = 0x(?P<addr>[0-9a-fA-F]{1,8})$")
_FUN = re.compile(r"^(?P<call>[a-z]+)__(?P<name>[A-Za-z_][\w@?$]*)\((?P<params>.*)\)$")

# Bytes of stack each parameter type occupies. Register-passed parameters
# (a fastcall's ecx and edx, a thiscall's this) occupy none.
_STACK_BYTES = {"psh": 4, "psh2": 8}

_KINDS = {"FUN": "function", "VAR": "variable", "UNK": "unknown", None: "unknown"}


def parse_params(text):
    """`psh Count, ecx this, psh2 rtTime` -> [{type, name}, ...]."""
    params = []
    for piece in text.split(","):
        piece = piece.strip()
        if not piece:
            continue
        kind, _, name = piece.partition(" ")
        params.append({"type": kind, "name": name.strip()})
    return params


def parse_line(line):
    """One line of CLI output -> a symbol dict, or None if it is not one."""
    m = _LINE.match(line.strip())
    if not m:
        return None
    kind = m.group("kind")
    rest = m.group("rest")
    sym = {
        "library": m.group("lib"),
        "kind": _KINDS[kind],
        "address": int(m.group("addr"), 16),
    }
    if kind == "FUN":
        f = _FUN.match(rest)
        if not f:
            return None
        params = parse_params(f.group("params"))
        sym.update(name=f.group("name"), call=f.group("call"), params=params,
                   stack_bytes=sum(_STACK_BYTES.get(p["type"], 0) for p in params))
    else:
        sym["name"] = rest
    return sym


def parse_output(text):
    return [s for s in (parse_line(l) for l in text.splitlines()) if s]


def find_cli(explicit=None):
    if explicit:
        return explicit
    if os.environ.get("XBSDB_CLI"):
        return os.environ["XBSDB_CLI"]
    # The exact executable name, not a prefix. A Visual Studio build tree has
    # five other files starting XbSymbolDatabaseCLI (.slnx, .vcxproj,
    # .vcxproj.filters, .exe.recipe, a .dir folder), and the prefix match
    # picked the .slnx: WinError 193, "not a valid Win32 application".
    exe = "XbSymbolDatabaseCLI" + (".exe" if os.name == "nt" else "")
    pattern = os.path.join(REPO, "third_party", "XbSymbolDatabase", "build",
                           "**", exe)
    hits = [p for p in glob.glob(pattern, recursive=True) if os.path.isfile(p)]
    # A multi-config generator can leave Debug and Release side by side.
    hits.sort(key=lambda p: "release" not in p.lower())
    return hits[0] if hits else None


def title_id(xbe_path):
    """The XBE certificate's title id, or None.

    Read straight out of the header rather than through tools.xbe_parser: this
    runs before the pipeline and should not need it. The base address is at
    +0x104, the certificate pointer at +0x118, and the id 8 bytes into the
    certificate.
    """
    try:
        data = pathlib.Path(xbe_path).read_bytes()
        base = struct.unpack_from("<I", data, 0x104)[0]
        cert = struct.unpack_from("<I", data, 0x118)[0]
        return struct.unpack_from("<I", data, cert - base + 0x08)[0]
    except Exception:
        return None


def load_extra(xbe_path, explicit):
    """Symbols the database cannot provide, supplied by hand.

    XbSymbolDatabase covers Direct3D, DirectSound and the application library.
    It does not cover the video library, the networking library, or several
    others, so a title that needs one of those replaced has addresses and no
    names. This is where those names live: a JSON file in the same shape as
    the generated one, keyed by title id under config/extra_symbols, holding
    entries that `scripts/section_calls.py` found and a person then named.

    Per title rather than per XDK version deliberately. A statically linked
    library lands wherever the linker put it, so its addresses differ between
    two titles built against the same SDK; only the behaviour is shared.
    """
    paths = []
    if explicit:
        paths.append(explicit)
    tid = title_id(xbe_path)
    if tid is not None:
        root = pathlib.Path(__file__).resolve().parents[2]
        paths.append(root / "config" / "extra_symbols" / ("%08X.json" % tid))
    for path in paths:
        path = pathlib.Path(path)
        if not path.is_file():
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        extra = data.get("symbols", [])
        print("%d hand-supplied symbol(s) from %s" % (len(extra), path),
              file=sys.stderr)
        return extra
    return []


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("xbe", help="the title's default.xbe")
    ap.add_argument("--cli", help="path to XbSymbolDatabaseCLI")
    ap.add_argument("--out", help="output JSON (default: <xbe stem>_xdk_symbols.json "
                                  "beside the XBE)")
    ap.add_argument("--extra", help="JSON of hand-supplied symbols to merge, for "
                                    "libraries the database does not cover "
                                    "(default: config/extra_symbols/<title id>.json)")
    args = ap.parse_args(argv)

    cli = find_cli(args.cli)
    if not cli:
        sys.exit("XbSymbolDatabaseCLI not found. Pass --cli, set XBSDB_CLI, or "
                 "build it under third_party/XbSymbolDatabase (see --help).")

    run = subprocess.run([cli, args.xbe, "-e"], capture_output=True, text=True)
    if run.returncode != 0:
        sys.exit(f"{cli} exited {run.returncode}: {run.stderr.strip()[:400]}")

    symbols = parse_output(run.stdout)
    known = {s["address"] for s in symbols}
    for sym in load_extra(args.xbe, args.extra):
        if sym.get("address") not in known:
            symbols.append(sym)
    symbols.sort(key=lambda s: s["address"])
    if not symbols:
        sys.exit("no symbols recognised -- is this an XBE the database covers?")

    out = args.out or os.path.join(
        os.path.dirname(os.path.abspath(args.xbe)),
        os.path.splitext(os.path.basename(args.xbe))[0] + "_xdk_symbols.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"xbe": os.path.abspath(args.xbe), "symbols": symbols}, f, indent=1)

    per_lib = collections.Counter((s["library"], s["kind"]) for s in symbols)
    print(f"{len(symbols)} symbols -> {out}", file=sys.stderr)
    for (lib, kind), n in sorted(per_lib.items()):
        print(f"  {lib:9s} {kind:9s} {n}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
