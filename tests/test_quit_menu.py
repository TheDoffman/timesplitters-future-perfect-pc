"""Compile the production Quit to Desktop guest adapter on either host."""
import os
import hashlib
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("native", [False, True])
@pytest.mark.parametrize("fixture", ['quit_menu', 'story_menu', 'story_lobby'])
def test_quit_menu(tmp_path, native, fixture):
    compiler, env = native_compiler(tmp_path)
    source = ROOT / 'tests' / (fixture+'.c')
    exe = tmp_path / ('quit.exe' if os.name == 'nt' else 'quit')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-lm', '-o', str(exe)]
    if native:
        args.insert(1, "/DTSFP_NATIVE_DISPLAY" if os.name == "nt" else "-DTSFP_NATIVE_DISPLAY")
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout


def test_story_original_us_entry_contract():
    """Check the observed original route and callback padding without disclosing code."""
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported local US executable required')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    base = struct.unpack_from('<I', image, 0x104)[0]
    count, table = struct.unpack_from('<II', image, 0x11c)
    sections = [struct.unpack_from('<IIII', image, table-base+i*0x38+4) for i in range(count)]
    def read(address, size):
        for start, _, offset, length in sections:
            if start <= address and address+size <= start+length:
                return image[offset+address-start:offset+address-start+size]
        raise AssertionError('Unmapped bounded US contract read')
    assert read(0x1237b, 4) == b'\xcc'*4
    call = read(0x2d0c89, 5)
    assert call[0] == 0xe8
    assert 0x2d0c8e+struct.unpack_from('<i', call, 1)[0] == 0x79d60
    assert b'\x68'+struct.pack('<I', 0x5238e0) in read(0x2d0aa0, 11)
    assert struct.unpack('<I', read(0x5238e0, 4))[0] == 0x2cb090
    # Rule10's original Story spawn selects the shared human team1 branch.
    index=read(0x25a484+9,1)[0]
    assert struct.unpack('<I',read(0x25a474+index*4,4))[0]==0x25a446
    assert read(0x25a446,2)==b'\x6a\x01'
    team_call=read(0x33acd6,5)
    assert team_call[0]==0xe8 and 0x33acdb+struct.unpack_from('<i',team_call,1)[0]==0xd7550
    # D7570 also writes slot+1 directly, bypassing D7550 for remote bodies.
    assert read(0xd75fd,10)==bytes.fromhex('8b4c24048b4108408981')
    # Original353990 copies into a256-byte stack buffer before353820 sends.
    assert read(0x353996,6)==bytes.fromhex('81ec04010000')
    assert read(0x353a0c,4)==bytes.fromhex('8d7c2410')
    # Actual first-level handoff ammo/grant return sites used by the adapter.
    for address, target in ((0x218962,0x1ba2b0),(0x21896a,0x1bfca0),
                            (0x21b58d,0x1ba2b0),(0x21b595,0x1bfca0),
                            (0x218a4f,0x1ba2b0),(0x218a57,0x1bfca0)):
        call=read(address,5)
        assert call[0]==0xe8 and address+5+struct.unpack_from('<i',call,1)[0]==target



def test_story_original_config_stages(tmp_path):
    """Execute the original stage builder, not a hand-written factory model."""
    from test_recomp_float_joins import compile_and_run
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported local US executable required')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous=dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions={start:dict(name=f'sub_{start:08X}',end=end) for start,end in (
            (0x65940,0x65c69),(0x64ce0,0x64d04),(0x3533f0,0x3533f6),(0x357540,0x35768e),
            (0x6b100,0x6b281),(0x3568c0,0x356913),(0x358560,0x358590))}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(start,info) for start,info in functions.items())
        row=image[config.va_to_file_offset(0x4d1ac0):config.va_to_file_offset(0x4d1ac0)+0x44]
        movie=struct.unpack_from('<I',row,0x20)[0]
        scene=image[config.va_to_file_offset(0x51fc30+movie*36)]
        outro=struct.unpack_from('<I',row,0x24)[0]
        outro_scene=image[config.va_to_file_offset(0x51fc30+outro*36)]
        constants='\n'.join(f'MEM8(0x{0x4d1ac0+i:x})={value};' for i,value in enumerate(row))
        constants+=f'\nMEM8(0x{0x51fc30+movie*36:x})={scene};'
        constants+=f'\nMEM8(0x{0x51fc30+outro*36:x})={outro_scene};'
        table=image[config.va_to_file_offset(0x6b284):config.va_to_file_offset(0x6b284)+40]
        constants+='\n'+'\n'.join(f'MEM8(0x{0x6b284+i:x})={value};' for i,value in enumerate(table))
    finally:
        vars(config).update(previous)
    source=(ROOT/'tests/story_config_guest.c').read_text()
    output=compile_and_run(tmp_path,source.replace('/* ORIGINAL_STAGE_BUILDER */',code)
        .replace('/* ORIGINAL_MISSION_DATA */',constants)
        .replace('INTRO_SCENE',str(scene)).replace('OUTRO_SCENE',str(outro_scene)))
    assert 'PASS:' in output
