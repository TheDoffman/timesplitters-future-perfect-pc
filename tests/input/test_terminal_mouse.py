"""Exercise the shared terminal pointer routing with guarded guest memory."""

from project_paths import source_file as repo_file

import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Host guest-memory guards require Windows or macOS')
@pytest.mark.parametrize('source', ['terminal_mouse.c', 'native_terminal_mouse.c'])
def test_terminal_mouse(tmp_path, source):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('terminal.exe' if os.name == 'nt' else 'terminal')
    sources = [str(repo_file(ROOT, 'tests', source)), str(ROOT / 'src/input/pc_input.c')]
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
    assert 'PASS: terminal mouse routing' in result.stdout
