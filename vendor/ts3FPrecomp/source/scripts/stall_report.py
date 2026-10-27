#!/usr/bin/env python3
"""
Where a stalled title stopped, from the profile dump and the lifted code.

A title that runs forever without crashing gives no address to start from.
Finding one by hand means: take the last function entered, look up what it
calls, check whether any of those ran, repeat. That walk found Burnout 2's
stall in four steps after state comparison had spent hours matching structures
that were all correct.

    python3 scripts/stall_report.py runs/run37.prof <gen-dir>

Two signals, both from data already on disk:

  the frontier    the functions entered last, by call ordinal. The tail of
                  that list is where execution stopped making progress.

  dead ends       a function that was entered, calls other functions, and
                  none of them ran. It went in and did not come out -- either
                  it is waiting on hardware state that never changes, or it
                  took an early return on a value that should not have been
                  zero. Burnout 2 stops in sub_0021B080, which reserves push
                  buffer space and waits for the GPU read pointer to advance.

Needs a profile dump with the first_call column (RECOMP_PROFILE_DUMP, written
by a build lifted with --trace-all-entries).
"""

import argparse
import collections
import pathlib
import re
import sys

CALL = re.compile(r"RECOMP_ABI_CALL\(0x[0-9A-Fa-f]+u,\s*(sub_[0-9A-Fa-f]+)\)")
TAIL = re.compile(r"\b(sub_[0-9A-Fa-f]+)\(\);\s*return;\s*/\* tail jmp")
DEF = re.compile(r"^void (sub_[0-9A-Fa-f]+)\(void\)")


def load_profile(path):
    """{name: (hits, first_call)} from a RECOMP_PROFILE_DUMP file."""
    entries = {}
    saw_header = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("#"):
            saw_header = saw_header or "first_call" in line
            continue
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        name = parts[1]
        hits = int(parts[2]) if parts[2].isdigit() else 0
        first = int(parts[3]) if len(parts) > 3 and parts[3].isdigit() else 0
        entries[name] = (hits, first)
    if not saw_header:
        print("warning: this dump has no first_call column, so the frontier "
              "cannot be ordered.\n         Rebuild with a current runtime and "
              "re-run.", file=sys.stderr)
    return entries


def load_callees(gen_dir):
    """{caller: {callee, ...}} from the lifted sources."""
    callees = collections.defaultdict(set)
    for src in sorted(gen_dir.glob("recomp_0*.c")):
        current = None
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            m = DEF.match(line)
            if m:
                current = m.group(1)
                continue
            if current is None:
                continue
            for pattern in (CALL, TAIL):
                for callee in pattern.findall(line):
                    if callee != current:
                        callees[current].add(callee)
    return callees


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("profile", type=pathlib.Path,
                    help="a RECOMP_PROFILE_DUMP file (runs/runNN.prof)")
    ap.add_argument("gen_dir", type=pathlib.Path,
                    help="the lifted sources (src/recomp/gen)")
    ap.add_argument("--frontier", type=int, default=12,
                    help="how many of the last-entered functions to show")
    ap.add_argument("--dead-ends", type=int, default=12,
                    help="how many dead ends to show")
    args = ap.parse_args()

    if not args.profile.is_file():
        print(f"error: no such profile: {args.profile}", file=sys.stderr)
        return 1
    if not args.gen_dir.is_dir():
        print(f"error: no such directory: {args.gen_dir}", file=sys.stderr)
        return 1

    profile = load_profile(args.profile)
    callees = load_callees(args.gen_dir)
    ran = set(profile)

    print(f"{len(profile)} functions entered, "
          f"{sum(len(v) for v in callees.values())} static call edges\n")

    # -- the frontier ---------------------------------------------------
    ordered = sorted(profile.items(), key=lambda kv: kv[1][1], reverse=True)
    print(f"-- frontier: the last {args.frontier} functions entered")
    for name, (hits, first) in ordered[:args.frontier]:
        out = callees.get(name, set())
        reached = sum(1 for c in out if c in ran)
        note = ""
        if out and reached == 0:
            note = "   <- calls nothing that ran"
        print(f"     {first:>9}  {name:<16} hits={hits:<7}"
              f" callees {reached}/{len(out)}{note}")

    # -- dead ends -------------------------------------------------------
    #
    # Ordered by how late they were reached: the last one to go in and not
    # come out is the one holding everything up. A leaf is not a dead end --
    # it has nothing to call -- so functions with no callees are excluded.
    dead = [(first, name, hits, len(callees[name]))
            for name, (hits, first) in profile.items()
            if callees.get(name) and not any(c in ran for c in callees[name])]
    dead.sort(reverse=True)

    print(f"\n-- dead ends: entered, and nothing they call ever ran "
          f"({len(dead)} total)")
    if not dead:
        print("     none -- every function that ran reached something it calls")
    for first, name, hits, n_callees in dead[:args.dead_ends]:
        print(f"     {first:>9}  {name:<16} hits={hits:<7} {n_callees} callees, none reached")

    if dead:
        latest = dead[0]
        print(f"\n   The last of those is {latest[1]}, entered at call "
              f"{latest[0]}.")
        print("   Read it: a wait on hardware state looks like a loop on a")
        print("   memory read, and an early return looks like a test of a")
        print("   value that should not be zero. RECOMP_WATCH_VA names")
        print("   whoever writes it; xemu says what the value should be.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
