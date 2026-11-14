"""Exercise the production asynchronous visibility adapters on the native host."""
import os
import subprocess
import pytest
from pathlib import Path
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('fixture', ['visibility_queries.c', 'visibility_queue.c', 'visibility_gl.c'])
def test_visibility_adapters(tmp_path, fixture):
    compiler, env = native_compiler(tmp_path)
    source = ROOT / 'tests/graphics' / fixture
    exe = tmp_path / ('visibility.exe' if os.name == 'nt' else 'visibility')
    extra = []
    runtime = ROOT / 'external/ts3FPrecomp/src/config'
    command = ([compiler, '/nologo', '/std:c11', '/W4', '/WX',
                '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(ROOT / 'src'),
                '/I' + str(runtime), '/I' + str(ROOT / 'src/interface'), str(source), *extra, '/Fe:' + str(exe)] if os.name == 'nt' else
               [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I' + str(ROOT / 'src'), '-I' + str(runtime), '-I' + str(ROOT / 'src/interface'), str(source), *extra, '-lm', '-pthread', '-o', str(exe)])
    build = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True,
                           text=True, timeout=60)
    assert build.returncode == 0, build.stdout + build.stderr
    run = subprocess.run([str(exe)], cwd=tmp_path, env=env, capture_output=True,
                         text=True, timeout=10)
    assert run.returncode == 0, run.stdout + run.stderr


def test_visibility_sdk_original_body_registration():
    import sys
    sys.path.insert(0,str(ROOT/'tools/compiler'))
    from c_source_scan import scan
    defined,wrapped,_=scan(ROOT/'external/ts3FPrecomp/titles/futureperfect/src/recomp_manual.c')
    # Both wrappers need discoverable original bodies; combining declarations
    # silently excludes these SDK bodies from generated C and fails the link.
    assert {0x3d4250,0x3d42f0} <= defined & wrapped
