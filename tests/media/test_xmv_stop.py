"""The attract-video skip must stop host playback, not start it."""


import json
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / 'external/ts3FPrecomp'


@pytest.mark.parametrize('wrong_binding', [False, True], ids=['fixed', 'detect-start-binding'])
def test_xmv_stop(tmp_path, wrong_binding):
    symbols = json.loads((RUNTIME/'config/extra_symbols/45410066.json').read_text(encoding='utf-8'))
    symbol = next(s for s in symbols['symbols'] if s['address'] == 0x445252)
    assert symbol['name'] == 'XMVPlaybackStop' and symbol['stack_bytes'] == 4
    source = (ROOT/'tests/media/xmv_stop.c').read_text(encoding='utf-8')
    if wrong_binding:
        source = source.replace('hle_XMVPlaybackStop();', 'hle_XMVPlaybackStart();')
    fixture = tmp_path/'xmv_stop.c'
    fixture.write_text(source, encoding='utf-8')
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('xmv.exe' if os.name == 'nt' else 'xmv')
    includes = [RUNTIME/'src']
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/we4028', '/we4029']
        args += ['/I'+str(p) for p in includes]
        args += [str(fixture), '/Fe:'+str(exe), '/Fo:'+str(tmp_path/'xmv.obj')]
    else:
        args = [compiler, '-std=gnu11', '-O2']
        args += ['-I'+str(p) for p in includes]
        args += [str(fixture), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert (result.returncode != 0) == wrong_binding, result.stdout+result.stderr
    assert ('closes==1 && !active' in result.stderr) if wrong_binding else ('PASS:' in result.stdout)
