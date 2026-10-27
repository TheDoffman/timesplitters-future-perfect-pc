#!/usr/bin/env python3
"""
Run the whole recompilation pipeline for one title.

The four stages have to run in order and each reads the previous one's output,
so running them by hand mostly means retyping the same XBE path four times and
remembering that stage 2 needs the JSON from stage 1.

    python3 scripts/recompile.py game_files/default.xbe

Defaults to --game-only. Lifting CRT and XDK code you intend to HLE away costs
compile time and debugging attention for code you will not run; switch to --all
only when you have a reason.

Stages:
    1. parse    tools.xbe_parser   header, sections, imports -> analysis JSON
    2. disasm   tools.disasm       .text -> instructions, functions, xrefs
    3. identify tools.func_id      classify CRT / RenderWare / game functions
    4. lift     tools.recomp       x86 -> C

Re-run a later stage on its own after changing something:

    python3 scripts/recompile.py game.xbe --from identify
    python3 scripts/recompile.py game.xbe --only lift --all

Working on more than one title: give each its own stage outputs and its own
project, or the second title silently lifts from the first one's disassembly
(every retail XBE is called default.xbe, and that name is all the guard checks):

    python3 scripts/recompile.py games/ts2/default.xbe \
        --work-dir games/_pipeline/ts2 --project titles/timesplitters2
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

def xbe_title_id(xbe):
    """The title ID from an XBE certificate, as eight uppercase hex digits.

    Read here rather than imported so the driver keeps working when the XBE is
    missing (--dry-run) and so stage one is not a prerequisite for choosing
    seeds. The certificate address is at header+0x0118 and is a plain VA, the
    image base at +0x0104, and the ID at certificate+0x08.
    """
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



STAGES = ["parse", "disasm", "identify", "lift"]


def build_commands(args, xbe: Path, analysis_json: Path):
    """Return [(stage, argv)] for the stages that were selected."""
    parse = [sys.executable, "-m", "tools.xbe_parser", str(xbe),
             "--json", str(analysis_json)]

    # Deliberately not --text-only. An XBE statically links its libraries into
    # their own executable sections -- D3D, DSOUND, XACTENG, XONLINE and the
    # rest -- and --text-only leaves those unswept. Their functions are still
    # detected, but their *ends* cannot be measured, because the end-finder
    # stops at the first address it has no instruction for. Burnout 2's
    # sub_00218CD0 came out 79 bytes instead of 321: truncated at its own tail
    # jump, with the pops and the ret stranded past the cut. Every call leaked
    # the 24-byte frame, and esp eventually walked out of the guest stack.
    #
    # The extra sections are a few hundred KB against a 2 MB .text, so the
    # scan costs little; --text-only trades correctness for a few seconds.
    disasm = [sys.executable, "-m", "tools.disasm", str(xbe)]
    if args.text_only:
        disasm.append("--text-only")
    # Every stage defaults to one shared tools/*/output directory, and the
    # recompiler's guard against mixing titles compares file names only --
    # every retail disc is default.xbe, so two titles worked on side by side
    # silently lift one game's code from the other's disassembly. --work-dir
    # gives a title its own set of stage outputs.
    work = Path(args.work_dir).resolve() if args.work_dir else None
    if work:
        disasm += ["-o", str(work / "disasm")]
    # Seeds are entry points nothing in the image references -- vtable slots
    # and indirect-call targets recovered from a run. Discovery cannot find
    # them by construction, so omitting the file here silently undoes every
    # seeding pass: the addresses stay undetected, the calls to them stay
    # unresolved, and the next run reports the very same targets as missing.
    # Picked up by default so running the pipeline never quietly discards them.
    for seed_file in args.seeds:
        disasm += ["--seed-functions", str(seed_file)]
    if args.verbose:
        disasm.append("-v")

    identify = [sys.executable, "-m", "tools.func_id", str(xbe)]
    if work:
        identify += ["--functions", str(work / "disasm" / "functions.json"),
                     "--strings", str(work / "disasm" / "strings.json"),
                     "--xrefs", str(work / "disasm" / "xrefs.json"),
                     "-o", str(work / "func_id")]
    if args.verbose:
        identify.append("-v")

    lift = [sys.executable, "-m", "tools.recomp", str(xbe)]
    lift.append("--all" if args.all else "--game-only")
    if args.split:
        lift += ["--split", str(args.split)]
    if work:
        lift += ["--disasm-dir", str(work / "disasm"),
                 "--func-id-dir", str(work / "func_id"),
                 "-o", str(work / "recomp"),
                 # Per-site indirect-call targets (tools.recomp.icall_feedback
                 # merge --sites-db): this title's, not the shared default.
                 "--icall-sites", str(work / "recomp" / "icall_sites.json")]
    # A title project copied from templates/new-game: the generated sources
    # go where its CMakeLists globs them, and the functions its
    # recomp_manual.c defines by hand are left out of the generated set so
    # the two never define the same symbol. --gen-dir still wins if both
    # are given.
    project = Path(args.project).resolve() if args.project else None
    gen_dir = args.gen_dir or (str(project / "src" / "recomp" / "gen")
                               if project else None)
    if gen_dir:
        lift += ["--gen-dir", gen_dir]
    if project and (project / "src" / "recomp_manual.c").is_file():
        lift += ["--exclude-manual", str(project / "src" / "recomp_manual.c")]
    if args.game_name:
        lift += ["--game-name", args.game_name]
    if args.trace_all_entries:
        lift.append("--trace-all-entries")
    # XDK functions with a name-keyed replacement (tools/recomp/hle.py), used
    # whenever tools.xdk_symbols has been run for this XBE.
    hle_symbols = xbe.with_name(xbe.stem + "_xdk_symbols.json")
    if hle_symbols.exists():
        lift += ["--hle-symbols", str(hle_symbols)]
    if args.verbose:
        lift.append("-v")

    commands = dict(zip(STAGES, [parse, disasm, identify, lift]))

    if args.only:
        selected = [args.only]
    else:
        selected = STAGES[STAGES.index(args.start):]

    return [(name, commands[name]) for name in selected]


def run(name: str, argv, dry_run: bool) -> bool:
    print(f"\n{'=' * 70}")
    print(f"  {STAGES.index(name) + 1}/4  {name}")
    print(f"{'=' * 70}")
    print("  $ " + " ".join(argv))

    if dry_run:
        return True

    start = time.time()
    result = subprocess.run(argv, cwd=REPO)
    elapsed = time.time() - start

    if result.returncode != 0:
        print(f"\n  FAILED: stage '{name}' exited {result.returncode} "
              f"after {elapsed:.1f}s", file=sys.stderr)
        return False

    print(f"\n  done in {elapsed:.1f}s")
    return True


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("xbe", help="Path to the title's default.xbe")
    ap.add_argument("--seeds", action="append", default=None, metavar="FILE",
                    help="Seed file of extra entry points. Repeatable. "
                         "Defaults to config/seeds/<TITLEID>.json for "
                         "the XBE given -- the file tools.seed_from_log "
                         "writes -- when one exists; --no-seeds "
                         "disables it.")
    ap.add_argument("--no-seeds", action="store_true",
                    help="Ignore this title's seed file")
    ap.add_argument("--trace-all-entries", action="store_true",
                    help="Hook every function's entry so a profiled run can "
                         "report how many distinct functions it reached. That "
                         "is the frontier measure worth comparing between "
                         "iterations; run the build with RECOMP_TRACE_PROFILE "
                         "set, or use run_and_report.py --profile.")
    ap.add_argument("--text-only", action="store_true",
                    help="Disassemble only .text, skipping the statically "
                         "linked library sections. Faster, and wrong for any "
                         "title that calls into them: their functions end up "
                         "truncated at the first byte the sweep never decoded.")
    ap.add_argument("--all", action="store_true",
                    help="Lift everything, including CRT and XDK code "
                         "(default: --game-only)")
    ap.add_argument("--split", type=int, default=1000, metavar="N",
                    help="Functions per generated .c file (default: 1000)")
    ap.add_argument("--gen-dir", metavar="DIR",
                    help="Output directory for generated sources")
    ap.add_argument("--work-dir", metavar="DIR",
                    help="Per-title directory for the stage outputs "
                         "(disasm/, func_id/, recomp/). Without it every "
                         "stage writes to the shared tools/*/output, and two "
                         "titles both called default.xbe overwrite each other.")
    ap.add_argument("--project", metavar="DIR",
                    help="A title project made from templates/new-game. "
                         "Generated sources go to DIR/src/recomp/gen and the "
                         "functions DIR/src/recomp_manual.c defines by hand "
                         "are not generated.")
    ap.add_argument("--game-name", metavar="NAME",
                    help="Name stamped into the generated-code banners")
    ap.add_argument("--json", metavar="FILE",
                    help="Where to write the stage-1 analysis JSON "
                         "(default: <xbe stem>_analysis.json beside the XBE)")
    ap.add_argument("--from", dest="start", choices=STAGES, default="parse",
                    metavar="STAGE",
                    help=f"Start from this stage: {', '.join(STAGES)}")
    ap.add_argument("--only", choices=STAGES, metavar="STAGE",
                    help="Run only this stage")
    ap.add_argument("--dry-run", action="store_true",
                    help="Print the commands without running them")
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args()

    xbe = Path(args.xbe).resolve()

    # Seeds belong to a title, not to the toolkit.
    #
    # This used to load config/seed_functions.json for every XBE. A seed is an
    # unconditional claim that a function starts at an address, and tools.disasm
    # only refuses one that lands mid-instruction -- so Burnout 2's 214
    # addresses were applied to any other title, and each one that happened to
    # fall on an instruction boundary became a fake function start, splitting
    # the real function around it. Silently, and on a "make this work for any
    # game" branch.
    if args.no_seeds:
        args.seeds = []
    elif args.seeds is None:
        args.seeds = []
        title = xbe_title_id(xbe)
        if title:
            per_title = REPO / "config" / "seeds" / (title + ".json")
            if per_title.is_file():
                args.seeds = [per_title]
                if args.verbose:
                    print(f"seeds: {per_title.name} (title {title})")
            elif args.verbose:
                print(f"seeds: none for title {title}")
        legacy = REPO / "config" / "seed_functions.json"
        if legacy.is_file():
            print(f"warning: {legacy} is ignored; seeds are per-title now, "
                  f"in config/seeds/<TITLEID>.json", file=sys.stderr)

    if not args.dry_run and not xbe.is_file():
        print(f"error: XBE not found: {xbe}", file=sys.stderr)
        return 1

    # Written next to the XBE, named after it, because tools.disasm looks
    # there first and several titles may share one directory.
    analysis_json = (Path(args.json).resolve() if args.json
                     else xbe.parent / f"{xbe.stem}_analysis.json")

    commands = build_commands(args, xbe, analysis_json)

    print(f"Title    {xbe}")
    print(f"Analysis {analysis_json}")
    print(f"Scope    {'all functions' if args.all else 'game functions only'}")

    started = time.time()
    for name, argv in commands:
        if not run(name, argv, args.dry_run):
            print("\nPipeline stopped. Fix the error above and re-run with "
                  f"--from {name}", file=sys.stderr)
            return 1

    if args.dry_run:
        print("\n(dry run: nothing was executed)")
        return 0

    print(f"\n{'=' * 70}")
    print(f"  Pipeline complete in {time.time() - started:.1f}s")
    print(f"{'=' * 70}")
    print("\nNext: build the generated sources against the runtime libraries.")
    print("See INSTRUCTIONS.md for the build and first-run loop.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
