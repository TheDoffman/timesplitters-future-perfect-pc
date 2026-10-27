"""Run the supported US result builder, including its controller-only filter."""


import hashlib
from pathlib import Path
import re
import sys

import pytest
from test_cutscene_letterbox import compile_and_run

ROOT = Path(__file__).resolve().parents[2]

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
        # Exercise the PC adapter with any host C compiler.
        adapter = '#ifndef _WIN32\n#define _WIN32 1\n#endif\n' + adapter
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
        adapter = '#ifndef _WIN32\n#define _WIN32 1\n#endif\n' + adapter
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
