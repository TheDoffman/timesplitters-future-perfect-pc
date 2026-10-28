"""Profile-scoped early access, shared menu integration and US binary contracts."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))


def test_unlock_content(tmp_path):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('unlocks.exe' if os.name == 'nt' else 'unlocks')
    source = ROOT / 'tests/storage/unlock_content.c'
    args = ([compiler, '/nologo', '/std:c11', '/W4', '/WX', str(source), '/Fe:' + str(exe)]
            if os.name == 'nt' else
            [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(exe)])
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


def test_us_menu_and_profile_contracts():
    import struct
    from tools.recomp import config
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required')
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(path))
        def raw(address, size):
            at = config.va_to_file_offset(address)
            return image[at:at + size]
        assert raw(0x1237b, 4) == b'\xcc' * 4
        assert struct.unpack('<4I', raw(0x524578, 16)) == (0x2d08c0, 0, 0, 0)
        assert struct.unpack('<4I', raw(0x523930, 16)) == (0x2cc420, 0x2cc390, 0x2cc5d0, 0)
        # Existing save/load both transfer the full 1F54 bytes, including the
        # extension. The original version stays 2, immediately before it.
        assert raw(0x191290, 5) == b'\x68\x54\x1f\x00\x00'
        assert raw(0x1925c6, 5) == b'\x68\x54\x1f\x00\x00'
        assert raw(0x1925b3, 10) == bytes.fromhex('c786e01e000002000000')
    finally:
        vars(config).update(previous)


def test_original_unlock_predicates(tmp_path):
    import struct
    from test_recomp_float_joins import compile_and_run, config, FunctionTranslator
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required')
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(path))
        ranges = [(0x69630,0x69636),(0x696d0,0x696ea),(0x696f0,0x69716),
                  (0x1cceb0,0x1ccebc),(0x1cd120,0x1cd166),
                  (0x1cd2b0,0x1cd32e),(0x1cd330,0x1cd37c),(0x1cd380,0x1cd3df),
                  (0x1cd3e0,0x1cd554),(0x1cd560,0x1cd58e),(0x1cd890,0x1cd8e1),
                  (0x1cd900,0x1cd91a),(0x1cd920,0x1cd93a),(0x1cd940,0x1cd979),
                  (0x1cd980,0x1cd9c4),(0x1cd9d0,0x1cd9e8),(0x1cd9f0,0x1cda1c),
                  (0x1cda20,0x1cda61),(0x1cda70,0x1cdad7),
                  (0x22e050,0x22e069),(0x22e070,0x22e08e),
                  (0x2be590,0x2be5d5),(0x2be5e0,0x2be5fd)]
        functions = {a:dict(name=f'sub_{a:08X}',end=e) for a,e in ranges}
        translator = FunctionTranslator(image, functions)
        hooked = {0x1cd3e0,0x1cda70,0x1cd560}
        bodies = []
        declarations = []
        for address, info in functions.items():
            declarations.append(f'void {info["name"]}(void);')
            body = translator.translate_function(address, info)
            if address in hooked:
                body = body.replace(f'void {info["name"]}(void)', f'void {info["name"]}_gen(void)', 1)
                declarations.append(f'void {info["name"]}_gen(void);')
            bodies.append(body)
        menu = (ROOT / 'src/interface/pc_display_menu.c').read_text()
        helpers = menu[menu.index('typedef struct GuestRegisters'):menu.index('static int guest_writable')]
        start = menu.index('static uint32_t guest_call(')
        helpers += menu[start:menu.index('\n}', start)+2]
        hooks = (ROOT / 'src/interface/pc_unlock_menu.inc').read_text()
        hooks = hooks[:hooks.index('void pc_unlock_profile_init')]
        # Load original initialized sections; virtual-only profile RAM stays zero.
        base = struct.unpack_from('<I',image,0x104)[0]
        count, headers = struct.unpack_from('<II',image,0x11c)
        loads = [f'FILE *xbe=fopen("{path.as_posix()}","rb");assert(xbe);']
        for index in range(count):
            _,va,_,offset,size = struct.unpack_from('<5I',image,headers-base+index*56)
            if size:
                loads.append(f'assert(!fseek(xbe,{offset},SEEK_SET));assert(fread((void *)(g_xbox_mem_offset+0x{va:x}),1,{size},xbe)=={size});')
        loads.append('fclose(xbe);')
        source = (ROOT / 'tests/storage/unlock_original.c').read_text()
        source = source.replace('/* GUEST_CALL_HELPERS */', helpers).replace('/* UNLOCK_HOOKS */', hooks)
        source = source.replace('/* DECLARATIONS */','\n'.join(declarations))
        source = source.replace('/* ORIGINAL_ROUTINES */','\n'.join(bodies)).replace('/* LOAD_XBE */','\n'.join(loads))
        print(compile_and_run(tmp_path,source))
    finally:
        vars(config).update(previous)
