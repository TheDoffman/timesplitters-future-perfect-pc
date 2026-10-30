"""Turn the executor's unhandled-method list into a named work queue.

    python3 scripts/pb_unhandled.py runs/run.err

Capture the input with:

    RECOMP_PB_EXEC=1 RECOMP_PB_UNHANDLED_ALL=1 ./game.exe 2> runs/pb.err

The executor reports "[GPU]   0xNNNN xCOUNT" lines. On its own that is a list
of numbers; matched against nv2a_regs.h it becomes a list of decisions, and
sorted into buckets it becomes a plan.
"""
import collections
import pathlib
import re
import sys

REGS = pathlib.Path(__file__).resolve().parent.parent / "src" / "nv2a" / "nv2a_regs.h"

# Methods that are genuinely nothing to do here: there is no command FIFO to
# idle and no scanout to sync against, so carrying them out means returning.
NOOP = {"NV097_NO_OPERATION", "NV097_WAIT_FOR_IDLE", "NV097_SET_FLIP_READ",
        "NV097_BREAK_VERTEX_BUFFER_CACHE", "NV097_INVALIDATE_VERTEX_CACHE_FILE",
        "NV097_INVALIDATE_VERTEX_FILE", "NV097_SET_ZSTENCIL_CLEAR_VALUE",
        "NV097_SET_ANTI_ALIASING_CONTROL"}

# State this rasteriser has no use for, but which a D3D11 backend would.
INERT = re.compile(r"FOG|STIPPLE|LOGIC_OP|DITHER|SHADOW|OCCLUSION|"
                   r"POLY_OFFSET|LINE_WIDTH|POINT_SIZE|EDGE_FLAG|SWATH")

# State that decides where or whether a pixel lands.
PIXELS = re.compile(r"VIEWPORT|CLIP|SURFACE|COLOR_MASK|BLEND|ALPHA|CULL|"
                    r"DEPTH|ZSTENCIL|STENCIL|TEXTURE|COMBINER|TRANSFORM|"
                    r"VERTEX_DATA|BEGIN_END|CLEAR")


def load_names():
    names = {}
    if not REGS.is_file():
        return names
    for line in REGS.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"#\s*define\s+(NV097_[A-Z0-9_]+)\s+0x([0-9A-Fa-f]+)", line)
        if m:
            names.setdefault(int(m.group(2), 16), m.group(1))
    return names


def main():
    err = pathlib.Path(sys.argv[1])
    names = load_names()
    counts = collections.Counter()
    for m, c in re.findall(r"\[GPU\]\s+0x([0-9A-Fa-f]{4})\s+x(\d+)", err.read_text(
            encoding="utf-8", errors="replace")):
        counts[int(m, 16)] = max(counts[int(m, 16)], int(c))

    if not counts:
        print("no unhandled-method lines found "
              "(run with RECOMP_PB_UNHANDLED_ALL=1)")
        return 1

    buckets = {"nothing to do": [], "affects pixels": [],
               "inert state": [], "unnamed": []}
    for method, count in counts.most_common():
        name = names.get(method)
        if name is None:
            # Multi-dword commands occupy consecutive slots: SET_TRANSFORM_
            # PROGRAM is 32 of them, SET_VIEWPORT_OFFSET is four. Reported
            # one per slot they drown the list, and they are one decision.
            base = max((a for a in names if a <= method and method - a < 0x80),
                       default=None)
            if base is not None:
                name = names[base] + f"+{method - base}"
        if name is None:
            buckets["unnamed"].append((method, count, f"0x{method:04X}"))
        elif name in NOOP:
            buckets["nothing to do"].append((method, count, name))
        elif INERT.search(name):
            buckets["inert state"].append((method, count, name))
        elif PIXELS.search(name):
            buckets["affects pixels"].append((method, count, name))
        else:
            buckets["inert state"].append((method, count, name))

    total = sum(counts.values())
    print(f"{len(counts)} distinct methods, {total} occurrences\n")
    for label in ("affects pixels", "nothing to do", "inert state", "unnamed"):
        rows = buckets[label]
        if not rows:
            continue
        n = sum(r[1] for r in rows)
        print(f"-- {label}: {len(rows)} methods, {n} occurrences "
              f"({100.0*n/total:.0f}%)")
        for method, count, name in rows[:18]:
            print(f"     0x{method:04X}  x{count:<6} {name}")
        if len(rows) > 18:
            print(f"     ... and {len(rows)-18} more")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
