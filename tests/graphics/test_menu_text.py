"""Physical keyboard policy and optional original US text editing routines."""


import hashlib
from pathlib import Path
import struct
import sys
import pytest
from test_cutscene_letterbox import compile_and_run, ROOT, RUNTIME


@pytest.mark.parametrize('mutation', ['none', 'per-row-query', 'stale-region'])
def test_menu_text(tmp_path, mutation):
    source = (ROOT / 'tests/graphics/menu_text.c').read_text(encoding='utf-8')
    if mutation != 'none':
        adapter = (ROOT / 'src/interface/pc_menu_text.inc').read_text(encoding='utf-8')
        old, new = ('if(pc_guest_region_remaining(&region,host,1)<0x10c) {', 'if(1) {') if mutation == 'per-row-query' else (
            'PcGuestRegion region={0};', 'static PcGuestRegion region={0};')
        assert old in adapter
        source = source.replace('#include "pc_menu_text.inc"', adapter.replace(old, new))
    compile_and_run(tmp_path, source, fails=mutation != 'none')


def test_original_keyboard(tmp_path):
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Local supported US XBE required; never included in CI')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    sys.path.insert(0, str(RUNTIME))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions = {start: {'end': end, 'name': f'sub_{start:08X}'} for start, end in (
            (0x7C1B0, 0x7C2D8), (0x7C300, 0x7C381), (0x7C390, 0x7C410),
            (0x7C410, 0x7C4C9), (0x7DB20, 0x7DD01))}
        routines = '\n'.join(FunctionTranslator(image, functions).translate_function(start, info)
                              for start, info in functions.items())
        constants = '\n'.join(f'MEM8(0x{a:X})={image[config.va_to_file_offset(a)]};'
                              for a in range(0x4D8DD0, 0x4D94D4))
        source = '#define ORIGINAL_KEYBOARD\n' + (ROOT / 'tests/graphics/menu_text.c').read_text(encoding='utf-8')
        source = source.replace('/* ORIGINAL_ROUTINES */', routines).replace('/* ORIGINAL_CONSTANTS */', constants)
        source += '\n' + (ROOT / 'src/runtime/guest_integer_fault.c').read_text(encoding='utf-8')
        compile_and_run(tmp_path, source)
    finally:
        vars(config).update(previous)
