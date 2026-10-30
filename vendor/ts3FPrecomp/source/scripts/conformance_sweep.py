#!/usr/bin/env python3
"""
Run the differential lifter oracle over every title and rank what it finds.

tools.conformance --xbe lifts a title's own functions and runs them against
their own machine code, on the real CPU, with the same inputs. A disagreement
is a lifter bug, full stop -- no game has to be running and no one has to
notice the symptom first.

It was never run over the library. The first time it ran against a real title
it found an x87 status-word divergence in Max Payne, which is the class of bug
the debug table tells people to suspect and gives them no way to locate.

    py -3 scripts/conformance_sweep.py                 # every title
    py -3 scripts/conformance_sweep.py --limit 400     # dig deeper per title
    py -3 scripts/conformance_sweep.py --titles "Max Payne"

Do not run it beside a build. The harness compiles and links an executable of
its own, and a parallel MSBuild is enough to make that fail -- which shows up
as "NO VECTORS", never as a false pass.

Per title it reports vectors compared and functions that disagreed, then lists
every failing function once across the whole sweep. A function that fails in
several titles is the same XDK or CRT routine lifted wrong in all of them, and
worth far more than one that fails in one game.
"""

import argparse
import json
import re
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

FAIL_RE = re.compile(r"^FAIL (sub_[0-9A-Fa-f]+)", re.M)
TOTAL_RE = re.compile(r"(\d+) function vectors from the title, (\d+) mismatch")
CORPUS_RE = re.compile(r"(\d+) entry points, (\d+) functions in their call "
                       r"closure, (\d+) comparable, (\d+) rejected")


def run_one(xbe, limit, timeout):
    cmd = [sys.executable, "-m", "tools.conformance", "--only", "corpus",
           "--xbe", str(xbe), "--xbe-limit", str(limit)]
    t0 = time.time()
    try:
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           timeout=timeout)
    except subprocess.TimeoutExpired:
        return {"status": "timeout", "seconds": timeout}
    out = r.stdout + r.stderr
    res = {"seconds": round(time.time() - t0, 1)}
    m = CORPUS_RE.search(out)
    if m:
        res.update(entry_points=int(m.group(1)), comparable=int(m.group(3)),
                   rejected=int(m.group(4)))
    m = TOTAL_RE.search(out)
    if m:
        res.update(vectors=int(m.group(1)), mismatches=int(m.group(2)))
    res["failing"] = sorted(set(FAIL_RE.findall(out)))
    # Nothing compared is not the same as nothing wrong, and it must never
    # print like a pass. The harness reports "0 function vectors" when its
    # executable did not build or did not run -- a missing runtime symbol, or
    # simply another build holding the compiler -- and a clean-looking zero
    # row is the worst possible answer to "did my change break anything".
    if "vectors" not in res or (res.get("vectors", 0) == 0
                                and res.get("comparable", 0) > 0):
        res["status"] = "NO VECTORS (harness did not run)"
        res["why"] = [l for l in out.splitlines()
                      if "error" in l.lower()][-3:]
    else:
        res["status"] = "ok"
    return res


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--limit", type=int, default=120,
                    help="candidate functions per title (default 120)")
    ap.add_argument("--timeout", type=int, default=1200,
                    help="seconds per title before giving up")
    ap.add_argument("--titles", help="comma-separated game directory names")
    ap.add_argument("--out", default="games/_pipeline/_matrix/conformance.json")
    args = ap.parse_args()

    wanted = set(args.titles.split(",")) if args.titles else None
    xbes = []
    for p in sorted(ROOT.glob("games/*/default.xbe")):
        if wanted and p.parent.name not in wanted:
            continue
        xbes.append(p)
    if not xbes:
        print("no games/*/default.xbe found", file=sys.stderr)
        return 1

    results = {}
    everywhere = defaultdict(list)
    for xbe in xbes:
        title = xbe.parent.name
        print(f"[{title}] ...", end="", flush=True)
        r = run_one(xbe, args.limit, args.timeout)
        results[title] = r
        if r["status"] == "ok":
            print(f" {r.get('vectors', 0)} vectors, "
                  f"{r.get('mismatches', 0)} mismatches ({r['seconds']}s)")
        else:
            print(f" {r['status']}")
        for fn in r.get("failing", []):
            everywhere[fn].append(title)

    print()
    hdr = f"{'title':<34}{'comparable':>11}{'vectors':>9}{'mismatch':>10}"
    print(hdr)
    print("-" * len(hdr))
    for title, r in results.items():
        if r["status"] != "ok":
            print(f"{title:<34}{r['status']:>30}")
            continue
        print(f"{title:<34}{r.get('comparable', 0):>11}"
              f"{r.get('vectors', 0):>9}{r.get('mismatches', 0):>10}")

    if everywhere:
        print("\nfunctions that disagreed with their own machine code:")
        for fn, titles in sorted(everywhere.items(),
                                 key=lambda kv: (-len(kv[1]), kv[0])):
            where = ", ".join(titles)
            print(f"  {fn:<24} {len(titles)} title(s): {where}")
        print("\nOne that fails in several titles is the same library routine "
              "lifted wrong in all of them.")
    else:
        print("\nno disagreements in the functions sampled -- raise --limit to "
              "sample more")

    out = ROOT / args.out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(results, indent=1), encoding="utf-8")
    print(f"\nwrote {out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
