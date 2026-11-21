"""Safety and measurement gates for the opt-in hardware acceptance runner."""
import importlib.util
import io
import json
import csv
from pathlib import Path
import subprocess
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("renderer_acceptance", ROOT / "tools/testing/test-launcher-renderers-windows.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def test_harness_build_cannot_contaminate_launcher_package_inputs(tmp_path, monkeypatch):
    monkeypatch.setattr(runner, "ROOT", tmp_path)
    monkeypatch.setenv("WINDIR", str(tmp_path / "Windows"))
    launcher = tmp_path / "build/launcher/Bin/TimeSplitters.App.exe"
    launcher.parent.mkdir(parents=True)
    launcher.write_bytes(b"verified launcher dependency")
    commands = []
    monkeypatch.setattr(runner.subprocess, "run", lambda command, **kwargs: commands.append(command))
    output = runner.compile_harness()
    assert output == tmp_path / "build/renderer-validation/renderer-acceptance.exe"
    assert (output.parent / launcher.name).read_bytes() == launcher.read_bytes()
    assert list(launcher.parent.iterdir()) == [launcher]
    assert "/out:" + str(output) in commands[0]


def test_existing_evidence_cannot_reuse_a_pass_or_modify_saves(tmp_path):
    destination = tmp_path / "evidence/d3d11-cold"
    destination.mkdir(parents=True)
    saved = destination / "d3d11-result.json"
    saved.write_text('{"passed":true}')
    with pytest.raises(ValueError, match="stale pass"):
        runner.run_case(tmp_path / "never-run.exe", tmp_path / "install", tmp_path / "run",
                        destination.parent, "d3d11", "cold", SimpleNamespace())
    assert saved.read_text() == '{"passed":true}'
    assert not (tmp_path / "install").exists()


def test_install_refuses_redirected_sandbox_data(tmp_path, monkeypatch):
    monkeypatch.setattr(runner, "ROOT", tmp_path)
    source = tmp_path / "private/game"
    source.mkdir(parents=True)
    (source / "default.xbe").write_bytes(b"immutable game fixture")
    run = tmp_path / "private/test-saves/run"
    (run / "install").mkdir(parents=True)
    outside = tmp_path / "outside"
    outside.mkdir()
    try:
        (run / "install/Data").symlink_to(outside, target_is_directory=True)
    except OSError:
        pytest.skip("Host does not permit creating a directory symlink")
    with pytest.raises(ValueError, match="linked test path"):
        runner.prepare_install(run)
    assert not list(outside.iterdir())


def test_loading_samples_cannot_hide_a_gameplay_regression():
    def sample(seconds, cpu, memory, ticks, gpu=None):
        return dict(seconds=seconds, scene=2, cpu_total_ms=cpu, private_bytes=memory,
                    working_set_bytes=memory // 2, handles=100, ticks=ticks, gpu_dedicated_bytes=gpu)
    samples = [sample(10, 999999, 999999999, 0)]
    samples += [sample(85 + i, 1000 + i * 500, 2000 + i, i * 60, 4096) for i in range(21)]
    result = runner.measurements(samples)
    assert result["cpu_core_equivalents"] == pytest.approx(.5)
    assert result["private_bytes_peak"] == 2020
    assert result["guest_ticks_per_second"] == pytest.approx(60)
    assert result["gpu_dedicated_bytes_peak"] == 4096
    for entry in samples:
        entry.pop("gpu_dedicated_bytes")
    assert runner.measurements(samples)["gpu_dedicated_bytes_peak"] is None
    with pytest.raises(ValueError, match="Insufficient"):
        runner.measurements(samples[:4])


def test_effective_settings_require_active_msaa_and_successful_smaa():
    config = dict(width=1920, height=1080, msaa=4, aa=2)
    log = "[GRAPHICS] Scene 1920x1080, MSAA 4 requested / 4 active\n[SMAA] enabled: 1x High (vulkan)\n"
    assert all(runner.effective_settings(log, config).values())
    assert not runner.effective_settings(log.replace("4 active", "2 active"), config)["scene_resolution_and_msaa"]
    assert not runner.effective_settings(log.replace("1920x1080", "640x480"), config)["scene_resolution_and_msaa"]
    assert not runner.effective_settings(log + "[SMAA] unavailable", config)["smaa_enabled"]
    config.update(width=1, height=1, aa=0)
    assert all(runner.effective_settings("[GRAPHICS] Scene 1008x729, MSAA 4 requested / 4 active", config).values())
    assert not runner.effective_settings(log, config)["smaa_disabled"]


def test_scenarios_bound_input_and_do_not_allow_arbitrary_expectation_overrides(tmp_path):
    path = tmp_path / "scenario.json"
    valid = dict(scenario="Story second mission", expected_weapon=-1, skip_intro=True,
                 input=[dict(at=5, key=69), dict(at=10, key=40, hold_ms=100)])
    path.write_text(json.dumps(valid))
    assert runner.scenario_options(path, 120)["expected_weapon"] == -1
    for invalid_action in (dict(at=float("nan"), key=69), dict(at=121, key=69),
                           dict(at=5, key=999), dict(at=5, key=True), dict(at=5, key=69, hold_ms=9999)):
        path.write_text(json.dumps(dict(valid, input=[invalid_action])))
        with pytest.raises(ValueError, match="Scheduled actions"):
            runner.scenario_options(path, 120)
    path.write_text(json.dumps(dict(valid, expected_weapon=True)))
    with pytest.raises(ValueError, match="expectations"):
        runner.scenario_options(path, 120)


def test_etw_requires_owned_display_frames_in_the_controlled_qpc_window(tmp_path):
    path = tmp_path / "trace.csv"
    samples = [dict(seconds=85 + i, scene=2, flags=0, clock_qpc_ms=1000 + i * 1000) for i in range(21)]
    rows = [dict(ProcessID=42, SwapChainAddress="0x123", CPUStartQPCTimeInMs=1000 + i * 1000 / 60,
                 MsBetweenPresents=1000 / 60, MsBetweenDisplayChange=1000 / 60,
                 MsGPUBusy=2, MsUntilDisplayed=18, PresentMode="Independent Flip") for i in range(1200)]
    def write():
        with path.open("w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
    write()
    result = runner.trace_measurements(path, samples, 42)
    assert result["submitted"]["frames"] == 1200
    assert result["displayed"]["p95_ms"] == pytest.approx(1000 / 60)
    assert result["gpu_busy"]["median_ms"] == 2
    rows[0]["ProcessID"] = 99
    write()
    with pytest.raises(ValueError, match="another process"):
        runner.trace_measurements(path, samples, 42)
    rows[0]["ProcessID"] = 42
    for row in rows:
        row["MsBetweenDisplayChange"] = "NA"
    write()
    with pytest.raises(ValueError, match="Insufficient ETW"):
        runner.trace_measurements(path, samples, 42)
    with pytest.raises(ValueError, match="gameplay samples"):
        runner.trace_measurements(path, [], 42)


def test_save_copy_refuses_redirected_destination_without_changing_source(tmp_path, monkeypatch):
    monkeypatch.setattr(runner, "ROOT", tmp_path)
    run = tmp_path / "private/test-saves/run"
    (run / "saves").mkdir(parents=True)
    (run / "saves/profile").write_bytes(b"fixture")
    install = run / "install"
    install.mkdir()
    outside = tmp_path / "outside"
    outside.mkdir()
    try:
        (install / "Saves").symlink_to(outside, target_is_directory=True)
    except OSError:
        pytest.skip("Host does not permit creating a directory symlink")
    with pytest.raises(ValueError, match="linked test path"):
        runner.copy_saves(run, install)
    assert (run / "saves/profile").read_bytes() == b"fixture"
    assert not list(outside.iterdir())


def test_timeout_stops_owned_child_and_recycles_only_test_disks(tmp_path, monkeypatch):
    install = tmp_path / "install"
    install.mkdir()
    run = tmp_path / "run"
    args = SimpleNamespace(seconds=120, capture=False, width=1280, height=720,
                           msaa=1, aa=0, anisotropy=1, fullscreen=False, label="timeout")
    cleaned = []

    class Child:
        stdout = io.BytesIO(b"bounded diagnostics\n")
        killed = False

        def wait(self, timeout):
            if not self.killed:
                raise subprocess.TimeoutExpired("owned-harness", timeout)
            return -1

        def poll(self):
            return -1 if self.killed else None

        def kill(self):
            self.killed = True

    child = Child()
    monkeypatch.setattr(runner, "ROOT", tmp_path)
    run = tmp_path / "private/test-saves/run"
    monkeypatch.setattr(runner, "copy_saves", lambda *a: None)
    monkeypatch.setattr(runner.subprocess, "Popen", lambda *a, **k: child)
    monkeypatch.setattr(runner, "stop_owned_game", lambda folder: cleaned.append(("stop", folder)))
    monkeypatch.setattr(runner, "recycle_disks", lambda root, folder: cleaned.append(("recycle", folder)))
    evidence = tmp_path / "evidence"
    evidence.mkdir()
    with pytest.raises(subprocess.TimeoutExpired):
        runner.run_case(tmp_path / "harness.exe", install, run, evidence, "vulkan", "cold", args)
    assert child.killed
    assert cleaned == [("stop", install), ("recycle", install)]
    assert not (evidence / "vulkan-cold/verified.json").exists()
