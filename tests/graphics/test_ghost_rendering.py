"""Exercise the real guest wrappers and HLE state cache together."""


import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('break_restore', [False, True], ids=['fixed', 'detect-state-leak'])
def test_spirit_rendering(tmp_path, break_restore):
    compiler, env = native_compiler(tmp_path)
    runtime = ROOT / 'external/ts3FPrecomp/src'
    includes = [runtime, ROOT/'src', *(runtime/n for n in ('hle','d3d','kernel','config','platform'))]
    if break_restore:
        source = (runtime/'hle/hle_d3d8_state.c').read_text(encoding='utf-8')
        target = '        g_prev_valid = 0;'
        assert source.count(target) == 1
        mutant = tmp_path/'mutant/hle/hle_d3d8_state.c'
        mutant.parent.mkdir(parents=True)
        mutant.write_text(source.replace(target, '        if (flags) g_prev_valid = 0; /* mutation: leak on exit */'), encoding='utf-8')
        includes.insert(0, mutant.parent.parent)
    exe = tmp_path/('ghost.exe' if os.name=='nt' else 'ghost')
    if os.name == 'nt':
        args = [compiler, '/nologo','/std:c11','/O2','/Gy','/Gw','/we4028','/we4029']
        args += ['/I'+str(p) for p in includes]
        args += [str(ROOT/'tests/graphics/ghost_rendering.c'),'/Fe:'+str(exe),'/Fo:'+str(tmp_path/'ghost.obj'),'/link','/OPT:REF']
    else:
        args = [compiler,'-std=gnu11','-O2','-Wl,-dead_strip']
        args += ['-I'+str(p) for p in includes]
        args += [str(ROOT/'tests/graphics/ghost_rendering.c'),'-o',str(exe)]
    result = subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    result = subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    if break_restore:
        assert result.returncode != 0, 'Test missed a stale render-state cache'
        assert 'observed[D3DRS_' in result.stderr
    else:
        assert result.returncode == 0, result.stdout+result.stderr
        assert 'PASS:' in result.stdout
