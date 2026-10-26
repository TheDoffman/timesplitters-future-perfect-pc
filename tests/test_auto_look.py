"""Exercise automatic lookahead in gameplay and the Controls menus."""
import os
from pathlib import Path
import subprocess
import sys
import re

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('mutation', ['none', 'persistent', 'uncached'])
def test_auto_look_regions(tmp_path, mutation):
    from test_cutscene_letterbox import compile_and_run
    memory = (ROOT/'src/pc_guest_memory.h').read_text()
    helpers = '\n'.join(re.search(r'static inline [^\n]*\b'+name+r'\(.*?\n\}', memory, re.S).group()
                        for name in ('pc_guest_region_remaining', 'pc_guest_region',
                                     'pc_guest_memory', 'pc_guest_memory_in_region'))
    body = (ROOT/'src/pc_auto_look_guest.inc').read_text()
    if mutation == 'persistent':
        assert 'PcGuestRegion regions[2]={{0}};' in body
        body = body.replace('PcGuestRegion regions[2]={{0}};', 'static PcGuestRegion regions[2]={{0}};')
    if mutation == 'uncached':
        body = body.replace('if(bytes<=pc_guest_region_remaining', 'if(0 && bytes<=pc_guest_region_remaining')
        helpers = helpers.replace('if(bytes<=pc_guest_region_remaining', 'if(0 && bytes<=pc_guest_region_remaining')
    source = (ROOT/'tests/auto_look_region.c').read_text()
    source = source.replace('/* PRODUCTION_MEMORY_HELPERS */', helpers).replace('/* PRODUCTION_AUTO_LOOK */', body)
    compile_and_run(tmp_path, source, fails=mutation!='none')


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Host guest-memory guards require Windows or macOS')
@pytest.mark.parametrize('fixture', ['auto_look', 'auto_look_span'])
def test_auto_look(tmp_path, fixture):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('auto-look.exe' if os.name == 'nt' else 'auto-look')
    sources = [str(ROOT / 'tests' / (fixture+'.c'))]
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
    assert 'PASS: automatic lookahead' in result.stdout
