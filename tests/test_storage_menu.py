"""Host save-volume UI; unrelated game text and guest disk geometry stay intact."""
from pathlib import Path
import os
import subprocess
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


def test_storage_menu(tmp_path):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('storage-menu.exe' if os.name == 'nt' else 'storage-menu')
    source = str(ROOT / 'tests/storage_menu.c')
    args = ([compiler, '/nologo', '/std:c11', '/W4', '/WX', '/D_CRT_SECURE_NO_WARNINGS',
             '/I' + str(ROOT / 'src'), source, '/Fe:' + str(exe)] if os.name == 'nt'
            else [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                  '-I' + str(ROOT / 'src'), source, '-o', str(exe)])
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
