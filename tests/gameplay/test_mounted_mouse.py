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
@pytest.mark.parametrize('fixture', ['mounted_mouse', 'native_mounted_mouse', 'camera_region_host', 'mech_mouse'])
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

@pytest.mark.parametrize('legacy', [False, True, 'driver'], ids=['vehicle-mouse', 'old-route-reproduction', 'old-tank-driver'])
def test_original_vehicle_mouse(tmp_path, legacy):
    import hashlib
    import struct
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported US XBE required for original vehicle update')
    from test_recomp_float_joins import config, FunctionTranslator
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions = {a: dict(name=f'sub_{a:08X}', end=end) for a, end in
                     ((0xd2470, 0xd2b7e), (0x17a440, 0x17aa1a), (0x17a0a0, 0x17a111),
                      (0x151650, 0x15178f), (0x152090, 0x15211d),
                      (0x1730e0,0x17362e),(0x153440,0x153490))}
        translator = FunctionTranslator(image, functions)
        code = '\n'.join(translator.translate_function(a, f) for a, f in functions.items())
        fixture = (ROOT / 'tests/gameplay/vehicle_mouse_guest.c').read_text()
        implemented = set(re.findall(r'void (sub_[0-9A-F]+)\(void\)', fixture + code))
        calls = set(re.findall(r'RECOMP_DIRECT_CALLEE_AT\([^,]+, (sub_[0-9A-F]+)\)', code))
        stubs = '\n'.join(f'void {n}(void){{assert(!"unexpected {n}");}}' for n in sorted(calls-implemented))
        constants = '\n'.join(f'MEM32(0x{a:X})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:X};'
                              for a in sorted(set(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',code + fixture)))
                              if 0x470000 <= a < 0x490000)
        helper = (ROOT / 'src/input/pc_mounted.h').read_text()
        if legacy is True:
            helper = helper.replace('mode!=0x80 && mode!=0x400', 'mode!=0x80')
        if legacy == 'driver':
            fixture = fixture.replace('pc_tank_driver_input(g_eax,g_edx,PLAYER,g_xbox_mem_offset,&r)', '(PcTankDriverScope){0}').replace('assert(scope.actor_link);', '')
        source = tmp_path / 'vehicle_mouse.c'
        source.write_text(fixture.replace('/* GENERATED_ROUTINES */', code).replace('/* UNREACHED */', stubs)
                          .replace('/* CONSTANTS */', constants).replace('/* MOUSE_IMPLEMENTATION */', helper))
        compiler, environment = native_compiler(tmp_path)
        exe = tmp_path / ('vehicle_mouse.exe' if os.name == 'nt' else 'vehicle_mouse')
        includes = [ROOT / 'src', ROOT / 'external/ts3FPrecomp/templates/runtime']
        sources = [str(source), str(ROOT / 'src/input/pc_input.c')]
        command = ([compiler, '/nologo', '/O2', *['/I'+str(p) for p in includes], *sources, '/Fe:'+str(exe)]
                   if os.name == 'nt' else [compiler, '-std=c11', '-O2', *['-I'+str(p) for p in includes],
                                           *sources, '-lm', '-o', str(exe)])
        result = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
        if legacy == 'driver':
            assert result.returncode != 0 and 'tracks[0]' in result.stderr, result.stdout + result.stderr
        elif legacy is True:
            assert result.returncode != 0 and 'handled==1' in result.stderr.replace(' ', ''), result.stdout + result.stderr
        else:
            assert result.returncode == 0, result.stdout + result.stderr
            assert 'PASS: original vehicle gunner' in result.stdout
    finally:
        vars(config).update(previous)

@pytest.mark.parametrize('legacy', [False, True], ids=['mech-mouse', 'old-mech-route'])
def test_original_mech_mouse(tmp_path, legacy):
    import hashlib
    import struct
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported US XBE required for original Battle Mech input')
    from test_recomp_float_joins import config, FunctionTranslator
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions = {a: dict(name=f'sub_{a:08X}', end=end) for a, end in
                     ((0x37aa70,0x37acaa),(0x3797b0,0x379957))}
        translator = FunctionTranslator(image, functions)
        code = '\n'.join(translator.translate_function(a, f) for a, f in functions.items())
        fixture = (ROOT / 'tests/gameplay/mech_mouse_guest.c').read_text()
        implemented = set(re.findall(r'void (sub_[0-9A-F]+)\(void\)', fixture + code))
        calls = set(re.findall(r'RECOMP_DIRECT_CALLEE_AT\([^,]+, (sub_[0-9A-F]+)\)', code))
        stubs = '\n'.join(f'void {n}(void){{assert(!"unexpected {n}");}}' for n in sorted(calls-implemented))
        constants = '\n'.join(f'MEM32(0x{a:X})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:X};'
                              for a in sorted(set(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',code + fixture)))
                              if 0x470000 <= a < 0x4a0000)
        helper = (ROOT / 'src/input/pc_mounted.h').read_text()
        if legacy:
            helper = helper.replace(' && mode!=0x8000', '')
        source = tmp_path / 'mech_mouse.c'
        source.write_text(fixture.replace('/* GENERATED_ROUTINES */', code).replace('/* UNREACHED */', stubs)
                          .replace('/* CONSTANTS */', constants).replace('/* MOUSE_IMPLEMENTATION */', helper))
        compiler, environment = native_compiler(tmp_path)
        exe = tmp_path / ('mech_mouse.exe' if os.name == 'nt' else 'mech_mouse')
        includes = [ROOT / 'src', ROOT / 'external/ts3FPrecomp/templates/runtime']
        sources = [str(source), str(ROOT / 'src/input/pc_input.c')]
        command = ([compiler, '/nologo', '/O2', *['/I'+str(p) for p in includes], *sources, '/Fe:'+str(exe)]
                   if os.name == 'nt' else [compiler, '-std=c11', '-O2', *['-I'+str(p) for p in includes],
                                           *sources, '-lm', '-o', str(exe)])
        result = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
        if legacy:
            assert result.returncode != 0 and 'handled==1' in result.stderr.replace(' ', ''), result.stdout + result.stderr
        else:
            assert result.returncode == 0, result.stdout + result.stderr
            assert 'PASS: original mech input' in result.stdout

    finally:
        vars(config).update(previous)
