from project_paths import source_file as repo_file
"""Compile the production Quit to Desktop guest adapter on either host."""


import os
import hashlib
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize("native", [False, True])
@pytest.mark.parametrize("fixture", ['quit_menu', 'story_menu', 'story_lobby', 'story_drop_state'])
def test_quit_menu(tmp_path, native, fixture):
    compiler, env = native_compiler(tmp_path)
    source = repo_file(ROOT, 'tests', fixture+'.c')
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
    assert read(0x123c9, 4) == b'\xcc'*4
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
    # Original held-model creation skips the built-in definitions41/56/61.
    # Their cosmetic fire replay cannot require a world-weapon entity.
    model_gate=read(0x1beed9,40)
    for comparison in ('83f829','83f838','83f83d'):
        assert bytes.fromhex(comparison) in model_gate
    # The original capsule/world overlap path skips bodies without collision40.
    assert read(0x733e4,4)==bytes.fromhex('f6462840')
    # Story's physical throw returns before the ordinary network-drop send.
    assert read(0xc2a34,1)==b'\xc3'
    drop_send=read(0xc2d22,5)
    assert drop_send[0]==0xe8 and 0xc2d27+struct.unpack_from('<i',drop_send,1)[0]==0x3581b0
    # Actual first-level handoff ammo/grant return sites used by the adapter.
    for address, target in ((0x218962,0x1ba2b0),(0x21896a,0x1bfca0),
                            (0x21b58d,0x1ba2b0),(0x21b595,0x1bfca0),
                            (0x218a4f,0x1ba2b0),(0x218a57,0x1bfca0)):
        call=read(address,5)
        assert call[0]==0xe8 and address+5+struct.unpack_from('<i',call,1)[0]==target



def test_story_original_splitter_camo(tmp_path):
    from test_recomp_float_joins import compile_and_run
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    import re
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported local US executable required')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous=dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions={a:dict(name=f'sub_{a:08X}',end=b) for a,b in (
            (0x11f780,0x11f7df),(0x11f7e0,0x11faf2),(0x11d510,0x11d566),(0x11d400,0x11d4c8))}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(a,info) for a,info in functions.items())
        # The current translator also loads SIMD constants via integer cells.
        constants={int(a,16)+offset for a in re.findall(r'0x([0-9A-Fa-f]+)',code)
                   for offset in (0,4,8,12)}
        constants={a for a in constants if 0x470000<=a<0x500000}
        seeds='\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08x}u;' for a in sorted(constants))
    finally:
        vars(config).update(previous)
    source=(ROOT/'tests/network/story_camo.c').read_text().replace('/* ORIGINAL_CAMO */',code)
    defined=set(re.findall(r'void (sub_[0-9A-F]+)\(void\)',source))
    missing=set(re.findall(r'\b(sub_[0-9A-F]+)\b',code))-defined
    source=source.replace('/* UNREACHED */','\n'.join('void '+name+'(void){assert(!"unexpected '+name+'");}' for name in sorted(missing)))
    production=(ROOT/'src/interface/pc_display_menu.c').read_text()
    start=production.index('/* Replica body effects need')
    end=production.index('void pc_network_bots_pose_weapons(',start)
    source=source.replace('/* PRODUCTION_CAMO_PRESENT */',production[start:end])
    source=source.replace('/* CAMO_HEADER */',(ROOT/'src/network/pc_character_effects.h').read_text())
    source=source.replace('/* ORIGINAL_CONSTANTS */',seeds)
    assert 'PASS:' in compile_and_run(tmp_path,source)


def test_story_original_barrel_blast(tmp_path):
    from test_recomp_float_joins import compile_and_run
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    import re
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported local US executable required')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous=dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions={start:dict(name=f'sub_{start:08X}',end=end) for start,end in (
            (0xefbd0,0xeff3b),(0xedd40,0xefb15),(0xedbe0,0xedca3),(0xeff90,0xf042c))}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(a,info) for a,info in functions.items())
        constants={int(a,16) for a in re.findall(r'MEMF\(0x([0-9A-Fa-f]+)\)',code)}
        constants={a for a in constants if 0x470000<=a<0x500000}
        constants.update(range(0x4db490,0x4db490+49*24,4))
        seeds='\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08x}u;' for a in sorted(constants))
    finally:
        vars(config).update(previous)
    code=code.replace('void sub_000EDD40(void)\n{','void sub_000EDD40_gen(void)\n{')
    source=(ROOT/'tests/network/story_barrel_blast.c').read_text().replace('/* ORIGINAL_BLAST */',code)
    defined=set(re.findall(r'void (sub_[0-9A-F]+(?:_gen)?)\(void\)',source))
    missing=set(re.findall(r'\b(sub_[0-9A-F]+(?:_gen)?)\b',code))-defined
    source=source.replace('/* UNREACHED */','\n'.join('void '+name+'(void){assert(!"unexpected '+name+'");}' for name in sorted(missing)))
    production=(ROOT/'src/network/pc_story_effects.inc').read_text()
    source=source.replace('/* PRODUCTION_BLAST_GATES */',production[production.index('/* EDD40 creates'):])
    source=source.replace('/* ORIGINAL_CONSTANTS */',seeds)
    assert 'PASS:' in compile_and_run(tmp_path,source)


def test_story_original_barrel_authority(tmp_path):
    from test_recomp_float_joins import compile_and_run
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    import re
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported local US executable required')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous=dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions={start:dict(name=f'sub_{start:08X}',end=end) for start,end in (
            (0x218650,0x218736),(0xbb6c0,0xbb842),(0xbb490,0xbb6bf),(0xba230,0xba306),(0x35c870,0x35cb79),(0x35af40,0x35afa7),(0x355ec0,0x355f02),(0x35be90,0x35beec))}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(a,info) for a,info in functions.items())
        constants={int(a,16) for a in re.findall(r'MEMF\(0x([0-9A-Fa-f]+)\)',code)}
        constants={a for a in constants if 0x470000<=a<0x500000}
        constants.update(range(0x4db490,0x4db490+49*24,4))
        seeds='\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08x}u;' for a in sorted(constants))
    finally:
        vars(config).update(previous)
    code=code.replace('void sub_0035AF40(void)\n{','void sub_0035AF40_gen(void)\n{')
    source=(ROOT/'tests/network/story_barrel_authority.c').read_text().replace('/* ORIGINAL_BLAST */',code)
    defined=set(re.findall(r'void (sub_[0-9A-F]+(?:_gen)?)\(void\)',source))
    missing=set(re.findall(r'\b(sub_[0-9A-F]+(?:_gen)?)\b',code))-defined
    source=source.replace('/* UNREACHED */','\n'.join('void '+name+'(void){assert(!"unexpected '+name+'");}' for name in sorted(missing)))
    production=(ROOT/'src/network/pc_story_effects.inc').read_text()
    source=source.replace('/* PRODUCTION_BLAST_GATES */',production[production.index('/* Story host owns'):])
    source=source.replace('/* ORIGINAL_CONSTANTS */',seeds)
    assert 'PASS:' in compile_and_run(tmp_path,source)


def test_story_original_drop_constructor(tmp_path):
    import re
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
            (0xc1950,0xc2087),(0x358160,0x3581a4),(0x355ec0,0x355f02))}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(start,info) for start,info in functions.items())
    finally:
        vars(config).update(previous)
    source=(ROOT/'tests/network/story_drop_constructor.c').read_text().replace('/* ORIGINAL_CONSTRUCTOR */',code)
    defined=set(re.findall(r'void (sub_[0-9A-F]+)\(void\)',source))
    missing=set(re.findall(r'\b(sub_[0-9A-F]+)\b',code))-defined
    source=source.replace('/* UNREACHED */','\n'.join('void '+name+'(void){assert(!"unexpected '+name+'");}' for name in sorted(missing)))
    source=source.replace('/* PRODUCTION_DROP_STATE */',(ROOT/'src/network/pc_story_drops.inc').read_text())
    assert 'PASS:' in compile_and_run(tmp_path,source)


def test_story_original_corpse_copy(tmp_path):
    import re
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
            (0xb3630,0xb3909),(0x15c310,0x15c386),(0x27cbd0,0x27cc1c),
            (0x27cef0,0x27d48b),(0x27cb10,0x27cbc3))}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(start,info) for start,info in functions.items())
    finally:
        vars(config).update(previous)
    code=code.replace('void sub_0015C310(void)\n{','void sub_0015C310_gen(void)\n{')
    source=(ROOT/'tests/network/story_corpse_copy.c').read_text().replace('/* ORIGINAL_CORPSE_COPY */',code)
    defined=set(re.findall(r'void (sub_[0-9A-F]+(?:_gen)?)\(void\)',source))
    missing=set(re.findall(r'\b(sub_[0-9A-F]+(?:_gen)?)\b',code))-defined
    source=source.replace('/* UNREACHED */','\n'.join('void '+name+'(void){assert(!"unexpected '+name+'");}' for name in sorted(missing)))
    source=source.replace('/* CHARACTER_POSE_HEADER */',(ROOT/'src/network/pc_character_pose.h').read_text())
    source=source.replace('/* PRODUCTION_CORPSE_COPY */',(ROOT/'src/network/pc_story_corpses.inc').read_text())
    assert 'PASS:' in compile_and_run(tmp_path,source)


def test_network_original_room_cache(tmp_path):
    from test_recomp_float_joins import compile_and_run
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    import re
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported local US executable required')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous=dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions={0xa17c0:dict(name='sub_000A17C0',end=0xa1aa5),
                   0xa1ab0:dict(name='sub_000A1AB0',end=0xa1b79)}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(a,info) for a,info in functions.items())
    finally:
        vars(config).update(previous)
    missing=set(re.findall(r'\b(sub_[0-9A-F]+)\b',code))-{v['name'] for v in functions.values()}
    stubs='\n'.join('void '+name+'(void){assert(!"unexpected '+name+'");}' for name in sorted(missing))
    production=(ROOT/'src/interface/pc_display_menu.c').read_text()
    start=production.index('void pc_network_bots_entity_refresh(')
    refresh=production[start:production.index('/* Share the contiguous animation slice',start)]
    source=(ROOT/'tests/network/network_room_cache.c').read_text().replace('/* ORIGINAL_ROOM_CACHE */',code)
    source=source.replace('/* UNREACHED */',stubs).replace('/* PRODUCTION_REFRESH */',refresh)
    assert 'PASS:' in compile_and_run(tmp_path,source)


@pytest.mark.parametrize('without_fix', [False, True])
def test_network_original_primary_room(tmp_path,without_fix):
    """Reproduce the recorded room5/27 omission in the actual render lists."""
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
            (0xa1ea0,0xa1f96),(0x9e430,0x9e4aa),(0xa79b0,0xa79f9))}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(a,info) for a,info in functions.items())
    finally:
        vars(config).update(previous)
    production=(ROOT/'src/interface/pc_display_menu.c').read_text()
    start=production.index('void pc_network_bots_entity_room(')
    helper=production[start:production.index('/* Share the contiguous animation slice',start)]
    if without_fix:
        helper='void pc_network_bots_entity_room(uint32_t entity,uint32_t room){(void)entity;(void)room;}'
    source=(ROOT/'tests/network/network_primary_room.c').read_text().replace('/* ORIGINAL_RENDER_ROOMS */',code)
    source=source.replace('/* PRODUCTION_PRIMARY_ROOM */',helper)
    if without_fix:
        with pytest.raises(AssertionError,match=r'MEM32\(0x20034\)==5'):
            compile_and_run(tmp_path,source)
    else:
        assert 'PASS:' in compile_and_run(tmp_path,source)


def test_story_original_drop_wire(tmp_path):
    import re
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
        functions={0x3c9d90:dict(name='sub_003C9D90',end=0x3c9dd1),
                   0x35b190:dict(name='sub_0035B190',end=0x35b53a),
                   0xc2090:dict(name='sub_000C2090',end=0xc2d35)}
        translator=FunctionTranslator(image,functions)
        code='\n'.join(translator.translate_function(a,info) for a,info in functions.items())
    finally:
        vars(config).update(previous)
    code=code.replace('void sub_000C2090(void)\n{','void sub_000C2090_gen(void)\n{')
    source=(ROOT/'tests/network/story_drop_wire.c').read_text().replace('/* ORIGINAL_DROP_RECEIVER */',code)
    defined=set(re.findall(r'void (sub_[0-9A-F]+(?:_gen)?)\(void\)',source))
    missing=set(re.findall(r'\b(sub_[0-9A-F]+(?:_gen)?)\b',code))-defined
    stubs='\n'.join('void '+name+'(void){assert(!"unexpected '+name+'");}' for name in sorted(missing))
    source=source.replace('/* UNREACHED */',stubs)
    source=source.replace('/* PRODUCTION_REPLAY */',(ROOT/'src/network/pc_story_drops.inc').read_text().split('\n#ifdef _WIN32\n/* Completed')[0])
    assert 'PASS:' in compile_and_run(tmp_path,source)


def test_story_original_room_scissor(tmp_path):
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
        info=dict(name='sub_000862C0',end=0x86479)
        code=FunctionTranslator(image,{0x862c0:info}).translate_function(0x862c0,info)
    finally:
        vars(config).update(previous)
    production=(ROOT/'src/network/pc_story_npc.inc').read_text()
    adapter=production[production.index('void pc_story_network_replica_clip('):]
    source=(ROOT/'tests/network/story_room_clip.c').read_text().replace('/* ORIGINAL_ROOM_SCISSOR */',code)
    source=source.replace('/* PRODUCTION_REPLICA_CLIP */',adapter)
    assert 'PASS:' in compile_and_run(tmp_path,source)


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
    source=(ROOT/'tests/network/story_config_guest.c').read_text()
    output=compile_and_run(tmp_path,source.replace('/* ORIGINAL_STAGE_BUILDER */',code)
        .replace('/* ORIGINAL_MISSION_DATA */',constants)
        .replace('INTRO_SCENE',str(scene)).replace('OUTRO_SCENE',str(outro_scene)))
    assert 'PASS:' in output
