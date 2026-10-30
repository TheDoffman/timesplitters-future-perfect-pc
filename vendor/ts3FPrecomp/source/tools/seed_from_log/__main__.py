"""
tools/seed_from_log/__main__.py

Feed function addresses a *run* discovered back into the seed file, so the next
codegen pass knows about them.

Static analysis cannot see every function. Two kinds are routinely invisible:

  * **Indirect-call targets.** Where a vtable call goes is a runtime fact.
    (`tools.rtti` recovers these statically when a title has RTTI; most do not.)
  * **Thread start routines.** The address handed to PsCreateSystemThreadEx is
    only ever pushed as an argument -- nothing calls it -- so the detector never
    finds it, it gets no dispatch entry, and the game thread silently never
    starts. The process then exits cleanly after two kernel calls, which reads
    like a successful run rather than zero progress. This is the single most
    common "it built and did nothing" cause.

Usage:

    py -3 -m tools.seed_from_log run.log game/title.xbe \
        --functions build/disasm/functions.json \
        --seeds     config/seed_functions.json

Then re-run disasm with --seed-functions and lift again.

An address the title mentioned is NOT automatically a function. A garbage slot
points at data just as easily, and seeding data splits real functions and breaks
the build far more thoroughly than the missing target did. Every candidate must
clear two gates:

  1. it lies in an executable section, and
  2. the disassembler's own probe reads it as a function body.

Both are needed. The probe alone accepts data that happens to decode -- a kernel
thunk table is a run of code addresses and decodes happily -- and on Wreckless
seeding two of its entries once took a boot that reached 34 assets down to 1.
"""

import argparse
import bisect
import json
import re
import sys
from pathlib import Path

from tools.disasm.engine import DisasmEngine
from tools.disasm.loader import DATA_SECTION_NAMES, load_image

# Each pattern must have exactly one group: the hex address.
LOG_PATTERNS = [
    # Unresolved indirect call, from the RECOMP_ICALL dispatch failure path.
    #
    # Two wordings, because the runtime's changed and old logs are still worth
    # reading. The current one says which form was refused, and only a *call*
    # belongs here: a refused jump is usually the guest entering the middle of
    # a function -- a coroutine, a longjmp, a switch arm it kept -- and seeding
    # that address declares a function start where there is none.
    # tools.seed_from_log's own gates would mostly reject it, but the ones that
    # slip through are worse than the ones that do not, so it is not offered.
    #
    # This regressed silently: renaming the line to distinguish call from jump
    # left this pattern matching nothing, and every log written afterwards
    # yielded no indirect-call seeds at all. Nobody noticed because the log
    # being tested with was older than the rename.
    (re.compile(r"unresolved call target 0x([0-9A-Fa-f]+)"),
     "Indirect-call target observed at runtime"),
    (re.compile(r"Failed to resolve VA (0x[0-9A-Fa-f]+)"),
     "Indirect-call target observed at runtime"),
    # PsCreateSystemThreadEx handed a routine with no dispatch entry. Nothing
    # in the image calls this address, so only a run can reveal it.
    (re.compile(r"start routine (0x[0-9A-Fa-f]+) not found in dispatch"),
     "Thread start routine observed at runtime"),
    # PsCreateSystemThreadEx's StartContext1. The Xbox thread convention hands
    # the wrapper its real entry through the context pointer, so on several
    # titles this is the game's main and nothing calls it directly. It is not
    # always a function -- the gates below decide, which is what they are for.
    (re.compile(r"PsCreateSystemThreadEx.*?ctx1=(0x[0-9A-Fa-f]+)"),
     "PsCreateSystemThreadEx StartContext1 observed at runtime"),
    # Generic kernel-bridge complaint about an address it could not dispatch.
    (re.compile(r"not found in dispatch.*?(0x[0-9A-Fa-f]{6,8})"),
     "Address the kernel bridge could not dispatch"),
]


def scan_log(text):
    """{address: reason} for every candidate the log mentions."""
    out = {}
    for pat, reason in LOG_PATTERNS:
        for m in pat.finditer(text):
            out.setdefault(int(m.group(1), 16), reason)
    return out


REPO = Path(__file__).resolve().parent.parent.parent


def _xbe_title_id(xbe):
    """Title ID from an XBE certificate, eight uppercase hex digits, or None."""
    try:
        data = Path(xbe).read_bytes()
    except OSError:
        return None
    if len(data) < 0x011C or data[:4] != b"XBEH":
        return None
    base = int.from_bytes(data[0x0104:0x0108], "little")
    cert = int.from_bytes(data[0x0118:0x011C], "little")
    off = cert - base
    if off < 0 or off + 12 > len(data):
        return None
    return "%08X" % int.from_bytes(data[off + 8:off + 12], "little")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log", type=Path, help="stdout+stderr of a run")
    ap.add_argument("xbe", type=Path)
    ap.add_argument("--functions", type=Path, required=True,
                    help="disasm functions.json, to report whether a candidate "
                         "is a new function or an alias inside a known one")
    ap.add_argument("--seeds", type=Path,
                    help="seed file to update (created if absent). Defaults to "
                         "config/seeds/<TITLEID>.json for the XBE given, since "
                         "a seed only means anything for the title it was "
                         "observed on.")
    ap.add_argument("--analysis-json", type=Path,
                    help="xbe_parser output; only needed when it is not named "
                         "<xbe stem>_analysis.json next to the XBE")
    ap.add_argument("--prune", action="store_true",
                    help="re-test seeds already in the file and "
                         "drop any that fail the gates")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)

    # Which file, and does it belong to this title?
    #
    # Seeds are per-title: an address that is a function start in one image is
    # the middle of something else in the next, and tools.disasm only refuses a
    # seed that lands mid-instruction. Naming the file after the title ID is
    # what keeps them apart, so default to the right one and say so when an
    # explicit path disagrees.
    title = _xbe_title_id(args.xbe)
    if args.seeds is None:
        if not title:
            print("error: cannot read a title ID from %s; pass --seeds"
                  % args.xbe, file=sys.stderr)
            return 2
        args.seeds = (REPO / "config" / "seeds" / (title + ".json"))
        print("seeds: %s (title %s)" % (args.seeds.name, title))
    elif title and args.seeds.stem.upper() != title:
        print("warning: %s is not named for title %s; a seed written to the "
              "wrong title's file is applied to that title instead"
              % (args.seeds.name, title), file=sys.stderr)

    args.seeds.parent.mkdir(parents=True, exist_ok=True)

    candidates = scan_log(args.log.read_text(errors="replace"))
    if not candidates:
        print("No seedable addresses in the log.")
        return 0

    image = load_image(str(args.xbe),
                       str(args.analysis_json) if args.analysis_json else None)
    engine = DisasmEngine(image)
    for s in image.sections:
        if s.executable and s.name not in DATA_SECTION_NAMES:
            engine.linear_sweep(s)

    executable = [(s.virtual_addr, s.virtual_addr + s.virtual_size)
                  for s in image.sections
                  if s.executable and s.name not in DATA_SECTION_NAMES]

    def in_executable(va):
        return any(lo <= va < hi for lo, hi in executable)

    funcs = json.loads(args.functions.read_text())
    as_int = lambda v: int(v, 0) if isinstance(v, str) else v
    bounds = sorted((as_int(f["start"]), as_int(f["end"]), f["name"])
                    for f in funcs)
    starts = [b[0] for b in bounds]

    existing = json.loads(args.seeds.read_text()) if args.seeds.exists() else []

    if args.prune:
        # Seeds accepted before this check existed are still in the file.
        # Re-test them rather than trusting that they were ever verified.
        kept, dropped = [], 0
        for entry in existing:
            unsaved = engine.entry_pops_unsaved(int(entry["start"], 0))
            if unsaved:
                print("  - %s  pruned: restores %s without saving it"
                      % (entry["start"], "/".join(unsaved)))
                dropped += 1
                continue
            kept.append(entry)
        print("  pruned %d of %d existing seed(s)" % (dropped, dropped + len(kept)))
        existing = kept

    have = {e["start"].lower() for e in existing if isinstance(e, dict)}

    added = 0
    for va, reason in sorted(candidates.items()):
        key = "0x%08X" % va
        if key.lower() in have:
            print("  = %08X  already seeded" % va)
            continue
        if not in_executable(va):
            print("  - %08X  not in an executable section" % va)
            continue
        if not engine.probes_as_function_body(va):
            print("  - %08X  does not read as a function body" % va)
            continue
        unsaved = engine.entry_pops_unsaved(va)
        if unsaved:
            # Restoring a register it never saved means this is the middle of
            # a function, whatever else it looks like. Seeding it makes the
            # recompiler emit an epilogue that runs against the *caller's*
            # stack, so the caller resumes with a corrupted register and faults
            # somewhere unrelated -- worse than the missing target, because a
            # missing one is reported and this is silent.
            print("  - %08X  restores %s without saving it: mid-function"
                  % (va, "/".join(unsaved)))
            continue
        i = bisect.bisect_right(starts, va) - 1
        inside = i >= 0 and bounds[i][0] < va < bounds[i][1]
        where = ("alias inside " + bounds[i][2]) if inside else "new function"
        print("  + %08X  %s  (%s)" % (va, where, reason))
        existing.append({"start": key, "note": reason + "; decodes as a "
                                                        "function body."})
        added += 1

    if args.dry_run:
        print("dry run: %d would be added" % added)
        return 0

    args.seeds.parent.mkdir(parents=True, exist_ok=True)
    args.seeds.write_text(json.dumps(existing, indent=1))
    print("seeds: %d (+%d) -> %s" % (len(existing), added, args.seeds))
    if added:
        print("now re-run tools.disasm --seed-functions and lift again")
    return 0


if __name__ == "__main__":
    sys.exit(main())
