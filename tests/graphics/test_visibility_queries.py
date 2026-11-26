"""Exercise the production asynchronous visibility adapters on the native host."""
import os
import subprocess
import pytest
from pathlib import Path
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('mutation', ['none', 'old-scope', 'no-draw-begin', 'no-draw-end'])
def test_visibility_draw_scope_controls(tmp_path, mutation):
    from test_cutscene_letterbox import compile_and_run
    import re
    adapter=(ROOT/'src/graphics/visibility_rhi_device.inc').read_text()
    if mutation=='old-scope':
        adapter=re.sub(r'static const TsfpVisibilityOps device_visibility_ops=.*?;',
            'static const TsfpVisibilityOps device_visibility_ops={NULL,visibility_create,visibility_destroy,visibility_begin,visibility_end,visibility_result};',adapter,flags=re.S)
    elif mutation=='no-draw-begin':
        adapter=adapter.replace('tsfp_visibility_draw_begin(&device_visibility_draws);','')
    elif mutation=='no-draw-end':
        adapter=adapter.replace('tsfp_visibility_draw_end(&device_visibility_draws);','')
    source=(ROOT/'tests/graphics/visibility_draw_scope.c').read_text().replace(
        '#include "graphics/visibility_rhi_device.inc"',adapter)
    compile_and_run(tmp_path,source,fails=mutation!='none')

@pytest.mark.parametrize('fixture', ['visibility_queries.c', 'visibility_queue.c', 'visibility_gl.c', 'visibility_draw_scope.c'])
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
