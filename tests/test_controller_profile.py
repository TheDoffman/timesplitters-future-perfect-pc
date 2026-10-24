"""Reproduce launcher device changes against the real runtime parser/sampler."""
import json
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "external/ts3FPrecomp/src"


@pytest.fixture(scope="module")
def controller_exe(tmp_path_factory):
    folder = tmp_path_factory.mktemp("controller-profile")
    compiler, env = native_compiler(folder)
    exe = folder / ("controller.exe" if os.name == "nt" else "controller")
    includes = [RUNTIME, RUNTIME / "input"]
    source = ROOT / "tests/controller_profile.c"
    args = ([compiler, "/nologo", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS",
             *["/I" + str(p) for p in includes], str(source), "/Fe:" + str(exe), "user32.lib"]
            if os.name == "nt" else
            [compiler, "-std=gnu11", *["-I" + str(p) for p in includes], str(source), "-o", str(exe)])
    result = subprocess.run(args, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize("device", ["gamepad:0", "gamepad:2", "xinput:1"])
@pytest.mark.parametrize("mode", ["standard", "custom", "unbound", "none", "defaults"])
def test_controller_profile(controller_exe, tmp_path, device, mode):
    config = json.loads((ROOT / "config/pc-controls.default.json").read_text())
    player = config["controllers"][0]
    player["device"] = device
    if mode == "custom":
        player["bindings"]["a"] = ["pad:b", "none"]
        player["bindings"]["rtrigger"] = ["none", "none"]
    elif mode == "unbound":
        player["bindings"] = {key: "none" for key in player["bindings"]}
    elif mode == "none":
        player["device"] = "none"
    elif mode == "defaults":
        del player["bindings"]
    path = tmp_path / "controls.json"
    path.write_text(json.dumps(config))
    before = path.read_bytes()
    result = subprocess.run([str(controller_exe), str(path), device[-1],
                             "standard" if mode == "defaults" else mode],
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert path.read_bytes() == before
