"""
Which of our HLE replacements a title's XDK actually has -- before lifting it.

The HLE replaces XDK functions by name, so a name the title's build does not
export is a replacement that silently never runs. That is invisible until the
title is lifted, built and watched doing the wrong thing.

It does not have to be. The names are in src/hle and the addresses are in the
title's xdk_symbols.json, and comparing them takes a second:

    py -3 -m tools.hle_audit "games/Max Payne/default.xbe"
    py -3 -m tools.hle_audit --all          # every title with a symbols file

On Max Payne this printed D3DDevice_Swap and the four SetVertexShaderConstant
variants as absent, with D3DDevice_Present and D3DDevice_SetVertexShaderConstant
sitting right there in the title unclaimed -- which is two of the four things
that were actually wrong with it, found before it ran once.

The `near` column is the part worth reading. A replacement missing with no
near name is a function the build genuinely does not have. A replacement
missing while a similarly-named export goes unclaimed is almost always the
same function under the name an older or newer XDK gave it, and wants a
replacement of its own.
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

EXPORT_RE = re.compile(r"^\s*HLE_EXPORT\(\s*([A-Za-z_]\w*)\s*\)", re.M)
IMPORT_VAR_RE = re.compile(r"^\s*HLE_IMPORT_VAR\(\s*([A-Za-z_]\w*)\s*\)", re.M)


def hle_names(hle_dir):
    """Every name src/hle claims to replace, and every variable it imports."""
    exports, variables = set(), set()
    for path in sorted(Path(hle_dir).rglob("*.c")):
        text = path.read_text(encoding="utf-8", errors="replace")
        exports |= set(EXPORT_RE.findall(text))
        variables |= set(IMPORT_VAR_RE.findall(text))
    return exports, variables


def title_symbols(path):
    """{name: address} from a tools.xdk_symbols JSON, whatever its shape."""
    out = {}

    def walk(x):
        if isinstance(x, dict):
            name = x.get("name") or x.get("symbol")
            addr = x.get("address")
            if addr is None:
                addr = x.get("addr")
            if addr is None:
                addr = x.get("va")
            if isinstance(name, str) and isinstance(addr, int):
                out[name] = addr
            for v in x.values():
                walk(v)
        elif isinstance(x, list):
            for v in x:
                walk(v)

    walk(json.loads(Path(path).read_text(encoding="utf-8")))
    return out


def stem(name):
    """The name with its trailing variant marker removed.

    D3DDevice_SetVertexShaderConstant4 and _NotInlineFast are the same
    function as D3DDevice_SetVertexShaderConstant on a build that has only
    one; GetBackBuffer2 is GetBackBuffer. Stripping the tail is what makes
    those line up as candidates rather than reading as unrelated.
    """
    n = re.sub(r"(?:_r\d+|__r\d+)$", "", name)
    n = re.sub(r"(?:NotInlineFast|NotInline|Fast|Inline)$", "", n)
    n = re.sub(r"\d+$", "", n)
    return n


def audit(symbols_path, exports, variables):
    syms = title_symbols(symbols_path)
    have = exports & syms.keys()
    missing = sorted(exports - syms.keys())
    unclaimed = syms.keys() - exports

    # For each missing name, any export of the title that shares its stem and
    # that nothing replaces -- the rename candidates.
    by_stem = {}
    for s in unclaimed:
        by_stem.setdefault(stem(s), []).append(s)
    near = {m: sorted(by_stem.get(stem(m), [])) for m in missing}

    return {
        "symbols": len(syms),
        "have": sorted(have),
        "missing": missing,
        "near": {k: v for k, v in near.items() if v},
        "vars_missing": sorted(variables - syms.keys()),
    }


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("xbe", nargs="?", help="the title's default.xbe (or its "
                                           "_xdk_symbols.json)")
    ap.add_argument("--all", action="store_true",
                    help="every games/*/default_xdk_symbols.json")
    ap.add_argument("--hle-dir", default=str(ROOT / "src" / "hle"))
    ap.add_argument("--json", action="store_true", help="machine-readable")
    args = ap.parse_args()

    exports, variables = hle_names(args.hle_dir)
    if not exports:
        print(f"no HLE_EXPORT names under {args.hle_dir}", file=sys.stderr)
        return 1

    targets = []
    if args.all:
        targets = sorted(ROOT.glob("games/*/default_xdk_symbols.json"))
    elif args.xbe:
        p = Path(args.xbe)
        if p.suffix.lower() == ".xbe":
            p = p.with_name(p.stem + "_xdk_symbols.json")
        if not p.is_file():
            print(f"{p} not found -- run tools.xdk_symbols on the title first",
                  file=sys.stderr)
            return 1
        targets = [p]
    else:
        ap.error("give an XBE or --all")

    results = {}
    for path in targets:
        title = path.parent.name
        results[title] = audit(path, exports, variables)

    if args.json:
        print(json.dumps(results, indent=1))
        return 0

    print(f"{len(exports)} HLE replacements defined in "
          f"{Path(args.hle_dir).relative_to(ROOT)}\n")
    for title, r in results.items():
        print(f"{title}")
        print(f"  {len(r['have'])}/{len(exports)} resolve "
              f"({r['symbols']} XDK symbols named in this title)")
        renames = r["near"]
        if renames:
            print(f"  {len(renames)} missing with a candidate under another name:")
            for name, cands in sorted(renames.items()):
                print(f"    {name:<44} -> {', '.join(cands)}")
        plain = [m for m in r["missing"] if m not in renames]
        if plain:
            print(f"  {len(plain)} missing outright: {', '.join(plain[:8])}"
                  + (" ..." if len(plain) > 8 else ""))
        if r["vars_missing"]:
            print(f"  variables not found: {', '.join(r['vars_missing'])}")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
