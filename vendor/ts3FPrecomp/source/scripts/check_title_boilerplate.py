"""Report where a title's recomp_manual.c has drifted from the template's.

`titles/<name>/src/recomp_manual.c` is two things in one file: the title's
own hand-written overrides, which must differ, and a block of runtime
boilerplate copied from `templates/new-game/src/recomp_manual.c`, which must
not. Nothing kept the second half in step, and it drifted.

How that stayed invisible is the point. `recomp_icall_not_code_log` gained a
second parameter, and the generated `recomp_types.h` in every title started
declaring and calling the two-argument form -- but six titles still *defined*
the one-argument form. recomp_manual.c deliberately re-declares the globals it
needs by hand instead of including a generated header, so that a title lifted
against an older header still compiles; the cost is that the compiler never
saw the two declarations of this function together. The calling convention
discards the extra argument, so there was no error, no warning, and no symptom
except that half the library went on reading the caller of a refused indirect
call out of a stale g_esp -- which is the bug the two-argument form exists to
fix, still live in the titles that most needed it, with a comment above it
saying it had been fixed.

A signature change reaching only half the library and saying nothing is worth
one script. This compares the boilerplate functions by name and prints what
differs; --fix copies the template's version over a stale one.

    py -3 scripts/check_title_boilerplate.py
    py -3 scripts/check_title_boilerplate.py --fix

Exits non-zero when anything has drifted, so it can gate a build.
"""

import argparse
import io
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "templates" / "new-game" / "src" / "recomp_manual.c"

# Functions the template owns. A title that edits one of these is doing
# something the template should be doing for every title instead.
SHARED = [
    "recomp_icall_fail_log",
    "recomp_icall_not_code_log",
]

# The extern block, by the section markers every copy carries. It drifted
# worse than the functions did: four titles declared g_eax and g_esp as plain
# externs while xbox_memory_layout.c defines them thread-local, which is not
# a declaration of the same object and which nothing diagnosed. The rest of
# the block -- g_xbox_code_lo/hi, g_icall_saved_esp, g_icall_dispatch_form --
# was simply absent, so the diagnostics that use them could not be copied in
# without the build breaking, which is how the drift stayed put.
REGION_BEGIN = "/* ── Register state (defined in xbox_memory_layout.c) ──────── */"
REGION_END = "/* ── Manual function overrides ─────────────────────────────── */"

# The includes and the macros above them, by the same method. This was missed
# the first time and the omission bit immediately: adding RECOMP_DIAG_LOCK to
# the template gave every title a function that used a macro none of them
# defined. A region that holds what the shared functions depend on has to be
# synced with them or the sync does not build.
PREAMBLE_BEGIN = "#include <stdio.h>"
PREAMBLE_END = "/* ── ICALL trace ring buffer ───────────────────────────────── */"


def read(path):
    text = io.open(path, encoding="utf-8", newline="").read()
    return text, ("\r\n" if "\r\n" in text else "\n")


def extract(path, name):
    """(lines, start, end) for one function including its leading comment.

    The comment matters: it is where the reasons live, and a copy that has
    the code but not the reason behind it is how the next person reintroduces
    the bug. end is exclusive and lands on the closing brace in column zero.
    """
    text, nl = read(path)
    lines = text.split(nl)
    sig = next((i for i, l in enumerate(lines)
                if l.startswith("void " + name + "(")), None)
    if sig is None:
        return None
    # Walk back over the block comment that introduces it.
    start = sig
    while start > 0 and not lines[start - 1].strip() == "":
        start -= 1
        if lines[start].startswith("/*"):
            break
    end = next((i for i, l in enumerate(lines) if i > sig and l == "}"), None)
    if end is None:
        return None
    return lines, start, end + 1


def extract_region(path, begin=REGION_BEGIN, end=REGION_END):
    """(lines, start, end) for the block between two marker lines."""
    text, _nl = read(path)
    lines = text.split(_nl)
    try:
        s = lines.index(begin)
        e = lines.index(end)
    except ValueError:
        return None
    return lines, s, e


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--fix", action="store_true",
                    help="copy the template's version over a drifted one")
    args = ap.parse_args(argv)

    canon = {}
    for name in SHARED:
        got = extract(TEMPLATE, name)
        if got is None:
            print(f"error: {TEMPLATE.name} has no {name}", file=sys.stderr)
            return 2
        lines, s, e = got
        canon[name] = lines[s:e]

    regions = [("the extern block", REGION_BEGIN, REGION_END),
               ("the includes and macros", PREAMBLE_BEGIN, PREAMBLE_END)]
    canon_regions = []
    for label, begin, end in regions:
        got = extract_region(TEMPLATE, begin, end)
        if got is None:
            print(f"error: {TEMPLATE.name} has no markers for {label}",
                  file=sys.stderr)
            return 2
        _lines, _s, _e = got
        canon_regions.append((label, begin, end, _lines[_s:_e]))

    drifted = 0
    for path in sorted((ROOT / "titles").glob("*/src/recomp_manual.c")):
        rel = path.relative_to(ROOT)

        # Regions first: the functions below cannot compile without the
        # declarations and macros they use, so fixing them in the other order
        # just breaks the build. Re-read between regions because --fix writes.
        for label, begin, end, canon_lines in canon_regions:
            got = extract_region(path, begin, end)
            if got is None:
                print(f"{rel}: no markers for {label}, cannot compare")
                drifted += 1
                continue
            lines, s, e = got
            if lines[s:e] == canon_lines:
                continue
            drifted += 1
            print(f"{rel}: {label} differs from the template "
                  f"({e - s} lines here, {len(canon_lines)} there)")
            if args.fix:
                _, nl = read(path)
                lines[s:e] = canon_lines
                io.open(path, "w", encoding="utf-8",
                        newline="").write(nl.join(lines))
                print(f"  fixed: copied the template's {label}")

        for name in SHARED:
            got = extract(path, name)
            if got is None:
                print(f"{rel}: {name} missing entirely")
                drifted += 1
                continue
            lines, s, e = got
            if lines[s:e] == canon[name]:
                continue
            drifted += 1
            print(f"{rel}: {name} differs from the template "
                  f"({e - s} lines here, {len(canon[name])} there)")
            if args.fix:
                _, nl = read(path)
                lines[s:e] = canon[name]
                io.open(path, "w", encoding="utf-8",
                        newline="").write(nl.join(lines))
                print(f"  fixed: copied the template's {name}")

    if not drifted:
        print("every title's boilerplate matches the template")
        return 0
    if args.fix:
        print(f"\nfixed {drifted}. Rebuild the titles you changed -- and check "
              f"they have the includes the new code needs, which this does "
              f"not add for you.")
        return 0
    print(f"\n{drifted} drifted. Run with --fix, or reconcile by hand if the "
          f"title's version is the one that is right -- in which case the "
          f"template is what should change.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
