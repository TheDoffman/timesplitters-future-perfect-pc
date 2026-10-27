"""Compile the production Quit to Desktop guest adapter on either host."""


import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize("native", [False, True])
def test_quit_menu(tmp_path, native):
    compiler, env = native_compiler(tmp_path)
    source = ROOT / 'tests/graphics/quit_menu.c'
    exe = tmp_path / ('quit.exe' if os.name == 'nt' else 'quit')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-lm', '-o', str(exe)]
    if native:
        args.insert(1, "/DTSFP_NATIVE_DISPLAY" if os.name == "nt" else "-DTSFP_NATIVE_DISPLAY")
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout
