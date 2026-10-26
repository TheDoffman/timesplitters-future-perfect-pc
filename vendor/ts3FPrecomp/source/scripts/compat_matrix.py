#!/usr/bin/env python3
"""
Run every title and print one table of how far each one got.

The bring-up loop has always been one title at a time, and so has every
judgement about whether a change helped: the NtFreeVirtualMemory fix, the
LOCK-prefix flag fix and the XDK 3925 work were each validated against a
single game. "Did this regress Burnout 2" was not a question anyone could
answer without half an hour of manual runs, so it mostly went unasked.

This asks it. Each title is built, run headless for a fixed time, and reduced
to the handful of numbers that say where its frontier is:

    title            boot  device  swaps  draws  skipped  tex refused  icalls
    burnout2          yes     yes   1204   8817        0            0       0
    timesplitters2    yes     yes    903  12044       31            0       2
    maxpayne          yes     yes    122    239        0            0       2
    mkda              yes     yes      0      0        0            0       2
    ...

With --baseline it prints the same table against a previous run's JSON and
marks every column that moved, which is the actual question: not "how good is
this title" but "did what I just changed break one of the others".

    py -3 scripts/compat_matrix.py --refresh-thunks --build --seconds 45
    py -3 scripts/compat_matrix.py --baseline runs/matrix-before.json

Titles are discovered from titles/*/src/main.c, which records the game
directory and entry point. A title with no built executable is reported as
such rather than skipped, because "it stopped building" is a regression too.

These are windowed programs: a run opens a window per title, in sequence.
"""

import argparse
import contextlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

CMAKE_CANDIDATES = [
    Path("C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/Common7"
         "/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"),
    Path("cmake"),
]


def find_cmake():
    for c in CMAKE_CANDIDATES:
        if c.name == "cmake" or c.exists():
            return str(c)
    return "cmake"


def discover(only=None):
    """Every title project, with the game directory its main.c points at."""
    out = []
    for main_c in sorted((ROOT / "titles").glob("*/src/main.c")):
        name = main_c.parent.parent.name
        if only and name not in only:
            continue
        text = main_c.read_text(encoding="utf-8", errors="replace")
        m = re.search(r'YOUR_GAME_DIR\s+"([^"]*)"', text)
        game = None
        if m:
            game = m.group(1).replace("\\\\", "/").replace("\\", "/").split("games/")[-1]
        out.append({
            "name": name,
            "project": main_c.parent.parent,
            "game": game,
            "exe": main_c.parent.parent / "build" / "Release" / f"{name}_recomp.exe",
            "pipeline": ROOT / "games" / "_pipeline" / name / "out",
            "game_dir": (ROOT / "games" / game) if game else None,
        })
    return out


def refresh_thunks(t, verbose=False):
    """Rewrite gen/recomp_hle.c against the current src/hle.

    Adding an HLE_EXPORT or HLE_ORIGINAL leaves every already-lifted title
    unable to link until its thunk file mentions the new name. Without this
    the matrix cannot run at all after an HLE change, which is exactly when
    it is most wanted.
    """
    funcs = t["pipeline"] / "disasm" / "functions.json"
    ident = t["pipeline"] / "func_id" / "identified_functions.json"
    xbe = ROOT / "games" / (t["game"] or "") / "default.xbe"
    syms = xbe.with_name("default_xdk_symbols.json")
    gen = t["project"] / "src" / "recomp" / "gen"
    if not (funcs.is_file() and xbe.is_file() and gen.is_dir()):
        return "no pipeline output"
    cmd = [sys.executable, "-m", "tools.recomp", str(xbe), "--game-only",
           "--split", "1000", "--functions", str(funcs),
           "--gen-dir", str(gen), "--only-hle-thunks"]
    if ident.is_file():
        cmd += ["--identified", str(ident)]
    if syms.is_file():
        cmd += ["--hle-symbols", str(syms)]
    manual = t["project"] / "src" / "recomp_manual.c"
    if manual.is_file():
        cmd += ["--exclude-manual", str(manual)]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        # The refresh declines when it cannot produce a working thunk file --
        # say which, rather than a bare failure. That message is the whole
        # answer ("lift it again"), so losing it costs a debugging round.
        for line in reversed(r.stderr.splitlines()):
            if line.startswith("Not rewriting"):
                return "needs a re-lift: " + line.split(": ", 1)[-1][:70]
        return "refresh failed"
    m = re.search(r"(\d+) of (\d+) original bodies", r.stderr)
    return f"{m.group(1)}/{m.group(2)} bodies" if m else "refreshed"


def build(t):
    bdir = t["project"] / "build"
    if not (bdir / "CMakeCache.txt").is_file():
        return "not configured"
    r = subprocess.run([find_cmake(), "--build", str(bdir), "--config", "Release",
                        "--target", f"{t['name']}_recomp", "--", "-m", "-v:m"],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        errs = [l for l in (r.stdout + r.stderr).splitlines()
                if " error " in l.lower()]
        return "BUILD FAILED: " + (errs[0][:90] if errs else "see log")
    return "built"


MARKERS = {
    # Two sources for the swap count, and the second is the reliable one.
    #
    # The shadow summary prints five seconds after the *first* swap, so a
    # title whose first frame arrives near the end of the window renders and
    # still counts zero -- Outrun 2 was reported as not rendering while the
    # SEGA screen was on the monitor. RECOMP_FPS counts from process start and
    # says so every window, whether or not anything has been drawn yet.
    "swaps":      (r"shadow: (\d+) swaps", max),
    # summed, not maxed: each line counts that window alone.
    "fps_swaps":  (r"\[FPS\] t=[^(]*\((\d+) swaps\)", sum),
    "clears":     (r"shadow: \d+ swaps, (\d+) clears", max),
    "kernel":     (r"\[KERNEL\] summary: (\d+) total calls", max),
    "tex_binds":  (r"shadow textures: (\d+) binds", max),
    "tex_refused": (r"skipped \d+ not a texture, \d+ cube/volume, (\d+) format", max),
}

DRAW_RE = re.compile(
    r"draws: (\d+) UP \+ (\d+) indexed UP \+ (\d+) buffer \+ (\d+) indexed buffer drawn; "
    r"skipped (\d+) program without layout, (\d+) declaration shader, (\d+) unknown "
    r"shader, (\d+) stride, (\d+) primitive, (\d+) failed")


def summarise(err_text, exit_code, seconds):
    s = {k: 0 for k in MARKERS}
    for key, (pat, agg) in MARKERS.items():
        vals = [int(v) for v in re.findall(pat, err_text)]
        s[key] = agg(vals) if vals else 0
    drawn = skipped = 0
    for m in DRAW_RE.finditer(err_text):
        g = [int(x) for x in m.groups()]
        drawn = max(drawn, sum(g[:4]))
        skipped = max(skipped, sum(g[4:]))
    if s.get("fps_swaps", 0) > s.get("swaps", 0):
        s["swaps"] = s["fps_swaps"]
    s["draws"] = drawn
    s["draws_skipped"] = skipped
    s["boot"] = "Loaded" in err_text and "sections" in err_text
    s["device"] = ("shadow device" in err_text
                   or "Direct3D_CreateDevice" in err_text)
    # Both wordings. A title built before the runtime renamed this line reports
    # the old one, and matching only the new one made the count read zero --
    # Dino Crisis 3 showed icalls=0 in this table while its log held 4,494
    # "Failed to resolve VA" lines, which is a clean bill of health issued to
    # the title whose unresolved calls were the thing being looked for.
    s["icalls"] = len(set(
        re.findall(r"(?:unresolved (?:call |jump )?target|Failed to resolve VA) "
                   r"0x([0-9A-Fa-f]{8})", err_text)))
    s["exit"] = exit_code
    # An NT exception code is an exit status only in the sense that the process
    # had one. TimeSplitters 2 returned 0xC0000005 and this table called it the
    # best-performing title in the library, because it rendered 98.5% of a
    # frame before faulting and nothing looked at the code.
    s["crashed"] = isinstance(exit_code, int) and exit_code >= 0x80000000
    s["seconds"] = seconds
    # Verdict, coarsest first: the point is to spot a title falling off a
    # rung, not to grade it.
    #
    # "no frames" names the window it did not produce them in, because for a
    # slow starter that is the whole story rather than a verdict. Max Payne
    # takes about 45 seconds to reach its first frame and renders 122 of them
    # after that; measured for exactly 45 seconds it reports zero, which reads
    # as a regression and is an artefact of the clock. The MvC2 notes record
    # six runs lost to the same mistake, and this harness reproduced it on its
    # first outing.
    # The swap *count* comes from a summary printed five seconds after the
    # first swap, so a title whose first frame lands near the end of the
    # window renders and still counts zero. The one-off "Swap on guest thread"
    # notice fires immediately, and separates "never rendered" from "rendered
    # too late to measure" -- Max Payne reached its first frame at about 45
    # seconds once and at about 95 the next time, so for it the difference is
    # the whole answer rather than a footnote.
    s["swapped"] = "shadow: Swap on guest thread" in err_text
    if s["crashed"]:
        # Said first and said loudly. How far it got before faulting is in the
        # other columns; a fault is a regression however much of a frame
        # arrived first, and it outranks every other thing this can report.
        s["verdict"] = "CRASHED 0x%08X" % s["exit"]
    elif s["swaps"] > 0:
        s["verdict"] = "renders"
    elif s["swapped"]:
        s["verdict"] = f"first frame late (>{seconds - 5}s)"
    elif s["device"]:
        s["verdict"] = f"no frames in {seconds}s"
    elif s["boot"]:
        s["verdict"] = "boots"
    else:
        s["verdict"] = "no start"
    return s


def _bmp_lit(path):
    """Percent of one BMP that is not black, or None if it cannot be read."""
    import struct
    try:
        data = path.read_bytes()
        off = struct.unpack("<I", data[10:14])[0]
        px = data[off:]
        if len(px) < 3:
            return None
        total = len(px) // 3
        # An all-black frame is the case this exists to catch and also the
        # common one, so answer it with a C-speed byte count instead of three
        # hundred thousand slice comparisons per file.
        if px.count(0) == len(px):
            return 0.0
        lit = sum(1 for i in range(0, len(px) - 3, 3)
                  if px[i:i + 3] != b"\x00\x00\x00")
        return round(100.0 * lit / total, 1)
    except (OSError, struct.error, IndexError):
        return None


def frame_motion(shots_dir, name):
    """How many of the captured frames differ from the one before, as a
    percentage of the comparisons made -- or None when there are too few.

    Pixels are not motion, which is the same lesson as "swaps are not pixels"
    one step further on and was learned the same way: by getting it wrong.
    Dino Crisis 3 was read as frozen because its indexed draw count stopped
    moving at 153792 while it went on swapping. It had in fact moved from 3D
    geometry to one full-screen quad per frame, and the contents of that quad
    changed every frame. Nothing in this table could say so -- `lit` reports
    that something is on screen and is perfectly happy with a still image --
    so the diagnosis rested on a draw counter, which is a proxy for progress
    and not a good one.

    Comparing whole frames, because that is the question: a title at a static
    menu scores 0 and a title playing anything scores high, and neither needs
    a draw call to be interpreted.
    """
    shots = sorted(shots_dir.glob(name + "*.bmp")) if shots_dir.is_dir() else []
    if len(shots) < 2:
        return None
    import hashlib
    import struct
    digests = []
    for f in shots:
        try:
            data = f.read_bytes()
            off = struct.unpack("<I", data[10:14])[0]
            digests.append(hashlib.sha1(data[off:]).hexdigest())
        except (OSError, struct.error, IndexError):
            pass
    if len(digests) < 2:
        return None
    moved = sum(1 for a, b in zip(digests, digests[1:]) if a != b)
    return round(100.0 * moved / (len(digests) - 1), 1)


def frame_lit(shots_dir, name):
    """Percent of the *best* dumped host frame that is not black, or None.

    A title can present thousands of frames and show nothing: Tony Hawk's Pro
    Skater 2X draws once per frame into a surface that never reaches the
    screen, so it swaps at 30 fps and every pixel stays 0. Counting swaps
    called that "renders". Looking at the pixels does not.

    Black is the honest test rather than a checksum, because the failure being
    caught is specifically "nothing arrived": a frame that is 0.0% lit is not
    rendering whatever the swap counter says, and Burnout 2 and TimeSplitters
    2 come back at 100% and 98% on the same measurement.

    The best frame and not the last one, because which frame the runtime
    happens to capture is arbitrary and a title is entitled to a black one:
    a fade, a load, a transition between screens. "Did anything ever arrive"
    is the question, and the maximum answers it from wherever in the run the
    captures land -- which is what makes it safe to capture them often enough
    for a title that only presents a hundred frames to be measured at all.
    """
    shots = sorted(shots_dir.glob(name + "*.bmp")) if shots_dir.is_dir() else []
    seen = [v for v in (_bmp_lit(f) for f in shots) if v is not None]
    return (max(seen) if seen else None), len(seen)


# Titles whose front end wants something other than the default. Keyed by
# project name; the value is a RECOMP_INPUT_SEQ string used verbatim.
TITLE_INPUT = {}


@contextlib.contextmanager
def fresh_save_data(game_dir, enabled=True):
    """Run with no save profile present, then put the player's back.

    A driven run presses A through the front end, and a title that already has
    a save meets that with "overwrite?" -- so the script is answering a
    question nobody meant to ask, on a screen the measurement was not aiming
    at, and the run measures the save dialog instead of the game.

    Nothing is deleted, ever. The existing UDATA is renamed aside, the title
    makes its own during the run, that one is moved into a scratch directory
    afterwards, and the original is renamed back. A crash mid-run leaves the
    original under its aside name rather than losing it, and the restore
    refuses to clobber anything it did not move itself.
    """
    if not enabled or not game_dir or not game_dir.is_dir():
        yield
        return
    live = game_dir / "UDATA"
    stamp = time.strftime("%Y%m%d-%H%M%S")
    aside = game_dir / f"UDATA.matrix-aside-{stamp}"
    moved = False

    # Put back anything a previous run failed to restore, before doing
    # anything else.
    #
    # A restore can fail because Windows holds the directory until the title
    # process is fully gone. When that happened the next run found no UDATA at
    # all, so it moved nothing aside, and the player's profiles stayed under
    # the old aside name while the title made fresh ones -- one failure turned
    # into the saves being quietly displaced for every run after it. Healing
    # first makes a failed restore cost one run instead of all of them.
    for stale in sorted(game_dir.glob("UDATA.matrix-aside-*")):
        try:
            if live.is_dir():
                scratch = ROOT / "games" / "_pipeline" / "_matrix" / "udata-scratch"
                scratch.mkdir(parents=True, exist_ok=True)
                dest = scratch / f"{game_dir.name}-orphan-{stale.name[-15:]}"
                if not dest.exists():
                    live.rename(dest)
            if not live.exists():
                stale.rename(live)
                print(f"   recovered save data left behind by an earlier run:"
                      f" {stale.name} -> UDATA")
        except OSError:
            pass
    try:
        if live.is_dir():
            if aside.exists():
                # Someone else's aside, or a previous crash. Leave it alone
                # and leave the save alone with it.
                yield
                return
            live.rename(aside)
            moved = True
        yield
    finally:
        if moved:
            scratch = ROOT / "games" / "_pipeline" / "_matrix" / "udata-scratch"
            # Windows keeps a directory locked until the process that was
            # using it has fully gone, and killing a title does not make that
            # instant. The first version of this renamed once and gave up on
            # the exception, which left a player's save profiles sitting under
            # the aside name with nothing in UDATA -- exactly the outcome the
            # whole move-aside dance exists to avoid.
            #
            # So retry, and if it still cannot be put back, say so loudly and
            # name the directory to rename by hand. Never silent, and never
            # deleted.
            for attempt in range(40):          # ~10s
                try:
                    if live.is_dir():
                        scratch.mkdir(parents=True, exist_ok=True)
                        dest = scratch / f"{game_dir.name}-{stamp}"
                        if not dest.exists():
                            live.rename(dest)
                    if not live.exists():
                        aside.rename(live)
                    break
                except OSError:
                    time.sleep(0.25)
            if not live.exists() and aside.exists():
                print(f"!! COULD NOT RESTORE SAVE DATA for {game_dir.name}."
                      f" Rename {aside} back to {live} by hand."
                      f" Nothing has been deleted.", file=sys.stderr)


def default_input_seq(seconds):
    """Press start a few times, then A, spread across the measured window.

    A title left alone is not a title being played, and the difference is not
    cosmetic: TimeSplitters 2 faults on 10 runs out of 10 when it is left on
    its start screen for 75 seconds and 0 out of 10 when something walks it
    through the menus. Every measurement this harness had ever taken was the
    idle path, so it spent two days reporting an attract-mode crash as a
    property of the title, and a bisect across three commits found nothing
    because all three were measuring the same thing.

    Proportions rather than fixed times, so the path scales with --seconds:
    start while the title is still booting, then A often enough to walk a menu
    without running off the end of the window.
    """
    ms = seconds * 1000
    steps = ["%d:start" % int(ms * f) for f in (0.17, 0.23, 0.29, 0.35)]
    steps += ["%d:a" % int(ms * f)
              for f in (0.42, 0.50, 0.58, 0.66, 0.74, 0.82, 0.90, 0.96)]
    return ",".join(steps)


def run_title(t, seconds, out_dir, extra_env=None):
    if not t["exe"].is_file():
        return {"verdict": "not built", "exit": None, "boot": False,
                "device": False, "swaps": 0, "draws": 0, "draws_skipped": 0,
                "tex_binds": 0, "tex_refused": 0, "icalls": 0, "kernel": 0,
                "clears": 0, "seconds": None, "lit": None,
                "shots": 0, "motion": None, "crashed": False,
                "runs": "0/0"}
    env = dict(os.environ)
    env.setdefault("RECOMP_VBLANK", "1")
    env.setdefault("RECOMP_AC97_READY", "1")
    # Count frames from process start rather than from the first swap.
    env.setdefault("RECOMP_FPS", "5")
    # Swaps are not pixels. Tony Hawk's Pro Skater 2X presents 1,638 frames in
    # a minute and every one of them is black: it draws once per frame into a
    # surface that never reaches the screen, and this table called it
    # "renders" for a day. Dump host frames and look at them.
    shots = out_dir / "frames"
    shots.mkdir(exist_ok=True)
    for old in shots.glob(f"{t['name']}*.bmp"):
        old.unlink()
    if t.get("input_seq"):
        env.setdefault("RECOMP_INPUT_SEQ", t["input_seq"])
    env.setdefault("RECOMP_HLE_D3D8_DUMP", str(shots / t["name"]))
    # Every 20 swaps, not 150: six of eleven titles came back with no frame
    # captured at all, so "renders" went unverified on half the table while
    # reading exactly like a pass. Max Payne presents around 130 frames in its
    # window and Outrun 2 fewer, which never reached 150. The runtime caps how
    # many files it writes, so this costs a fast title nothing.
    env.setdefault("RECOMP_HLE_D3D8_DUMP_EVERY", "20")
    # ...and at 20 a fast title fills all 24 files in its first 480 swaps.
    # Outrun 2's were all intro logos while it spent the last minute of the
    # run in a race. Keep 12 from the start and let the other 12 roll, so the
    # frames show where the run ended up as well as how it began.
    env.setdefault("RECOMP_HLE_D3D8_DUMP_KEEP_LAST", "12")
    env.update(extra_env or {})
    err_path = out_dir / f"{t['name']}.err"
    with open(err_path, "wb") as errf, fresh_save_data(t.get("game_dir"),
                                                       t.get("fresh_saves")):
        p = subprocess.Popen([str(t["exe"])], cwd=str(t["project"]),
                             stdout=subprocess.DEVNULL, stderr=errf, env=env)
        try:
            code = p.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            code = "timeout"
    text = err_path.read_text(encoding="utf-8", errors="replace")
    s = summarise(text, code, seconds)
    # How many frames that rests on, because 0.0 from 24 captures and 0.0
    # from 6 are not the same claim. A title only reaching 134 swaps in its
    # window gets 6 and cannot be sampled harder; one presenting 3800 gets the
    # full 24, and a black verdict on that means something.
    s["lit"], s["shots"] = frame_lit(shots, t["name"])
    s["motion"] = frame_motion(shots, t["name"])
    # Presenting frames that are entirely black is not rendering, and saying
    # so is the whole reason this column exists.
    if s["verdict"] == "renders" and s["lit"] == 0.0:
        s["verdict"] = "black screen"
    s["log"] = str(err_path.relative_to(ROOT))
    return s


def run_repeated(t, seconds, out_dir, repeat):
    """Run a title `repeat` times and report the worst run, plus the spread.

    One run per title is only honest for a title that behaves the same way
    every time, and at least one does not: TimeSplitters 2 faults about half
    the runs, so a single sample reports "renders" or "CRASHED 0xC0000005"
    with equal confidence and no way to tell which is the title. That produced
    three wrong conclusions in two days -- a regression pair that was the tool
    changing, an exit code dismissed as a one-off, and a crash declared fixed
    by a merge when five clean runs in a row had simply been luck.

    The worst run, because a title that crashes half the time has a crash, and
    reporting the good half hides it. `runs` carries the detail the verdict
    cannot: "2/5" against a crash verdict says flaky, "5/5" says broken, and
    those want different work.
    """
    if repeat <= 1:
        s = run_title(t, seconds, out_dir)
        s["runs"] = "1/1"
        return s

    results = [run_title(t, seconds, out_dir) for _ in range(repeat)]
    worst = min(results, key=lambda r: verdict_rank(r.get("verdict")))
    same = sum(1 for r in results
               if verdict_rank(r.get("verdict"))
               == verdict_rank(worst.get("verdict")))
    worst["runs"] = f"{same}/{repeat}"
    # Keep the best lit/motion seen: a title that rendered once can render.
    for key in ("lit", "motion"):
        seen = [r.get(key) for r in results if r.get(key) is not None]
        if seen:
            worst[key] = max(seen)
    return worst


# (name, minimum width, which direction is better). The width is widened to
# the header itself below, because a column narrower than its own name runs
# into the next one and the table stops being readable.
COLUMNS = [("verdict", 18, None), ("swaps", 8, "higher"), ("draws", 9, "higher"),
           ("draws_skipped", 8, "lower"), ("tex_binds", 10, "higher"),
           ("tex_refused", 8, "lower"), ("icalls", 7, "lower"),
           ("lit", 6, "higher"), ("motion", 7, "higher"),
           ("shots", 6, None), ("runs", 6, None),
           ("exit", 9, None), ("kernel", 11, None)]


# Bumped whenever a change in this file alters what a verdict or a counter
# means. Comparing across it compares two different measurements, and every
# such comparison so far has read as a regression in the title: reading the
# best capture instead of the last one moved Dino Crisis 3 from "black screen"
# to "renders" and Max Payne the other way, and surfacing the exit code turned
# TimeSplitters 2 from "renders" into a crash it had been doing all along.
# None of those three titles changed.
#   1  swaps and draws only
#   2  lit column, from the last capture, dumped every 150 swaps
#   3  lit from the best capture every 20 swaps, shot count, crash verdicts
#   4  motion: whether the picture changes between captures at all
#   5  titles are driven through their front end instead of left idle,
#      each starting with no save profile present
METHOD = 5


# How far a title got, worst to best. A verdict change is only a regression
# when the rank falls: "not built -> boots" is the opposite of one, and
# reporting it as a regression is how a report stops being read.
VERDICT_RANK = [
    ("build failed", 0), ("not built", 0), ("CRASHED", 1), ("no start", 1),
    ("boots", 2),
    ("black screen", 3), ("no frames", 3), ("first frame late", 4),
    ("renders", 5),
]


def verdict_rank(v):
    for prefix, rank in VERDICT_RANK:
        if (v or "").startswith(prefix):
            return rank
    return -1


def counter_regressed(col, cur, was):
    """A drop worth reporting, rather than run-to-run noise.

    Frame counts vary by a frame or two between identical runs -- flagging
    TimeSplitters 2 for 2368 -> 2367 trains the reader to ignore the list,
    which costs more than the one real regression it was built to catch. Ten
    per cent, and at least two, before it counts.
    """
    if not isinstance(cur, int) or not isinstance(was, int):
        return False
    delta = was - cur
    if delta <= 0:
        return False
    return delta >= 2 and delta * 10 >= was


def cell(col, value):
    """One column's text.

    The exit code gets printed the way it is read. A fault arrives here as
    3221225477, which is 0xC0000005 written in the one base nobody recognises
    it in -- and at ten digits it was also one wider than its column, so it ran
    into the number beside it and 98.5 + 3221225477 read as a single figure.
    That is how a crashing title was misread as the best result in the table.
    """
    if col == "exit" and isinstance(value, int) and value >= 0x80000000:
        return "0x%08X" % value
    return str(value)


def print_table(rows, baseline=None, baseline_method=None):
    # Width from the widest thing that will actually be printed, not from a
    # number guessed when the column was added. Every column is declared with
    # a minimum, every value gets at least one space in front of it, and no
    # value can push into its neighbour whatever it turns out to be.
    widths = {c: max(w, len(c) + 1) for c, w, _ in COLUMNS}
    for s in rows.values():
        for col, _w, _ in COLUMNS:
            widths[col] = max(widths[col], len(cell(col, s.get(col, 0))) + 1)
    head = f"{'title':<16}" + "".join(f"{c:>{widths[c]}}" for c, _w, _ in COLUMNS)
    print(head)
    print("-" * len(head))
    # A baseline measured by an older version of this script is not a
    # baseline, it is a different question asked of the same titles. Say so
    # once, loudly, and keep its verdict changes out of the regression list
    # rather than reporting the tool's own repairs as the library breaking.
    comparable = baseline_method == METHOD
    regressions = []
    improvements = []
    incomparable = []
    for name, s in rows.items():
        line = f"{name:<16}"
        for col, _w, better in COLUMNS:
            cur = s.get(col, 0)
            line += f"{cell(col, cur):>{widths[col]}}"
            if baseline and name in baseline and better:
                was = baseline[name].get(col, 0)
                if isinstance(cur, int) and isinstance(was, int) and cur != was:
                    worse = (counter_regressed(col, cur, was)
                             if better == "higher"
                             else counter_regressed(col, was, cur))
                    line += "!" if worse else ("+" if cur > was else "-")
                    if worse:
                        regressions.append(f"{name}.{col}: {was} -> {cur}")
                else:
                    line += " "
            elif baseline:
                line += " "
        if baseline and name in baseline:
            before = baseline[name].get("verdict")
            if s.get("verdict") != before:
                fell = verdict_rank(s.get("verdict")) < verdict_rank(before)
                line += f"   [{'was' if fell else 'up from'} {before}]"
                if not comparable:
                    incomparable.append(
                        f"{name}: {before} -> {s.get('verdict')}")
                elif fell:
                    regressions.append(
                        f"{name}: {before} -> {s.get('verdict')}")
                else:
                    improvements.append(
                        f"{name}: {before} -> {s.get('verdict')}")
        print(line)
    if baseline:
        print()
        if not comparable:
            print(f"MEASURED BY A DIFFERENT TOOL: this run is method "
                  f"{METHOD}, the baseline method {baseline_method}. What a "
                  f"verdict means changed between them, so a verdict that "
                  f"moved may be this script correcting itself rather than "
                  f"the title doing anything different. Re-run the baseline "
                  f"before trusting any of it as a regression.")
            print()
        windows = {r.get("seconds") for r in rows.values() if r.get("seconds")}
        was = {b.get("seconds") for b in baseline.values() if b.get("seconds")}
        if windows and was and windows != was:
            print(f"MEASURED DIFFERENTLY: this run gave each title "
                  f"{sorted(windows)}s, the baseline {sorted(was)}s. A title "
                  f"that starts slowly reports fewer frames for that reason "
                  f"alone -- the columns below are not comparable.")
            print()
        if improvements:
            print("improved:")
            for r in improvements:
                print(f"  {r}")
            print()
        if incomparable:
            print("verdicts that moved (NOT comparable, see above):")
            for r in incomparable:
                print(f"  {r}")
        elif regressions:
            print("REGRESSED:")
            for r in regressions:
                print(f"  {r}")
        else:
            print("nothing regressed against the baseline")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--fresh-saves", action="store_true",
                    help="move each title's UDATA aside for the run so a "
                         "driven run meets a first-time front end instead of "
                         "an overwrite prompt, and put it back afterwards. "
                         "OFF by default: Windows holds the directory until "
                         "the title process is fully gone, the restore can "
                         "therefore fail, and a failed restore leaves a "
                         "player's profiles under an aside name while the "
                         "title makes fresh ones. That happened twice in one "
                         "afternoon. Nothing is ever deleted either way, but "
                         "not touching save data is the safer default and the "
                         "overwrite prompt is a smaller problem than losing "
                         "track of someone's saves")
    ap.add_argument("--mute", action="store_true",
                    help="run every title silently (RECOMP_MUTE=1): the audio "
                         "device still opens and plays at its own pace, so "
                         "timing is the same as an audible run")
    ap.add_argument("--idle", action="store_true",
                    help="do not press anything. The old behaviour, and worth "
                         "having: TimeSplitters 2 crashes 10/10 idle and 0/10 "
                         "driven, so the two paths are different measurements "
                         "and an attract-mode fault is only visible in this "
                         "one")
    ap.add_argument("--repeat", type=int, default=1, metavar="N",
                    help="run each title N times and report the worst, with "
                         "how many runs agreed. Anything below 5 cannot see a "
                         "fault that happens half the time, which is the rate "
                         "TimeSplitters 2 actually faults at")
    ap.add_argument("--seconds", type=int, default=45,
                    help="how long to run each title (default 45)")
    ap.add_argument("--titles", help="comma-separated subset")
    ap.add_argument("--refresh-thunks", action="store_true",
                    help="rewrite each gen/recomp_hle.c first (needed after "
                         "any HLE_EXPORT or HLE_ORIGINAL change)")
    ap.add_argument("--build", action="store_true", help="build before running")
    ap.add_argument("--no-run", action="store_true",
                    help="refresh and/or build only")
    ap.add_argument("--out-dir", default="games/_pipeline/_matrix",
                    help="where per-title logs and the JSON go")
    ap.add_argument("--baseline", help="a previous run's JSON to compare against")
    args = ap.parse_args()
    if args.mute:
        os.environ["RECOMP_MUTE"] = "1"      # inherited by every title run

    only = set(args.titles.split(",")) if args.titles else None
    titles = discover(only)
    if not titles:
        print("no titles found under titles/*/src/main.c", file=sys.stderr)
        return 1
    out_dir = ROOT / args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    baseline = None
    baseline_method = None
    if args.baseline:
        bp = Path(args.baseline)
        if not bp.is_absolute():
            bp = ROOT / bp
        if bp.is_file():
            loaded = json.loads(bp.read_text(encoding="utf-8"))
            baseline = loaded.get("titles")
            # Absent means it predates the field, which is method 1 or 2 --
            # either way not this one.
            baseline_method = loaded.get("method")
        else:
            print(f"baseline {bp} not found; running without one", file=sys.stderr)

    rows = {}
    for t in titles:
        prefix = f"[{t['name']}]"
        if args.refresh_thunks:
            print(f"{prefix} thunks: {refresh_thunks(t)}", flush=True)
        if args.build:
            r = build(t)
            print(f"{prefix} build: {r}", flush=True)
            if r.startswith("BUILD FAILED"):
                rows[t["name"]] = {"verdict": "build failed", "swaps": 0,
                                   "draws": 0, "draws_skipped": 0,
                                   "tex_binds": 0, "tex_refused": 0,
                                   "icalls": 0, "kernel": 0, "note": r}
                continue
        if args.no_run:
            continue
        t0 = time.time()
        if not args.idle:
            t["input_seq"] = TITLE_INPUT.get(t["name"],
                                             default_input_seq(args.seconds))
        t["fresh_saves"] = args.fresh_saves
        rows[t["name"]] = run_repeated(t, args.seconds, out_dir, args.repeat)
        print(f"{prefix} {rows[t['name']]['verdict']} "
              f"({time.time() - t0:.0f}s)", flush=True)

    if args.no_run:
        return 0

    print()
    print_table(rows, baseline, baseline_method)

    stamp = time.strftime("%Y%m%d-%H%M%S")
    jpath = out_dir / f"matrix-{stamp}.json"
    jpath.write_text(json.dumps({"seconds": args.seconds, "repeat": args.repeat,
                                 "idle": args.idle, "method": METHOD,
                                 "titles": rows},
                                indent=1), encoding="utf-8")
    latest = out_dir / "matrix-latest.json"
    latest.write_text(jpath.read_text(encoding="utf-8"), encoding="utf-8")
    print(f"\nwrote {jpath.relative_to(ROOT)} (and matrix-latest.json)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
