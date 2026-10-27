"""A definition-only option addition joins the model on both host compilers."""

from project_paths import source_file as repo_file

import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('extension', [False, True])
def test_debug_settings_definitions(tmp_path, extension):
    compiler, env = native_compiler(tmp_path)
    for name in ('pc_debug.h', 'pc_debug_options.def'):
        (tmp_path / name).write_bytes((repo_file(ROOT, 'src', name)).read_bytes())
    if extension:
        with (tmp_path / 'pc_debug_options.def').open('a') as output:
            output.write('\nPC_DEBUG_OPTION(NEW, new_shared_option, "Future shared option", 0)\n')
    exe = tmp_path / ('model.exe' if os.name == 'nt' else 'model')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/W4', '/WX', '/I'+str(tmp_path),
                str(ROOT / 'tests/diagnostics/debug_settings_model.c'), '/Fe:'+str(exe)]
        if extension: args.append('/DTEST_NEW_OPTION')
    else:
        args = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(tmp_path),
                str(ROOT / 'tests/diagnostics/debug_settings_model.c'), '-o', str(exe)]
        if extension: args.append('-DTEST_NEW_OPTION')
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout+result.stderr


@pytest.mark.skipif(os.name != 'nt', reason='Win32 settings adapter')
def test_windows_debug_dialog_compiles(tmp_path):
    compiler, env = native_compiler(tmp_path)
    result = subprocess.run([compiler, '/nologo', '/std:c11', '/Zs',
        '/I'+str(ROOT / 'src'), str(ROOT / 'src/interface/pc_settings.c')],
        cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
