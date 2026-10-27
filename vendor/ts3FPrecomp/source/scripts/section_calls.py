#!/usr/bin/env python3
"""Find the entry points of a statically linked library inside a title.

The Xbox has no DLLs: the SDK libraries are linked into the XBE, so their code
is lifted and runs as the title's own. Replacing one at its API boundary means
knowing which of its functions the game actually calls, and normally that
means a signature database. For the libraries nobody has signatured -- the
video library among them -- there is a cheaper answer.

Each library gets its own named section, so the address range already tells us
which functions belong to it. What it does not tell us is which of them are
*entry points*, and that is the only part that matters: a library with two
thousand internal functions may be reached through six.

This finds them by reading the generated C. Every call site carries both the
caller (the function it is written inside) and the callee address, so a call
whose caller sits outside the section and whose callee sits inside it is a
crossing of the boundary. Those are the functions to replace.

    py -3 scripts/section_calls.py titles/black XMV \\
        --analysis "games/Black/default_analysis.json"

Two limits worth knowing. Calls made through a function pointer are invisible
here, because the target is a run-time value; the runtime's own
`[ICALL]` logging is what finds those. And a section's code can be reached by
a tail jump as well as a call, so both are counted and the output says which.
"""

import argparse
import json
import pathlib
import re
import sys

FUNC_DEF = re.compile(r"^void (sub_([0-9A-F]{8}))(?:_hle_original)?\(void\)", re.M)
# RECOMP_ABI_CALL_POP(0xAAAAAAAAu, sub_AAAAAAAA, 12u) and the no-pop form.
CALL = re.compile(r"RECOMP_ABI_CALL(?:_POP)?\(0x([0-9A-F]{8})u,\s*sub_[0-9A-F]{8},"
                  r"(?:\s*(\d+)u)?\)")
TAIL = re.compile(r"sub_([0-9A-F]{8})\(\); return;\s*/\* tail jmp")


def load_sections(analysis_path):
    data = json.loads(pathlib.Path(analysis_path).read_text(encoding="utf-8"))
    out = {}
    for s in data["sections"]:
        base = s["virtual_addr"]
        base = int(base, 16) if isinstance(base, str) else base
        out[s["name"]] = (base, base + s["virtual_size"])
    return out


def scan(gen_dir, lo, hi):
    """Return {callee: {"pop": bytes|None, "callers": set, "tail": bool}}."""
    entries = {}
    for path in sorted(pathlib.Path(gen_dir).glob("*.c")):
        text = path.read_text(encoding="utf-8", errors="ignore")

        # Where each function starts, so a call can be attributed to a caller.
        bounds = [(m.start(), int(m.group(2), 16)) for m in FUNC_DEF.finditer(text)]
        if not bounds:
            continue
        starts = [b[0] for b in bounds]

        def caller_at(pos):
            # Rightmost function definition at or before pos.
            lo_i, hi_i = 0, len(starts) - 1
            best = None
            while lo_i <= hi_i:
                mid = (lo_i + hi_i) // 2
                if starts[mid] <= pos:
                    best = bounds[mid][1]
                    lo_i = mid + 1
                else:
                    hi_i = mid - 1
            return best

        for m in CALL.finditer(text):
            callee = int(m.group(1), 16)
            if not (lo <= callee < hi):
                continue
            caller = caller_at(m.start())
            if caller is None or lo <= caller < hi:
                continue            # internal to the library
            e = entries.setdefault(callee, {"pop": None, "callers": set(),
                                            "tail": False})
            if m.group(2) is not None:
                e["pop"] = int(m.group(2))
            e["callers"].add(caller)

        for m in TAIL.finditer(text):
            callee = int(m.group(1), 16)
            if not (lo <= callee < hi):
                continue
            caller = caller_at(m.start())
            if caller is None or lo <= caller < hi:
                continue
            e = entries.setdefault(callee, {"pop": None, "callers": set(),
                                            "tail": False})
            e["tail"] = True
            e["callers"].add(caller)
    return entries


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("project", help="titles/<name>")
    ap.add_argument("section", help="section name, e.g. XMV or DSOUND")
    ap.add_argument("--analysis", required=True,
                    help="the title's *_analysis.json")
    ap.add_argument("--gen-dir", default=None,
                    help="generated C (default <project>/src/recomp/gen)")
    args = ap.parse_args(argv)

    sections = load_sections(args.analysis)
    if args.section not in sections:
        print("no section %r. Sections: %s"
              % (args.section, ", ".join(sorted(sections))), file=sys.stderr)
        return 1
    lo, hi = sections[args.section]

    gen = args.gen_dir or str(pathlib.Path(args.project) / "src" / "recomp" / "gen")
    entries = scan(gen, lo, hi)

    print("%s spans 0x%08X..0x%08X" % (args.section, lo, hi))
    if not entries:
        print("nothing outside it calls into it by a direct call or tail jump.")
        print("It may be reached only through function pointers; check the")
        print("runtime's [ICALL] output instead.")
        return 0

    print("%d entry point(s), most-called first:\n" % len(entries))
    print("  %-12s %-6s %-8s %s" % ("entry", "args", "via", "called from"))
    order = sorted(entries.items(), key=lambda kv: (-len(kv[1]["callers"]), kv[0]))
    for addr, e in order:
        args_txt = "?" if e["pop"] is None else "%d" % (e["pop"] // 4)
        via = "tail" if e["tail"] else "call"
        callers = " ".join("0x%08X" % c for c in sorted(e["callers"])[:4])
        if len(e["callers"]) > 4:
            callers += " +%d more" % (len(e["callers"]) - 4)
        print("  0x%08X %-6s %-8s %s" % (addr, args_txt, via, callers))
    return 0


if __name__ == "__main__":
    sys.exit(main())
