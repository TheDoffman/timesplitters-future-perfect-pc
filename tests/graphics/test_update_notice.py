"""Validate launcher-to-game notice input on all supported hosts."""
import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler
ROOT = Path(__file__).resolve().parents[2]
def test_update_notice(tmp_path):
    compiler, env = native_compiler(tmp_path)
    source = ROOT / 'tests/graphics/update_notice.c'
    exe = tmp_path / ('notice.exe' if os.name == 'nt' else 'notice')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/D_CRT_SECURE_NO_WARNINGS',
                '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
