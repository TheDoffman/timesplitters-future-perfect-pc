"""Run the supported US result builder, including its controller-only filter."""


import hashlib
from pathlib import Path
import re
import sys

import pytest
from test_cutscene_letterbox import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('host', ['windows', 'native'])
@pytest.mark.parametrize('original', [False, True])
@pytest.mark.parametrize('broken', [False, True])
def test_authoritative_door_animation(tmp_path, host, original, broken):
    source = (ROOT / 'tests/network/network_doors.c').read_text()
    guest = (ROOT / 'src/network/pc_network_doors_guest.inc').read_text()
    if broken:
        guest = guest.replace('&s->progress,4u);', '&s->extra,4u);')
    source = source.replace('/* HOST */', '#define _WIN32 1' if host == 'windows'
                            else '#define TSFP_NATIVE_NETWORK 1')
    source = source.replace('/* TRANSPORT */', (ROOT / 'src/network/pc_network_doors_transport.inc').read_text())
    source = source.replace('/* GUEST */', guest)
    if original:
        xbe = ROOT / 'private/game/default.xbe'
        if not xbe.is_file():
            pytest.skip('Local supported US XBE required; not distributed')
        image = xbe.read_bytes()
        assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
        sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))
        from tools.recomp import config
        from tools.recomp.translator import FunctionTranslator
        previous = dict(vars(config))
        try:
            config.configure_from_xbe(str(xbe))
            functions = {a: dict(name=f'sub_{a:08X}', end=b) for a, b in
                         [(0x359880, 0x3598fd), (0xb0dd0, 0xb0e30), (0xb0e30, 0xb0e91)]}
            translator = FunctionTranslator(image, functions)
            code = '\n'.join(translator.translate_function(a, f) for a, f in functions.items())
        finally:
            vars(config).update(previous)
        declarations = '\n'.join('void sub_'+f'{a:08X}'+'(void);' for a in functions)
        code = declarations + '\nvoid sub_00355F10(void){g_eax=1;g_esp+=4;}\n' + code
        source = '#define TSFP_TEST_ORIGINAL_DOORS 1\n' + source.replace('/* ORIGINAL_ROUTINES */', code)
    compile_and_run(tmp_path, source, fails=broken)


@pytest.mark.parametrize('broken', [False, True])
def test_original_bot_results(tmp_path, broken):
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Local supported US XBE required; not distributed')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        info = {'end': 0x64083, 'name': 'sub_00063EB0'}
        routine = FunctionTranslator(image, {0x63eb0: info}).translate_function(0x63eb0, info)
        adapter = (ROOT / 'src/network/pc_network_bots_guest.inc').read_text()
        names = ('pc_network_bots_guest_actor', 'pc_network_bots_guest_score_network')
        adapter = '\n'.join(re.search(r'static (?:uint32_t|void) '+name+r'\(.*?\n\}', adapter, re.S).group() for name in names)
        # Enable the shared adapter without impersonating the host OS: other
        # compiled helpers must still select native signal/exception APIs.
        adapter = '#define TSFP_NATIVE_NETWORK 1\n' + adapter
        if broken:
            adapter = adapter.replace('caller==0x00063f05u', 'caller==0xffffffffu')
        source = (ROOT / 'tests/network/network_bot_results.c').read_text()
        source = source.replace('/* PRODUCTION_ADAPTER */', adapter).replace('/* ORIGINAL_ROUTINE */', routine)
        compile_and_run(tmp_path, source, fails=broken)
    finally:
        vars(config).update(previous)


@pytest.mark.parametrize('broken', [False, True])
def test_original_bot_match_limit(tmp_path, broken):
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Local supported US XBE required; not distributed')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        info = {'end': 0x67cc6, 'name': 'sub_000674C0'}
        routine = FunctionTranslator(image, {0x674c0: info}).translate_function(0x674c0, info)
        adapter = (ROOT / 'src/network/pc_network_bots_guest.inc').read_text()
        names = ('pc_network_bots_guest_actor', 'pc_network_bots_guest_score_network')
        adapter = '\n'.join(re.search(r'static (?:uint32_t|void) '+name+r'\(.*?\n\}', adapter, re.S).group() for name in names)
        adapter = '#define TSFP_NATIVE_NETWORK 1\n' + adapter
        if broken:
            adapter = adapter.replace('caller==0x00067868u', 'caller==0xffffffffu')
        source = (ROOT / 'tests/network/network_bot_match_limit.c').read_text()
        implemented = set(re.findall(r'void (sub_[0-9A-F]+)\(', source))
        calls = set(re.findall(r'call 0x([0-9A-F]{8})', routine))
        stubs = '\n'.join('void sub_'+call+'(void) { CHECK(!"unexpected call '+call+'"); }'
                          for call in sorted(calls) if 'sub_'+call not in implemented)
        source = source.replace('/* UNREACHED_STUBS */', stubs)
        source = source.replace('/* PRODUCTION_ADAPTER */', adapter).replace('/* ORIGINAL_ROUTINE */', routine)
        compile_and_run(tmp_path, source, fails=broken)
    finally:
        vars(config).update(previous)


@pytest.mark.parametrize('broken', [False, True])
def test_original_gladiator_bot_selection(tmp_path, broken):
    import struct
    from guest_constants import absolute_data_words
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Local supported US XBE required; not distributed')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        ranges = [(0x64350,0x64451),(0x27c2f0,0x27c39b),(0x35a5c0,0x35a66f),
                  (0x35a670,0x35a6e5),(0x3568c0,0x356913)]
        functions = {a:dict(name=f'sub_{a:08X}',end=b) for a,b in ranges}
        translator = FunctionTranslator(image, functions)
        routines = '\n'.join(translator.translate_function(a,f) for a,f in functions.items())
        routines = routines.replace('void sub_003568C0(void)', 'void stock_active(void)')
        adapter = (ROOT / 'src/network/pc_network_bots_guest.inc').read_text()
        names = ('pc_network_bots_guest_active','pc_network_bots_guest_actor',
                 'pc_network_bots_guest_entity','pc_network_bots_guest_encode','pc_network_bots_guest_decode')
        adapter = '\n'.join(re.search(r'static (?:uint32_t|void) '+name+r'\(.*?\n\}', adapter, re.S).group() for name in names)
        adapter = '#define TSFP_NATIVE_NETWORK 1\n' + adapter
        if broken:
            adapter = adapter.replace('case 0x0035A609u:', 'case 0xffffffffu:')
        constants = absolute_data_words(image,ranges,config.va_to_file_offset)
        constants = '\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:x}u;' for a in constants)
        source = (ROOT / 'tests/network/gladiator_selection.c').read_text()
        source = source.replace('/* PRODUCTION_ADAPTER */',adapter).replace('/* ORIGINAL_ROUTINES */',routines).replace('/* CONSTANTS */',constants)
        compile_and_run(tmp_path,source,fails=broken)
    finally:
        vars(config).update(previous)
