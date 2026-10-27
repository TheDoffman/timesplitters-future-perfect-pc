"""Replay every capture through two renderers and compare the images.

    py -3 scripts/replay_ab.py --a <replay.exe> --b <replay.exe>
    py -3 scripts/replay_ab.py --a <replay.exe> --b-arg=--backend --b-arg=vulkan
    py -3 scripts/replay_ab.py --a <replay.exe> --b-env RECOMP_D3D8_BACKEND=vulkan --arg=--present

This is the acceptance test the Vulkan work runs on
(docs/technical/vulkan-backend.md, section 5): a capture is deterministic, so
the same frame drawn by two builds, or by two backends of one build, can be
compared pixel for pixel. "Looks right" is not the test -- the frame
brightness investigation is the standing record of what that costs.

Side A and side B are each an executable plus environment overrides; either
may be omitted to mean "the same as the other side". Captures default to every
*.d3dcap under games/_pipeline whose version this build reads (a capture is
game content and is never committed, so there is no fixed corpus).

The report counts byte-identical images. For the rest it gives the share of
pixels that differ, the largest channel difference and the PSNR, so that a
backend which is merely rounding differently can be told from one that drew
the wrong thing. --diff-dir writes an amplified difference image for each.
Exit status is 0 only when every image is identical, or, with --tolerance,
when every image is within it.

--arg (both sides), --a-arg and --b-arg pass arguments on to d3d8_replay; give
each as --arg=--present, since argparse takes a bare leading "--" for one of
its own options. --present compares the frame as it reaches the swap chain as
well as the scene, which is the only way the display resolve is covered. An
older replay build that does not know an argument fails every capture, so pass
it to both sides only when both builds have it.
"""
import argparse
import glob
import math
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MIN_READ, MAX_READ = 5, 6          # D3D8CAP_VERSION_MIN_READ .. D3D8CAP_VERSION


def capture_version(path):
    with open(path, "rb") as f:
        head = f.read(12)
    if len(head) < 12 or head[:7] != b"XBRD3D8":
        return None
    return struct.unpack_from("<I", head, 8)[0]


def read_bmp(path):
    """24-bit BMP -> (width, height, rows top-down as bytes of BGR)."""
    d = Path(path).read_bytes()
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0]
    if bpp != 24:
        raise ValueError(f"{path}: {bpp}-bit BMP, expected 24")
    stride = (w * 3 + 3) & ~3
    top_down = h < 0
    h = abs(h)
    rows = []
    for y in range(h):
        sy = y if top_down else h - 1 - y
        rows.append(d[off + sy * stride: off + sy * stride + w * 3])
    return w, h, rows


def compare(pa, pb):
    wa, ha, ra = read_bmp(pa)
    wb, hb, rb = read_bmp(pb)
    if (wa, ha) != (wb, hb):
        return {"size": f"{wa}x{ha} vs {wb}x{hb}"}
    differ = 0
    worst = 0
    sq = 0
    for a, b in zip(ra, rb):
        if a == b:
            continue
        for i in range(0, len(a), 3):
            d0 = abs(a[i] - b[i])
            d1 = abs(a[i + 1] - b[i + 1])
            d2 = abs(a[i + 2] - b[i + 2])
            m = max(d0, d1, d2)
            if m:
                differ += 1
                worst = max(worst, m)
                sq += d0 * d0 + d1 * d1 + d2 * d2
    n = wa * ha
    mse = sq / (n * 3) if n else 0
    psnr = float("inf") if mse == 0 else 10 * math.log10(255 * 255 / mse)
    return {"differ": differ, "pixels": n, "worst": worst, "psnr": psnr}


def write_diff(pa, pb, out):
    wa, ha, ra = read_bmp(pa)
    _, _, rb = read_bmp(pb)
    stride = (wa * 3 + 3) & ~3
    body = bytearray()
    for y in range(ha - 1, -1, -1):          # bottom-up
        a, b = ra[y], rb[y]
        row = bytearray(min(255, abs(a[i] - b[i]) * 16) for i in range(len(a)))
        body += row + b"\0" * (stride - len(row))
    hdr = struct.pack("<2sIHHI", b"BM", 54 + len(body), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, wa, ha, 1, 24, 0, len(body), 2835, 2835, 0, 0)
    Path(out).write_bytes(hdr + info + bytes(body))


def replay(exe, env_over, extra, capture, prefix):
    env = dict(os.environ)
    env.update(env_over)
    r = subprocess.run([exe, capture, "--out", prefix, "--quiet"] + extra, env=env,
                       capture_output=True, text=True, errors="replace", timeout=180)
    out = sorted(glob.glob(prefix + "*.bmp"))
    return r.returncode, out, (r.stderr or r.stdout).strip().splitlines()[-1:]


def parse_env(pairs):
    env = {}
    for p in pairs or []:
        k, _, v = p.partition("=")
        env[k] = v
    return env


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--a", help="side A replay executable")
    ap.add_argument("--b", help="side B replay executable (default: side A's)")
    ap.add_argument("--a-env", action="append", metavar="KEY=VALUE")
    ap.add_argument("--b-env", action="append", metavar="KEY=VALUE")
    ap.add_argument("--arg", action="append", default=[], metavar="ARG",
                    help="pass ARG to both sides' d3d8_replay (write it as --arg=--present)")
    ap.add_argument("--a-arg", action="append", default=[], metavar="ARG")
    ap.add_argument("--b-arg", action="append", default=[], metavar="ARG")
    ap.add_argument("--captures", nargs="*", help="capture files (default: games/_pipeline/**)")
    ap.add_argument("--diff-dir", help="write amplified difference images here")
    ap.add_argument("--tolerance", type=float, default=None,
                    help="pass when PSNR is at least this many dB (default: exact only)")
    ap.add_argument("--keep", help="keep both sides' images in this directory")
    args = ap.parse_args()

    exe_a = args.a or args.b
    exe_b = args.b or args.a
    exe_a = exe_a and os.path.abspath(exe_a)
    exe_b = exe_b and os.path.abspath(exe_b)
    if not exe_a:
        ap.error("give --a and/or --b")
    env_a, env_b = parse_env(args.a_env), parse_env(args.b_env)

    caps = args.captures or sorted(glob.glob(str(ROOT / "games" / "_pipeline" / "**" / "*.d3dcap"),
                                             recursive=True))
    caps = [c for c in caps if (capture_version(c) or 0) in range(MIN_READ, MAX_READ + 1)]
    if not caps:
        print("no readable captures found")
        return 2

    work = Path(args.keep) if args.keep else Path(tempfile.mkdtemp(prefix="replay_ab_"))
    work.mkdir(parents=True, exist_ok=True)
    if args.diff_dir:
        Path(args.diff_dir).mkdir(parents=True, exist_ok=True)

    identical = within = failed = 0
    rows = []
    for i, cap in enumerate(caps):
        tag = f"{i:03d}_" + Path(cap).stem
        rc_a, imgs_a, tail_a = replay(exe_a, env_a, args.arg + args.a_arg, cap,
                                      str(work / ("a_" + tag)))
        rc_b, imgs_b, tail_b = replay(exe_b, env_b, args.arg + args.b_arg, cap,
                                      str(work / ("b_" + tag)))
        rel = os.path.relpath(cap, ROOT)
        if rc_a or rc_b or not imgs_a or len(imgs_a) != len(imgs_b):
            failed += 1
            rows.append((rel, f"FAILED  a: rc {rc_a} {len(imgs_a)} img {tail_a}  "
                              f"b: rc {rc_b} {len(imgs_b)} img {tail_b}"))
            continue
        worst_psnr = float("inf")
        notes = []
        for ia, ib in zip(imgs_a, imgs_b):
            if Path(ia).read_bytes() == Path(ib).read_bytes():
                continue
            c = compare(ia, ib)
            if "size" in c:
                worst_psnr = -1
                notes.append(c["size"])
                continue
            worst_psnr = min(worst_psnr, c["psnr"])
            notes.append(f"{100.0 * c['differ'] / c['pixels']:.2f}% px differ, "
                         f"max {c['worst']}, PSNR {c['psnr']:.1f} dB")
            if args.diff_dir:
                write_diff(ia, ib, str(Path(args.diff_dir) / (tag + "_" + Path(ia).name)))
        if not notes:
            identical += 1
        elif args.tolerance is not None and worst_psnr >= args.tolerance:
            within += 1
            rows.append((rel, "within tolerance: " + "; ".join(notes)))
        else:
            rows.append((rel, "DIFFERS: " + "; ".join(notes)))
    for rel, text in rows:
        print(f"{rel}: {text}")
    total = len(caps)
    print(f"\n{identical} of {total} identical"
          + (f", {within} within {args.tolerance} dB" if args.tolerance is not None else "")
          + f", {failed} failed to replay"
          + ("" if args.keep else f"  (images in {work})"))
    ok = identical + within == total
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
