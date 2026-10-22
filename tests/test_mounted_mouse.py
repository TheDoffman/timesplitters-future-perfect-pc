"""Exercise the shared mounted gun aiming with guarded guest memory."""
import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Host guest-memory guards require Windows or macOS')
@pytest.mark.parametrize('fixture', ['mounted_mouse', 'native_mounted_mouse'])
def test_mounted_mouse(tmp_path, fixture):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('mounted.exe' if os.name == 'nt' else 'mounted')
    sources = [str(ROOT / 'tests' / (fixture+'.c')), str(ROOT / 'src/pc_input.c')]
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/W4',
                   '/I' + str(ROOT / 'src'), *sources, '/Fe:' + str(exe)]
    else:
        command = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-I' + str(ROOT / 'src'), *sources, '-o', str(exe)]
    result = subprocess.run(command, cwd=tmp_path, env=environment,
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS: mounted mouse routing' in result.stdout
    if fixture == 'native_mounted_mouse':
        for mutation in ('legacy-clock', 'legacy-batch'):
            legacy = subprocess.run([str(exe), mutation], capture_output=True, text=True, timeout=10)
            assert legacy.returncode != 0 and 'FAIL' in legacy.stderr, legacy.stdout + legacy.stderr
