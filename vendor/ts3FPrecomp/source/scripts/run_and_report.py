#!/usr/bin/env python3
"""
Run a recompiled title and summarise how far it got.

The bring-up loop is run, read stderr, fix, regenerate, run again -- and the
only question that matters each time is whether the frontier moved. Scrolling a
few thousand lines of kernel trace to answer that is what makes the loop slow.

    python3 scripts/run_and_report.py build/Debug/your_game_recomp.exe

Writes the full stderr next to the summary so nothing is lost, and prints the
handful of numbers worth comparing between runs: how many kernel calls ran, how
many indirect calls were dispatched, which ones failed, any callee-saved ABI
violations (needs -DRECOMP_ABI_CHECK), and where it stopped.

Compare against a previous run to see whether a change helped:

    python3 scripts/run_and_report.py game.exe --baseline runs/run7.err
"""

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path


def run(exe: Path, seconds: float, out_dir: Path, tag: str, profile=None,
        kernel_log=None):
    """Run the executable for a bounded time, capturing stdout and stderr."""
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{tag}.out"
    err_path = out_dir / f"{tag}.err"

    with open(out_path, "wb") as out, open(err_path, "wb") as err:
        started = time.time()
        env = dict(os.environ)
        if profile:
            # The interval is in calls. The profiler reports as it goes because
            # a run that faults or is killed never reaches atexit, and this one
            # always faults.
            env["RECOMP_TRACE_PROFILE"] = str(profile)
            # The stderr report is a top-40 list. That answers "where do the
            # calls go" and cannot answer "did this function ever run", which
            # is what bring-up asks -- an initialiser that should have run and
            # did not is simply absent from a top-40 of 25,000. Reading absence
            # out of that list looks like evidence and is not, so take the
            # whole table too.
            env["RECOMP_PROFILE_DUMP"] = str((out_dir / f"{tag}.prof").resolve())
        if kernel_log is not None:
            env["RECOMP_KERNEL_LOG_BUDGET"] = str(kernel_log)
        proc = subprocess.Popen([str(exe)], stdout=out, stderr=err,
                                cwd=str(exe.parent), env=env)
        try:
            proc.wait(timeout=seconds)
            status = f"exited with code {proc.returncode}"
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            status = f"still running after {seconds:.0f}s (killed)"
        elapsed = time.time() - started

    # The profiler only prints a running total every `profile` calls, so the
    # highest number in the log undershoots the truth by up to one interval.
    # Two runs sampled at different intervals are therefore not comparable, and
    # nothing in the .err records which interval produced it -- so note it here.
    (out_dir / f"{tag}.meta").write_text(
        "\n".join([f"profile_interval={profile or 0}",
                    f"kernel_budget={kernel_log if kernel_log is not None else 200}",
                    f"seconds={seconds:.0f}",
                    # Switches that change what the program does, not just what
                    # it prints. RECOMP_VBLANK gates the whole frame-delivery
                    # chain, so a run without it is a different program -- and
                    # comparing one against a run with it reads as "the change
                    # did nothing".
                    "switches=" + ",".join(
                        # With the value, where it is more than a flag: 60 Hz
                        # and 120 Hz are different runs.
                        (k if os.environ.get(k) == "1" else f"{k}={os.environ.get(k)}")
                        for k in ("RECOMP_VBLANK", "RECOMP_PB_EXEC",
                                    "RECOMP_PB_SCAN", "RECOMP_NV2A_TRACE",
                                    # Without these DirectSoundCreate fails,
                                    # and the title crashes much later in
                                    # DownloadEffectsImage on a null object.
                                    "RECOMP_AC97_READY", "RECOMP_APU_DSP_ACK",
                                    # Host drawing, the frame clock and the
                                    # profilers: a frame-rate comparison
                                    # between runs is meaningless unless
                                    # these match, and RECOMP_SAMPLE costs a
                                    # little itself.
                                    "RECOMP_HLE_D3D8", "RECOMP_VBLANK_HZ",
                                    "RECOMP_VBLANK_CLOCK", "RECOMP_FPS",
                                    "RECOMP_SAMPLE")
                        if os.environ.get(k)) or "switches=none",
                    f"exe={exe}", ""]),
        encoding="utf-8")

    return err_path, status, elapsed


def summarise(err_path: Path) -> dict:
    text = err_path.read_text(encoding="utf-8", errors="replace")

    icall_totals = [int(m) for m in re.findall(r"total calls: (\d+)", text)]
    # Both log forms: the old "Failed to resolve VA", and the one the runtime
    # prints now, "[ICALL] unresolved call|jump target". Matching only the old
    # one reported "unresolved icalls 0" for a Mortal Kombat run in which one
    # jump target was refused a billion times.
    failed = sorted(set(re.findall(r"Failed to resolve VA (0x[0-9A-Fa-f]+)", text))
                    | set(re.findall(r"\[ICALL\] unresolved (?:call |jump )?target "
                                     r"(0x[0-9A-Fa-f]+)", text)))
    abi = re.findall(r"^\[ABI\] (sub_[0-9A-Fa-f]+):(.*)$", text, re.M)

    # The profiler reports "[PROFILE] N functions entered" periodically, so a
    # run that is killed or faults still leaves one behind. Take the highest.
    reached = [int(m) for m in re.findall(r"\[PROFILE\] (\d+) functions entered", text)]

    # Still climbing, or settled?
    #
    # "Why did it stop" is the wrong question for a title that has not
    # stopped, and the two look identical in a single number. Burnout 2's
    # count plateaus for a long stretch, resumes, and was still rising when
    # every run so far was killed -- so several careful investigations were
    # measuring a title cut off mid-flight and reading the snapshot as a
    # final state.
    #
    # Compare the last tenth of the samples against the previous tenth: a
    # title that is still loading gains functions there, one that has settled
    # does not.
    growing = None
    if len(reached) >= 20:
        tail = max(2, len(reached) // 10)
        recent = reached[-tail:]
        earlier = reached[-2 * tail:-tail]
        growing = max(recent) - max(earlier)
    table_full = "table full" in text

    crash = None
    m = re.search(r"^\[CRASH\][^\n]*\n(?:[^\n]*\n){0,4}?\s*in (sub_[0-9A-Fa-f]+\+0x[0-9A-Fa-f]+)",
                  text, re.M)
    if m:
        crash = m.group(1)
    fault = re.search(r"Xbox VA of fault: (0x[0-9A-Fa-f]+)", text)

    meta = err_path.with_suffix(".meta")
    interval = 0
    budget = 0
    switches = ""

    if meta.is_file():
        meta_text = meta.read_text(encoding="utf-8")
        m = re.search(r"profile_interval=([0-9]+)", meta_text)
        if m:
            interval = int(m.group(1))
        m = re.search(r"kernel_budget=([0-9]+)", meta_text)
        if m:
            budget = int(m.group(1))
        m = re.search(r"switches=(\S*)", meta_text)
        if m:
            switches = m.group(1)

    return {
        "profile_interval": interval,
        "lines": text.count("\n"),
        "kernel_calls": len(re.findall(r"\[KERNEL\] #", text)),
        "kernel_budget": budget,
        "switches": switches,
        "icalls": max(icall_totals) if icall_totals else 0,
        "icall_failures": failed,
        "abi_violations": abi,
        "crash_in": crash,
        "fault_va": fault.group(1) if fault else None,
        "exited_cleanly": "HalReturnToFirmware" in text,
        "files_opened": len(set(re.findall(r"\[PATH\] (\S+)", text))),
        "functions_reached": max(reached) if reached else 0,
        "still_growing": growing,
        "table_full": table_full,
    }


def show(label: str, current, baseline=None):
    if baseline is None:
        print(f"  {label:<22} {current}")
        return
    if isinstance(current, int) and isinstance(baseline, int):
        delta = current - baseline
        arrow = "" if delta == 0 else (f"  ({delta:+d})")
        print(f"  {label:<22} {current}{arrow}   [was {baseline}]")
    else:
        same = " (unchanged)" if current == baseline else ""
        print(f"  {label:<22} {current}{same}")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe", type=Path, help="The built executable")
    ap.add_argument("--seconds", type=float, default=40.0,
                    help="How long to let it run (default 40)")
    ap.add_argument("--out-dir", type=Path, default=Path("runs"),
                    help="Where to keep the captured logs (default runs/)")
    ap.add_argument("--tag", default=None,
                    help="Name for this run (default: the next free runNN)")
    ap.add_argument("--profile", type=int, nargs="?", const=100, default=None,
                    metavar="EVERY",
                    help="Run with the entry profiler on, reporting every N "
                         "function calls (default 100). Needs a build lifted "
                         "with --trace-all-entries. The interval only sets how "
                         "often a running total is printed; this reads the "
                         "highest. Keep it small while a title dies early -- a "
                         "run that faults before the first report leaves none, "
                         "and a crash never reaches atexit. The runtime caps "
                         "reports at one per second regardless, so a small "
                         "interval no longer slows the title.")
    ap.add_argument("--kernel-log", type=int, default=None, metavar="N",
                    help="How many kernel calls to log (RECOMP_KERNEL_LOG_BUDGET). "
                         "The default of 200 is a budget, not a limit on the "
                         "title -- a run that reaches it reports 200 whatever "
                         "it actually did, which reads like a measurement and "
                         "is not one. Raise it when asking what the title "
                         "called the kernel for.")
    ap.add_argument("--baseline", type=Path, default=None,
                    help="A previous .err to compare against")
    args = ap.parse_args()

    if not args.exe.is_file():
        print(f"error: no such executable: {args.exe}", file=sys.stderr)
        return 1

    tag = args.tag
    if tag is None:
        n = 1
        while (args.out_dir / f"run{n:02d}.err").exists():
            n += 1
        tag = f"run{n:02d}"

    print(f"running {args.exe.name} for up to {args.seconds:.0f}s ...")
    err_path, status, elapsed = run(args.exe, args.seconds, args.out_dir, tag,
                                    profile=args.profile,
                                    kernel_log=args.kernel_log)
    print(f"  {status} after {elapsed:.1f}s")
    print(f"  stderr -> {err_path}")
    prof = err_path.with_suffix(".prof")
    if prof.is_file():
        print(f"  every function entered -> {prof}")

    now = summarise(err_path)
    before = summarise(args.baseline) if args.baseline and args.baseline.is_file() else None

    print("\nfrontier")
    if before and before["switches"] != now["switches"]:
        print(f"      RUN DIFFERENTLY from the baseline: switches "
              f"{before['switches'] or 'none'} vs {now['switches'] or 'none'}.")
        print("      These change what the title does, not just what it logs,")
        print("      so the differences below are not attributable to a change.")
    if now["functions_reached"] or (before and before["functions_reached"]):
        show("functions reached", now["functions_reached"],
             before["functions_reached"] if before else None)
        if now["profile_interval"]:
            print(f"      (sampled every {now['profile_interval']} calls, so"
                  f" the true count is up to {now['profile_interval'] - 1}"
                  f" higher)")
        if before and before["profile_interval"] != now["profile_interval"]:
            print("      MEASURED DIFFERENTLY from the baseline (every "
                  f"{before['profile_interval'] or '?'} calls vs "
                  f"{now['profile_interval'] or '?'}). The change above is not"
                  " evidence on its own -- re-run at the same interval.")
        if now["still_growing"] is not None:
            if now["still_growing"] > 0:
                print(f"      STILL CLIMBING: +{now['still_growing']} in the last"
                      f" tenth of the run. It had not finished --")
                print("      give it longer before asking why it stopped.")
            else:
                print("      settled: no new functions in the last tenth of the"
                      " run.")
        if now["table_full"]:
            print("      (profiler table filled - the real count is higher)")
    if now["kernel_budget"] and now["kernel_calls"] >= now["kernel_budget"]:
        print(f"      note: the kernel log stopped at its budget of "
              f"{now['kernel_budget']}, so \"kernel calls\" below is that")
        print("      budget and not a count. Re-run with a larger --kernel-log.")
    for key, label in (("kernel_calls", "kernel calls"),
                       ("icalls", "indirect calls"),
                       ("files_opened", "files opened"),
                       ("lines", "stderr lines")):
        show(label, now[key], before[key] if before else None)

    show("unresolved icalls", len(now["icall_failures"]),
         len(before["icall_failures"]) if before else None)
    show("abi violations", len(now["abi_violations"]),
         len(before["abi_violations"]) if before else None)

    print("\nstopped")
    if now["crash_in"]:
        print(f"  crash in              {now['crash_in']}"
              + (f"  reading {now['fault_va']}" if now["fault_va"] else ""))
        if before and before["crash_in"] == now["crash_in"]:
            print("  SAME SITE as the baseline. Two runs stopping at the same")
            print("  event means stop varying the run: compare the two logs and")
            print("  find the earliest divergence instead.")
    elif now["exited_cleanly"]:
        print("  exited via HalReturnToFirmware (the title chose to quit)")
    else:
        print("  no crash recorded - it was still running when time ran out")

    if now["abi_violations"]:
        print("\ncallee-saved violations (the caller is corrupted, not the callee)")
        for name, detail in now["abi_violations"][:10]:
            print(f"  {name}{detail}")

    if now["icall_failures"]:
        print("\nunresolved indirect-call targets")
        print("  " + " ".join(now["icall_failures"][:12]))
        print("  (tools.seed_from_log turns the ones that are code into functions)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
