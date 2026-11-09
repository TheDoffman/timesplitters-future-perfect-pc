"""First-person shoulder filtering uses the shared guest model metadata."""
import os
import subprocess
from pathlib import Path
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]

def test_viewmodel_visibility(tmp_path):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('viewmodel.exe' if os.name == 'nt' else 'viewmodel')
    includes = [ROOT / 'src', ROOT / 'src/interface']
    source = ROOT / 'tests/graphics/viewmodel_visibility.c'
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/O2', *['/I'+str(p) for p in includes], str(source), '/Fe:'+str(exe)]
    else:
        command = [compiler, '-std=c11', '-O2', *['-I'+str(p) for p in includes], str(source), '-o', str(exe)]
    result = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout
