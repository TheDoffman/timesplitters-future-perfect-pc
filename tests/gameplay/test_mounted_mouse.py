"""Exercise the shared mounted gun aiming with guarded guest memory."""

from project_paths import source_file as repo_file

import os
from pathlib import Path
import subprocess
import sys
import re

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('adapter', ['src/input/pc_controls.c', 'src/native/input_sdl.c'])
@pytest.mark.parametrize('mutation', ['none', 'persistent', 'uncached', 'unordered'])
def test_mouse_update_regions(tmp_path, adapter, mutation):
    text = (ROOT / adapter).read_text()
    body = re.search(r'void pc_controls_apply_mouse\([^\n]*\).*?\n\}', text, re.S).group()
    seed = re.search(r'PcGuestRegion region=(pc_camera_mouse_region\([^;]+\));', body)
    assert seed, 'Both adapters must seed a fresh, ordered mouse mapping'
    if mutation == 'persistent':
        body = body.replace(seed.group(), 'static PcGuestRegion region={0}; if(!region.size) region='+seed.group(1)+';')
    elif mutation == 'unordered':
        body = body.replace(seed.group(), 'PcGuestRegion region={0};')
    memory = (ROOT / 'src/runtime/pc_guest_memory.h').read_text()
    helpers = '\n'.join(re.search(r'static inline [^\n]*\b' + name + r'\(.*?\n\}', memory, re.S).group()
                        for name in ('pc_guest_region_remaining',))
    span = (ROOT / 'src/runtime/pc_guest_span.h').read_text()
    helpers += '\n' + re.search(r'static inline int pc_guest_memory_in_span\(.*?\n\}', span, re.S).group()
    if mutation == 'uncached':
        helpers = helpers.replace('if(bytes<=pc_guest_region_remaining', 'if(0 && bytes<=pc_guest_region_remaining')
    fixture = (ROOT / 'tests/graphics/mouse_region.c').read_text().replace('/* PRODUCTION_MEMORY_HELPERS */', helpers)
    fixture = fixture.replace('/* PRODUCTION_MOUSE_ADAPTER */', body)
    source = tmp_path / 'mouse-region.c'
    source.write_text(('#define TEST_SDL\n' if 'input_sdl' in adapter else '') + fixture)
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('mouse-region.exe' if os.name == 'nt' else 'mouse-region')
    sources = [str(source), str(ROOT / 'src/input/pc_input.c')]
    command = ([compiler, '/nologo', '/std:c11', '/O2', '/W4', '/I'+str(ROOT/'src'), *sources, '/Fe:'+str(exe)]
               if os.name == 'nt' else [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                                       '-I'+str(ROOT/'src'), *sources, '-lm', '-o', str(exe)])
    result = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    if mutation != 'none':
        assert result.returncode != 0 and 'FAIL' in result.stderr, result.stdout + result.stderr
        expected = {'uncached': ('queries==1', '(queries=7)'),
                    'persistent': ('queries==2', '(queries=0)'),
                    'unordered': ('queries==1', '(queries=2)')}[mutation]
        assert all(s in result.stderr for s in expected), result.stderr
    else:
        assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Host guest-memory guards require Windows or macOS')
@pytest.mark.parametrize('fixture', ['mounted_mouse', 'native_mounted_mouse', 'camera_region_host'])
def test_mounted_mouse(tmp_path, fixture):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('mounted.exe' if os.name == 'nt' else 'mounted')
    sources = [str(repo_file(ROOT, 'tests', fixture+'.c')), str(ROOT / 'src/input/pc_input.c')]
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
    expected = 'PASS: camera mapping order' if fixture == 'camera_region_host' else 'PASS: mounted mouse routing'
    assert expected in result.stdout
    if fixture == 'native_mounted_mouse':
        for mutation in ('legacy-clock', 'legacy-batch'):
            legacy = subprocess.run([str(exe), mutation], capture_output=True, text=True, timeout=10)
            assert legacy.returncode != 0 and 'FAIL' in legacy.stderr, legacy.stdout + legacy.stderr


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Host guest-memory guards require Windows or macOS')
def test_original_mounted_binding(tmp_path):
    import hashlib
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported US XBE required for original mount/dismount routines')
    from test_recomp_float_joins import config, FunctionTranslator
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions = {a: dict(name=f'sub_{a:08X}', end=end)
                     for a, end in ((0xb76b0, 0xb7823), (0xb7830, 0xb7946))}
        translator = FunctionTranslator(image, functions)
        code = '\n'.join(translator.translate_function(a, f) for a, f in functions.items())
        fixture = (ROOT / 'tests/gameplay/mounted_guest_binding.c').read_text()
        implemented = set(re.findall(r'void (sub_[0-9A-F]+)\(void\)', fixture + code))
        calls = set(re.findall(r'RECOMP_DIRECT_CALLEE_AT\([^,]+, (sub_[0-9A-F]+)\)', code))
        stubs = '\n'.join(f'void {name}(void){{assert(!"unexpected {name}");}}'
                          for name in sorted(calls - implemented))
        fixture = fixture.replace('/* UNREACHED_SERVICES */', stubs).replace('/* GENERATED_ROUTINES */', code)
        fixture = fixture.replace('/* MOUSE_IMPLEMENTATION */', '#include "pc_mounted.h"')
        source = tmp_path / 'mounted_guest_binding.c'
        source.write_text(fixture)
        compiler, environment = native_compiler(tmp_path)
        exe = tmp_path / ('mounted_guest.exe' if os.name == 'nt' else 'mounted_guest')
        includes = [ROOT / 'src', ROOT / 'external/ts3FPrecomp/templates/runtime']
        sources = [str(source), str(ROOT / 'src/input/pc_input.c')]
        command = ([compiler, '/nologo', '/O2', *['/I'+str(p) for p in includes], *sources, '/Fe:'+str(exe)]
                   if os.name == 'nt' else [compiler, '-std=c11', '-O2', *['-I'+str(p) for p in includes],
                                           *sources, '-lm', '-o', str(exe)])
        result = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS: original mount/dismount' in result.stdout
    finally:
        vars(config).update(previous)
