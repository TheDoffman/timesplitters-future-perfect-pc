#!/usr/bin/env python3
"""Turn a watchdog stack dump into a guest call chain.

RECOMP_WATCHDOG_SECS prints the guest ESP, the recent indirect-call targets,
and up to 400 words of guest stack. Those words are not opaque: the lifted
code pushes the guest return address before every call, so the stack holds a
real return-address chain -- the one thing a title spinning in pure guest code
with no kernel calls leaves behind.

This reads the dump and names each of those addresses, innermost first.

    py -3 scripts/stackwalk.py runs/run.err
    py -3 scripts/stackwalk.py runs/run.err --functions path/to/functions.json

Values that are not code addresses are locals and saved registers; they are
dropped rather than guessed at. A repeated frame is collapsed with a count,
because a spin usually shows the same two or three addresses over and over.
"""
import argparse
import bisect
import json
import pathlib
import re
import sys


def load_functions(path):
    """start -> (name, end), plus a sorted list of starts for bisect."""
    data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    table = {}
    for f in data:
        start = int(f["start"], 16)
        table[start] = (f["name"], int(f["end"], 16))
    return table, sorted(table)


def describe(va, table, starts):
    """Name the function containing va, with its offset, or None."""
    i = bisect.bisect_right(starts, va) - 1
    if i < 0:
        return None
    start = starts[i]
    name, end = table[start]
    if va > end + 16:          # slack: `end` is the last instruction, not past it
        return None
    return "%s+0x%X" % (name, va - start) if va != start else name


def parse_dump(text):
    """(esp, [(addr, value)], [icall targets], regs line) from a watchdog dump."""
    esp = None
    words = []
    icalls = []
    regs = None

    m = re.search(r"\[WATCHDOG\][^\n]*guest esp=0x([0-9A-Fa-f]+)", text)
    if m:
        esp = int(m.group(1), 16)
    m = re.search(r"^\s*regs: (.+)$", text, re.M)
    if m:
        regs = m.group(1).strip()
    m = re.search(r"recent ICALL targets:([^\n]*)", text)
    if m:
        icalls = [int(t, 16) for t in m.group(1).split()]
    for m in re.finditer(r"^\s*GS ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{8})\s*$", text, re.M):
        words.append((int(m.group(1), 16), int(m.group(2), 16)))
    return esp, words, icalls, regs


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump", help="run log containing a [WATCHDOG] report")
    ap.add_argument("--functions", default="tools/disasm/output/functions.json",
                    help="functions.json from tools.disasm (default: %(default)s)")
    args = ap.parse_args()

    text = pathlib.Path(args.dump).read_text(encoding="utf-8", errors="replace")
    esp, words, icalls, regs = parse_dump(text)

    if esp is None:
        sys.exit("no [WATCHDOG] report in %s -- was RECOMP_WATCHDOG_SECS set, "
                 "and did the run reach the timeout?" % args.dump)

    table, starts = load_functions(args.functions)

    print("guest esp = 0x%08X, %d stack words captured" % (esp, len(words)))
    if regs:
        print("regs: %s" % regs)

    if icalls:
        print("\n-- recent indirect-call targets, oldest first")
        print("   (the ring is 16 deep; a spin repeats the same few)")
        run_name, run_len = None, 0
        for t in icalls:
            name = describe(t, table, starts) or "0x%08X" % t
            # The synthetic thunk range is KERNEL_VA_BASE plus ordinal*4 over
            # roughly 366 ordinals, so it ends well below 0xFE001000. Labelling
            # the whole 16 MB window swallowed the MCPX aperture at 0xFE800000
            # and reported the APU as a kernel thunk -- the exact confusion the
            # label exists to prevent.
            if 0xFE000000 <= t < 0xFE001000:
                name = "0x%08X (kernel thunk, ordinal %d)" % (t, (t - 0xFE000000) // 4)
            elif 0xFE800000 <= t < 0xFF000000:
                name = "0x%08X (MCPX aperture: APU/AC97/USB)" % t
            if name == run_name:
                run_len += 1
                continue
            if run_name:
                print("   %s%s" % (run_name, "  x%d" % run_len if run_len > 1 else ""))
            run_name, run_len = name, 1
        if run_name:
            print("   %s%s" % (run_name, "  x%d" % run_len if run_len > 1 else ""))

    print("\n-- call chain, innermost first")
    print("   (return addresses found on the guest stack; other words dropped)")
    shown = 0
    last, count = None, 0
    for addr, value in words:
        what = describe(value, table, starts)
        if what is None:
            continue
        if what == last:
            count += 1
            continue
        if last:
            print("   %-40s%s" % (last, "  x%d" % count if count > 1 else ""))
        last, count = what, 1
        shown += 1
    if last:
        print("   %-40s%s" % (last, "  x%d" % count if count > 1 else ""))
    if not shown:
        print("   nothing on the stack resolved to a known function -- either the"
              "\n   stack is shallow and holds only locals, or functions.json is"
              "\n   from a different build of the title.")


if __name__ == "__main__":
    main()
