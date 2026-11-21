"""Bounded real-game renderer acceptance through the unchanged Windows launcher.

Requires the normal native game and launcher builds, and the Test Lab fixture.
No personal saves, settings or shader caches are used. Capture and measurement
runs are separate; CPU samples are process CPU time, not GPU/display timing.
"""
import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
import platform
import re
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from project_paths import configure
configure()
from testing_profile import XBE_SHA256, check_fixture, contained, prepare_run, recycle_disks


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def link_file(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_symlink():
        raise ValueError("Refusing a redirected test file")
    if target.exists():
        if os.path.samefile(source, target):
            return
        target.unlink()  # Only managed code/data files under the isolated install.
    os.link(source, target)


def prepare_install(run):
    install = contained(ROOT, run / "install", "private/test-saves")
    for source in (ROOT / "private/game").rglob("*"):
        if source.is_file():
            target = contained(ROOT, install / "Data" / source.relative_to(ROOT / "private/game"), "private/test-saves")
            link_file(source, target)
    for source in [ROOT / "build/windows/Release/futureperfect_recomp.exe",
                   *(ROOT / "build/windows/Release").glob("*.dll")]:
        link_file(source, contained(ROOT, install / "Bin" / source.name, "private/test-saves"))
    contained(ROOT, install / "Settings", "private/test-saves").mkdir(exist_ok=True)
    controls = contained(ROOT, install / "Settings/pc-controls.json", "private/test-saves")
    if not controls.exists():
        shutil.copyfile(ROOT / "tests/fixtures/pc-controls.json", controls)
    contained(ROOT, install / "Launch.ps1", "private/test-saves").write_text("# Isolated test marker; launcher starts the native EXE.\n")
    contained(ROOT, install / "BUILD.json", "private/test-saves").write_text(json.dumps(dict(
        version="v0.8.0-dev", graphics_backends=["d3d11", "vulkan"],
        graphics_default="vulkan", auto_display_resolution=True, display_settings_version=5)))
    return install


def copy_saves(run, install):
    source = contained(ROOT, run / "saves", "private/test-saves")
    for entry in source.rglob("*"):
        contained(ROOT, entry, "private/test-saves")
        if entry.is_file():
            target = contained(ROOT, install / "Saves" / entry.relative_to(source), "private/test-saves")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(entry, target)


def compile_harness():
    framework = Path(os.environ["WINDIR"]) / "Microsoft.NET/Framework64/v4.0.30319"
    launcher = ROOT / "build/launcher/Bin/TimeSplitters.App.exe"
    directory = ROOT / "build/renderer-validation"
    directory.mkdir(parents=True, exist_ok=True)
    # Keep opt-in test executables out of all release package input folders.
    shutil.copy2(launcher, directory / launcher.name)
    output = directory / "renderer-acceptance.exe"
    refs = ["System.dll", "System.Core.dll", "System.Xaml.dll", "System.Drawing.dll", str(launcher)]
    refs += [str(framework / "WPF" / name) for name in ("PresentationCore.dll", "PresentationFramework.dll", "WindowsBase.dll")]
    subprocess.run([str(framework / "csc.exe"), "/nologo", "/target:exe", "/platform:x64",
                    *["/reference:" + ref for ref in refs], "/out:" + str(output),
                    str(ROOT / "tests/launcher_lifecycle/RendererGameSmoke.cs")], check=True)
    subprocess.run([str(output), "--self-test-io"], check=True, timeout=10)
    return output


def stop_owned_game(install):
    # Native PowerShell compares resolved executable paths before stopping only
    # this sandbox's child. No personal process command lines are collected.
    env = dict(os.environ, TSFP_ACCEPTANCE_GAME=str(install / "Bin/futureperfect_recomp.exe"))
    command = "$target=[IO.Path]::GetFullPath($env:TSFP_ACCEPTANCE_GAME); Get-Process -Name futureperfect_recomp -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $target} | ForEach-Object {if(!$_.HasExited){$_.Kill();$_.WaitForExit()}}"
    subprocess.run(["powershell", "-NoProfile", "-Command", command], env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=30)


def compile_capture():
    # GDI can retain the initial redirected bitmap for Vulkan independent flip.
    # This helper captures only the path-verified game's rectangle via DXGI.
    from native_compiler import native_compiler
    folder = ROOT / "build/renderer-validation"
    folder.mkdir(parents=True, exist_ok=True)
    compiler, env = native_compiler(folder)
    output = folder / "renderer-window-capture.exe"
    subprocess.run([compiler, "/nologo", "/EHsc", "/std:c++17",
                    str(ROOT / "tests/launcher_lifecycle/RendererWindowCapture.cpp"),
                    "/Fe:" + str(output), "/Fo:" + str(folder / "renderer-window-capture.obj"),
                    "/link", "d3d11.lib", "dxgi.lib", "windowsapp.lib",
                    "windowscodecs.lib", "ole32.lib", "user32.lib"],
                   env=env, cwd=folder, check=True, timeout=60)
    return output


def compile_unavailable_dxc():
    from native_compiler import native_compiler
    folder = ROOT / "build/renderer-validation"
    folder.mkdir(parents=True, exist_ok=True)
    compiler, env = native_compiler(folder)
    output = folder / "unavailable-dxc.dll"
    subprocess.run([compiler, "/nologo", "/LD",
                    str(ROOT / "tests/launcher_lifecycle/UnavailableDxc.c"),
                    "/Fe:" + str(output), "/Fo:" + str(folder / "unavailable-dxc.obj")],
                   cwd=folder, env=env, check=True, timeout=60)
    return output


def measurements(samples, expected_scene=2):
    active = [sample for sample in samples if 85 <= sample["seconds"] <= 115 and sample["scene"] == expected_scene
              and not sample.get("flags", 0) & 0x20000000]
    if len(active) < 20:
        raise ValueError("Insufficient matched gameplay samples")
    elapsed = active[-1]["seconds"] - active[0]["seconds"]
    cpu = (active[-1]["cpu_total_ms"] - active[0]["cpu_total_ms"]) / (elapsed * 1000)
    gpu = [sample["gpu_dedicated_bytes"] for sample in active if sample.get("gpu_dedicated_bytes") is not None]
    return dict(window_seconds=[active[0]["seconds"], active[-1]["seconds"]],
                cpu_core_equivalents=cpu, machine_cpu_percent=cpu * 100 / os.cpu_count(),
                gpu_dedicated_bytes_peak=max(gpu) if gpu else None,
                private_bytes_peak=max(sample["private_bytes"] for sample in active),
                working_set_bytes_peak=max(sample["working_set_bytes"] for sample in active),
                handles_range=[min(sample["handles"] for sample in active), max(sample["handles"] for sample in active)],
                guest_ticks_per_second=(active[-1]["ticks"] - active[0]["ticks"]) / elapsed)


def effective_settings(text, config):
    scene = re.search(r"\[GRAPHICS\] Scene (\d+)x(\d+), MSAA (\d+) requested / (\d+) active", text)
    expected = (config["width"], config["height"], config["msaa"], config["msaa"])
    automatic = config["width"] == config["height"] == 1
    observed = tuple(map(int, scene.groups())) if scene else None
    checks = dict(scene_resolution_and_msaa=bool(observed) and
                  ((observed[0] >= 640 and observed[1] >= 480 and observed[2:] == expected[2:])
                   if automatic else observed == expected))
    if config["aa"] == 2:
        checks["smaa_enabled"] = "[SMAA] enabled:" in text and "[SMAA] unavailable" not in text
    else:
        checks["smaa_disabled"] = "[SMAA] enabled:" not in text
    return checks


def sustained_checks(samples, seconds, expect_pause=False, expected_scene=2):
    early = [s for s in samples if 85 <= s["seconds"] <= 115]
    late = [s for s in samples if seconds - 30 <= s["seconds"] <= seconds]
    active = [s for s in late if s["scene"] == expected_scene and not s["flags"] & 0x20000000
              and s.get("position") is not None]
    checks = dict(late_gameplay=len(active) >= 40 and len(active) >= .9 * len(late))
    checks["late_clock_60hz"] = bool(active) and 59 <= ((active[-1]["ticks"] - active[0]["ticks"]) /
        max(.001, active[-1]["seconds"] - active[0]["seconds"])) <= 61
    checks["late_movement"] = bool(active) and any(sum((a-b)**2 for a,b in
        zip(s["position"],active[0]["position"])) > 1 for s in active)
    if expect_pause:
        paused = [s for s in samples if s.get("paused") and s["scene"] == expected_scene]
        checks["pause_observed"] = len(paused) >= 3
        checks["resumed_after_pause"] = bool(paused) and any(
            not s.get("paused",True) and s["seconds"] > paused[-1]["seconds"] + 1 for s in active)
    growth = {}
    for key, limit in (("private_bytes",128*1024*1024),("handles",32),("gpu_dedicated_bytes",128*1024*1024)):
        before=[s[key] for s in early if s.get(key) is not None]
        after=[s[key] for s in late if s.get(key) is not None]
        growth[key] = statistics.median(after)-statistics.median(before) if before and after else None
        if key != "gpu_dedicated_bytes" or growth[key] is not None:
            checks[key+"_bounded_growth"] = growth[key] is not None and growth[key] <= limit
    return dict(checks=checks, median_growth=growth, late_samples=len(active),
                scope="Last30s active input/ticks and bounded resource growth; not proof against slower leaks")


def scenario_options(path, seconds):
    if path is None:
        return {}
    if path.stat().st_size > 65536:
        raise ValueError("Scenario exceeds 64 KiB")
    document = json.loads(path.read_text(encoding="utf-8"))
    name = document.get("scenario")
    actions = document.get("input")
    if not isinstance(name, str) or not 1 <= len(name) <= 128 or any(ord(c) < 32 for c in name):
        raise ValueError("Scenario needs a bounded descriptive name")
    if not isinstance(actions, list) or not 1 <= len(actions) <= 64:
        raise ValueError("Scenario needs 1..64 scheduled keyboard actions")
    previous = -1
    for action in actions:
        if not isinstance(action, dict):
            raise ValueError("Invalid scheduled action")
        at, key, hold = action.get("at"), action.get("key"), action.get("hold_ms", 180)
        if (type(at) not in (int, float) or not math.isfinite(at) or not 1 <= at < seconds or at - previous < .5 or
            type(key) is not int or not 1 <= key <= 255 or type(hold) is not int or not 50 <= hold <= 500):
            raise ValueError("Scheduled actions need ordered times, valid keys and bounded holds")
        previous = at
    expected = document.get("expected_weapon", 14)
    skip = document.get("skip_intro", True)
    pause = document.get("pause_expected", False)
    scene = document.get("expected_scene",2)
    if type(expected) is not int or not -1 <= expected <= 255 or type(skip) is not bool or type(pause) is not bool or type(scene) is not int or not 1 <= scene <= 99:
        raise ValueError("Invalid scenario expectations")
    if pause and seconds < 300:
        raise ValueError("Pause expectation requires an extended scenario")
    return dict(scenario=name, input=actions, expected_weapon=expected, expected_scene=scene, skip_intro=skip, pause_expected=pause)


def pinned_presentmon():
    pin = json.loads((ROOT / "config/build/presentmon-windows.json").read_text())
    path = ROOT / "external/presentmon" / pin["version"] / Path(pin["url"]).name
    if path.stat().st_size != pin["size"] or digest(path) != pin["sha256"]:
        raise ValueError("PresentMon does not match the verified standalone console pin")
    return path


def interval_statistics(values):
    if len(values) < 100:
        raise ValueError("Insufficient ETW gameplay frames")
    percentiles = statistics.quantiles(values, n=100, method="inclusive")
    return dict(frames=len(values), mean_ms=statistics.mean(values), median_ms=statistics.median(values),
                p95_ms=percentiles[94], p99_ms=percentiles[98], max_ms=max(values),
                frames_over_25ms=sum(value > 25 for value in values),
                frames_over_50ms=sum(value > 50 for value in values))


def trace_measurements(path, samples, pid, expected_scene=2):
    active = [sample for sample in samples if 85 <= sample["seconds"] <= 115 and sample["scene"] == expected_scene
              and not sample.get("flags", 0) & 0x20000000]
    if len(active) < 20:
        raise ValueError("Insufficient QPC-aligned gameplay samples")
    start, end = active[0]["clock_qpc_ms"], active[-1]["clock_qpc_ms"]
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or any(int(row["ProcessID"]) != pid for row in rows):
        raise ValueError("ETW trace is missing or contains another process")
    matched = [row for row in rows if start <= float(row["CPUStartQPCTimeInMs"]) <= end]
    chains = sorted(set(row["SwapChainAddress"] for row in matched))
    if len(chains) != 1:
        raise ValueError("ETW gameplay window has missing or ambiguous swap chains")
    def values(column):
        return [float(row[column]) for row in matched if row.get(column) not in (None, "", "NA") and float(row[column]) > 0]
    gpu = values("MsGPUBusy")
    submitted = interval_statistics(values("MsBetweenPresents"))
    observed_fps = len(matched) * 1000 / (end - start)
    if abs(observed_fps / (1000 / submitted["mean_ms"]) - 1) > .2:
        raise ValueError("ETW row coverage disagrees with its reported present intervals")
    return dict(scope="PID-filtered PresentMon ETW; submitted and displayed intervals, QPC-aligned gameplay window",
                qpc_window_ms=[start, end], matched_frames=len(matched),
                swap_chains=chains,
                submitted=submitted, observed_frame_rate=observed_fps,
                displayed=interval_statistics(values("MsBetweenDisplayChange")),
                gpu_busy=interval_statistics(gpu) if len(gpu) >= 100 else None,
                display_latency=interval_statistics(values("MsUntilDisplayed")),
                present_modes=sorted(set(row["PresentMode"] for row in matched)))


def wait_with_trace(process, destination, seconds):
    profiler = None
    output = None
    session = None
    deadline = time.monotonic() + seconds + 60
    try:
        while process.poll() is None:
            if profiler is None and (destination / "game-pid.txt").exists():
                pid = int((destination / "game-pid.txt").read_text())
                session = "TSFP-launcher-" + str(pid) + "-" + uuid.uuid4().hex
                (destination / "presentmon-session.json").write_text(json.dumps(dict(pid=pid, session=session)) + "\n")
                output = (destination / "presentmon.log").open("wb")
                profiler = subprocess.Popen([str(pinned_presentmon()), "--process_id", str(pid),
                    "--session_name", session, "--output_file", str(destination / "presentmon.csv"),
                    "--no_console_stats", "--no_track_input", "--qpc_time_ms", "--timed", str(seconds + 2),
                    "--terminate_after_timed", "--terminate_on_proc_exit"],
                    stdout=output, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            if time.monotonic() >= deadline:
                raise subprocess.TimeoutExpired("launcher-and-etw", seconds + 60)
            time.sleep(.2)
        if profiler is None:
            raise ValueError("Launcher did not expose its owned game PID for tracing")
        if process.returncode == 0:
            if profiler.wait(timeout=15) != 0:
                raise ValueError("PresentMon failed; inspect presentmon.log")
        return process.returncode
    finally:
        if profiler is not None and profiler.poll() is None:
            try:
                cleanup = subprocess.run([str(pinned_presentmon()), "--session_name", session, "--terminate_existing_session"],
                    capture_output=True, text=True, timeout=5, creationflags=subprocess.CREATE_NO_WINDOW)
                (destination / "presentmon-cleanup.json").write_text(json.dumps(dict(
                    session=session, exit_code=cleanup.returncode, output=cleanup.stdout + cleanup.stderr)) + "\n")
            finally:
                if profiler.poll() is None:
                    profiler.kill()
                profiler.wait(timeout=10)
        if output is not None:
            output.close()


def run_case(executable, install, run, evidence, backend, phase, args):
    label = backend + "-" + phase
    destination = evidence / label
    destination.mkdir(parents=True, exist_ok=True)
    result_path = destination / (backend + "-result.json")
    if result_path.exists():
        raise ValueError("Evidence already exists; choose a fresh --label (no stale pass reuse)")
    copy_saves(run, install)
    config = dict(backend=backend, seconds=args.seconds, capture=args.capture,
                  width=args.width, height=args.height, msaa=args.msaa, aa=args.aa,
                  anisotropy=args.anisotropy, fullscreen=args.fullscreen)
    config["display_cycle"] = getattr(args, "display_cycle", False)
    config.update(scenario_options(getattr(args, "scenario_file", None), args.seconds))
    if getattr(args,"unavailable_dxc",False):
        config["expected_backend"] = "d3d11"
    if args.capture:
        config["capture_helper"] = str(executable.parent / "renderer-window-capture.exe")
    if getattr(args, "capture_times", None):
        config["capture_times"] = args.capture_times
    config_path = destination / "case.json"
    config_path.write_text(json.dumps(config, indent=2) + "\n")
    cache = contained(ROOT, run / "cache" / args.label / backend, "private/test-saves")
    cache.mkdir(parents=True, exist_ok=True)
    before = {str(path.relative_to(cache)): path.stat().st_size for path in cache.rglob("*") if path.is_file()}
    if phase == "cold" and before:
        raise ValueError("Cold case requires a new, empty isolated cache")
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith(("TSFP_", "RECOMP_"))}
    env.update(APPDATA=str(cache), LOCALAPPDATA=str(cache))
    started = time.monotonic()
    process = subprocess.Popen([str(executable), str(install), str(destination), str(config_path)],
                               cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    def drain():
        with (destination / "harness.log").open("wb") as output:
            remaining = 4 * 1024 * 1024
            while data := process.stdout.read1(65536):
                output.write(data[:remaining])
                output.flush()
                remaining = max(0, remaining - len(data))
    reader = threading.Thread(target=drain, daemon=True)
    reader.start()
    try:
        status = wait_with_trace(process, destination, args.seconds) if getattr(args, "presentmon", False) else process.wait(timeout=args.seconds + 60)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        reader.join(timeout=10)
        stop_owned_game(install)
        recycle_disks(ROOT, install)
    if status or not result_path.exists():
        raise ValueError("Real launcher/game case failed; inspect " + str(destination / "harness.log"))
    result = json.loads(result_path.read_text())
    spec = importlib.util.spec_from_file_location("native_refresh_acceptance", ROOT / "tests/gameplay/native_refresh_game.py")
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    text = (destination / (backend + ".err")).read_text(errors="replace")
    result["renderer_checks"] = native.renderer_checks(text, config.get("expected_backend",backend))
    if getattr(args,"unavailable_dxc",False):
        del result["renderer_checks"]["no_fallback"]
        result["renderer_checks"]["expected_compiler_recovery"] = (
            "DXC compiler (dxcompiler) was not found" in text and
            "the vulkan renderer could not start; using d3d11" in text)
    result["present_checks"] = native.native_present_checks(text)
    result["effective_settings_checks"] = effective_settings(text, config)
    if config["display_cycle"]:
        result["effective_settings_checks"]["display_cycle_completed"] = result.get("display_cycle_completed", False)
        if config["width"] == config["height"] == 1:
            resized = next(snapshot for snapshot in result["display_snapshots"] if snapshot["label"] == "resized")
            expected = f"[PC-DISPLAY] Native scene target {resized['client_width']}x{resized['client_height']}"
            result["effective_settings_checks"]["automatic_resize_applied"] = expected in text
    samples = json.loads((destination / (backend + "-samples.json")).read_text())
    expected_scene=config.get("expected_scene",2)
    result["measurements"] = measurements(samples, expected_scene)
    if args.seconds >= 300:
        result["sustained"] = sustained_checks(samples, args.seconds, config.get("pause_expected",False), expected_scene)
    if getattr(args, "presentmon", False):
        trace = destination / "presentmon.csv"
        if trace.stat().st_size > 32 * 1024 * 1024:
            raise ValueError("ETW CSV exceeded the 32 MiB session budget")
        result["etw"] = trace_measurements(trace, samples, result["pid"], expected_scene)
    result["cache_before"] = before
    result["cache_after"] = {str(path.relative_to(cache)): path.stat().st_size for path in cache.rglob("*") if path.is_file()}
    result["cache_read_logged"] = "[SHADER-CACHE] loaded from disk:" in text
    result["cache_scope"] = "Application shader cache only; vendor/OS driver caches are not cleared"
    if phase != "cold" and (not before or not result["cache_read_logged"]):
        raise ValueError("Warm case did not demonstrate application cache reads")
    result["runner_seconds"] = time.monotonic() - started
    result["passed"] = (result["passed"] and all(result["renderer_checks"].values()) and
                        all(result["present_checks"].values()) and all(result["effective_settings_checks"].values()) and
                        all(result.get("sustained", {}).get("checks", {}).values()))
    (destination / "verified.json").write_text(json.dumps(result, indent=2) + "\n")
    if not result["passed"]:
        raise ValueError("Backend/presentation acceptance failed")
    print(label, "PASS", json.dumps(result["measurements"]), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", help="Unique evidence/cache label; letters, numbers and hyphens")
    parser.add_argument("--compile-only", action="store_true", help="CI build check; no game data or GPU required")
    parser.add_argument("--unavailable-dxc",action="store_true",help="Test-only compiler stub in disposable Bin; require saved Vulkan to recover to D3D11")
    parser.add_argument("--backend", choices=("both", "vulkan", "d3d11"), default="both")
    parser.add_argument("--warm", action="store_true", help="Repeat each cold case with its populated isolated cache")
    parser.add_argument("--repetitions", type=int, choices=(1, 2, 3), help="Matched runs per backend: cold followed by warm repetitions")
    parser.add_argument("--presentmon", action="store_true", help="Collect PID-filtered ETW display/submission timing with the pinned console")
    parser.add_argument("--capture", action="store_true", help="Visual run; do not treat its resource samples as a benchmark")
    parser.add_argument("--capture-times", type=lambda text: [int(value) for value in text.split(",")],
                        help="Up to 12 unique capture seconds; implies --capture")
    parser.add_argument("--display-cycle", action="store_true", help="Exercise actual Alt+Enter and resize/restore; use a visual run")
    parser.add_argument("--scenario-file", type=Path, help="Bounded JSON keyboard schedule for another Story/Arcade scenario")
    parser.add_argument("--seconds", type=int, choices=range(120, 601), default=120)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fullscreen", action="store_true")
    parser.add_argument("--msaa", type=int, choices=(1, 2, 4, 8), default=1)
    parser.add_argument("--aa", type=int, choices=(0, 1, 2), default=0, help="Off / FXAA / SMAA")
    parser.add_argument("--anisotropy", type=int, choices=(1, 2, 4, 8, 16), default=1)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("Windows hardware runner")
    if args.compile_only:
        compile_harness()
        compile_capture()
        compile_unavailable_dxc()
        return 0
    if args.unavailable_dxc and (args.backend!="vulkan" or args.warm or args.repetitions not in (None,1)):
        parser.error("--unavailable-dxc requires one Vulkan cold case")
    if args.warm and args.repetitions:
        parser.error("Choose --warm or --repetitions, not both")
    if args.presentmon:
        pinned_presentmon()
    if args.display_cycle and args.fullscreen:
        parser.error("Display cycle starts windowed; test startup fullscreen in a separate case")
    if args.capture_times:
        if len(args.capture_times) > 12 or args.capture_times != sorted(set(args.capture_times)) or any(t < 1 or t >= args.seconds for t in args.capture_times):
            parser.error("Capture seconds must be sorted, unique, within the session and at most 12")
        args.capture = True
    if args.display_cycle and not args.capture:
        parser.error("Display changes perturb timing; use --capture to mark this as a visual run")
    if args.presentmon and args.capture:
        parser.error("Keep ETW measurement runs separate from visual capture")
    if not args.label or len(args.label) > 48 or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-" for c in args.label):
        parser.error("Unsafe evidence label")
    if digest(ROOT / "private/game/default.xbe") != XBE_SHA256:
        raise ValueError("Unsupported game executable")
    run = ROOT / "private/test-saves/renderer-validation"
    evidence = ROOT / "logs/renderer-validation" / args.label
    lock = contained(ROOT, run.parent / "renderer-validation.lock", "private/test-saves")
    report = dict(passed=False, source=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  source_dirty=bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
                  host=platform.platform(), logical_processors=os.cpu_count(),
                  game_sha256=digest(ROOT / "build/windows/Release/futureperfect_recomp.exe"),
                  launcher_sha256=digest(ROOT / "build/launcher/Bin/TimeSplitters.App.exe"),
                  harness_sha256=digest(ROOT / "tests/launcher_lifecycle/RendererGameSmoke.cs"),
                  runner_sha256=digest(Path(__file__)),
                  capture_enabled=args.capture, cases={})
    if args.presentmon:
        report["presentmon_sha256"] = digest(pinned_presentmon())
        report["presentmon_version"] = json.loads((ROOT / "config/build/presentmon-windows.json").read_text())["version"]
    # Refuse any previous label before acquiring resources or modifying fixtures.
    evidence.mkdir(parents=True, exist_ok=False)
    host = subprocess.check_output(["powershell", "-NoProfile", "-Command",
        "Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion | ConvertTo-Json -Compress"], text=True)
    report["gpu"] = json.loads(host)
    with lock.open("x") as stream:
        stream.write(str(os.getpid()))
    prepared = False
    install = None
    try:
        (evidence / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        prepare_run(ROOT, run)
        prepared = True
        install = prepare_install(run)
        if args.unavailable_dxc:
            stub=compile_unavailable_dxc()
            target=contained(ROOT,install / "Bin/dxcompiler.dll","private/test-saves")
            if not target.exists() or not os.path.samefile(target,ROOT / "build/windows/Release/dxcompiler.dll"):
                raise ValueError("Compiler test requires the managed sandbox DLL link")
            target.unlink()  # Unlink only; never write through the build's hard link.
            link_file(stub,target)
            report["injected_dxc_sha256"] = digest(stub)
            report["recovery_scope"] = "Valid test DLL without compiler entry point; original source/build DLLs preserved"
        executable = compile_harness()
        if args.capture:
            helper = compile_capture()
            report["capture_method"] = "dxgi-desktop-duplication-game-rectangle"
            report["capture_helper_sha256"] = digest(helper)
            report["capture_source_sha256"] = digest(ROOT / "tests/launcher_lifecycle/RendererWindowCapture.cpp")
        for backend in (("d3d11", "vulkan") if args.backend == "both" else (args.backend,)):
            phases = ("cold", "warm") if args.warm else ("cold",)
            if args.repetitions:
                phases = ("cold",) + tuple("warm-" + str(index) for index in range(1, args.repetitions))
            for phase in phases:
                report["cases"][backend + "-" + phase] = run_case(executable, install, run, evidence, backend, phase, args)
        check_fixture(ROOT)
        report["passed"] = True
    finally:
        try:
            try:
                if prepared:
                    recycle_disks(ROOT, run)
                    if args.unavailable_dxc and install is not None:
                        stop_owned_game(install)
                        link_file(ROOT / "build/windows/Release/dxcompiler.dll",
                                  contained(ROOT,install / "Bin/dxcompiler.dll","private/test-saves"))
            except BaseException:
                report["passed"] = False
                raise
            finally:
                (evidence / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        finally:
            lock.unlink()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
