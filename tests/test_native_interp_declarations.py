"""Execute the shared replay-state regression on the native Mac toolchain."""
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(sys.platform != "darwin", reason="Native Mac compiler/linker regression")
def test_interpolation_retains_vertex_declarations(tmp_path):
    source = ROOT / "external/ts3FPrecomp/src"
    executable = tmp_path / "interp-declarations"
    includes = [source, *(source / name for name in ("d3d", "config", "hle", "kernel", "platform"))]
    command = ["cc", "-std=gnu11", "-O1", "-Wl,-dead_strip"]
    command += ["-I" + str(path) for path in includes]
    command += [str(ROOT / "tests/native_interp_declarations.c"), "-o", str(executable)]
    subprocess.run(command, check=True, capture_output=True, text=True)
    result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
    assert "snapshot/change/copy/restore: PASS" in result.stdout
