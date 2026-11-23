"""Execute original US lobby gates with the shared one-player adapter."""
import hashlib
from pathlib import Path
import re
import sys

import pytest
from test_cutscene_letterbox import compile_and_run

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('host',['windows','native'])
@pytest.mark.parametrize('broken',[False,True])
def test_original_arcade_solo_start(tmp_path,host,broken):
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():pytest.skip('Local supported US XBE required; not distributed')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    sys.path.insert(0,str(ROOT/'external/ts3FPrecomp'))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous=dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions={a:dict(name=f'sub_{a:08X}',end=b) for a,b in
                   [(0x64ea0,0x64ecc),(0x199eb0,0x199ef3),(0x1cc2d0,0x1cc31b)]}
        translator=FunctionTranslator(image,functions)
        routines='\n'.join(translator.translate_function(a,f) for a,f in functions.items())
        routines=routines.replace('void sub_00064EA0(void)','void sub_00064EA0_gen(void)')
        source=(ROOT/'tests/network/arcade_solo_lobby.c').read_text()
        adapter=(ROOT/'src/network/pc_network_bots_lobby.inc').read_text()
        names=('pc_network_bots_lobby_network','pc_network_bots_guest_minimum')
        adapter='\n'.join(re.search(r'static (?:int|void) '+name+r'\(.*?\n\}',adapter,re.S).group() for name in names)
        if broken:
            needle='if(arcade_minimum)g_eax=1u;'
            assert needle in adapter
            adapter=adapter.replace(needle,'(void)arcade_minimum;')
        offset=config.va_to_file_offset(0x64ecc)
        constants='memcpy(mem+0x64ecc,(unsigned char[]){'+','.join(str(b) for b in image[offset:offset+33])+'},33);'
        source=source.replace('/* HOST */','#define _WIN32 1' if host=='windows' else '#define TSFP_NATIVE_NETWORK 1')
        source=source.replace('/* PRODUCTION_ADAPTER */',adapter).replace('/* ORIGINAL_ROUTINES */',routines).replace('/* ORIGINAL_CONSTANTS */',constants)
        compile_and_run(tmp_path,source,fails=broken)
    finally:vars(config).update(previous)
