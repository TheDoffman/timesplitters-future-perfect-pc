"""Past Cortez's Ghost Gun reveal must stay registered to the world image."""


import hashlib
import re
import struct
import sys

import pytest

from test_cutscene_letterbox import ROOT, RUNTIME, compile_and_run


@pytest.mark.parametrize('mutation', ['none', 'old-placement', 'no-flush', 'no-restore'])
def test_actor_composite(tmp_path, mutation):
    production = (ROOT / 'src/interface/pc_display_ui.inc').read_text(encoding='utf-8')
    if mutation == 'old-placement':
        production = production.replace('caller==0x001232A9u', 'caller==0xFFFFFFFFu')
    elif mutation == 'no-flush':
        production = production.replace('guest_call(sub_00022FF0,0);', '')
    elif mutation == 'no-restore':
        production = production.replace('original();ui_place(old,old_tag);', 'original();')
    harness = (ROOT / 'tests/graphics/ui_layout.c').read_text(encoding='utf-8')
    harness = harness.replace('#include "pc_display_ui.inc"', production)
    fixture = (ROOT / 'tests/graphics/ghost_actor_overlay.c').read_text(encoding='utf-8')
    source = '#define main ui_suite\n' + harness + '\n#undef main\n' + fixture
    compile_and_run(tmp_path, source, fails=mutation != 'none')


@pytest.mark.parametrize('platform', ['TSFP_PC_CONTROLS', 'TSFP_NATIVE_DISPLAY'])
def test_actor_composite_runtime_hook(tmp_path, platform):
    manual = (RUNTIME / 'titles/futureperfect/src/recomp_manual.c').read_text(encoding='utf-8')
    wrapper = re.search(r'void sub_0001F090\(void\)\n\{.*?\n\}', manual, re.S)
    assert wrapper, 'Apply the complete patch stack first'
    source = '''
#include <stdio.h>
#include <stdlib.h>
static int calls;
void sub_0001F090_gen(void) { calls++; }
void pc_display_weapon_effect(void (*original)(void), unsigned kind) {
    if(kind!=0)exit(1);calls++;original();
}
/* WRAPPER */
int main(void) {
    sub_0001F090();if(calls!=2)return 1;
    puts("PASS: shared runtime composite wrapper");return 0;
}
'''
    compile_and_run(tmp_path, '#define ' + platform + '\n' + source.replace('/* WRAPPER */', wrapper.group()))


def test_original_us_actor_reveal(tmp_path):
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
        info = {'end': 0x1232C4, 'name': 'sub_00122F10_gen'}
        routine = FunctionTranslator(image, {0x122F10: info}).translate_function(0x122F10, info)
        addresses = sorted({int(a, 16) for a in re.findall(r'MEMF\(0x([0-9A-Fa-f]+)\)', routine)
                            if 0x470000 <= int(a, 16) < 0x490000})
        constants = '\n'.join(f'MEM32(0x{a:X})=0x{struct.unpack_from("<I", image, config.va_to_file_offset(a))[0]:08X};'
                              for a in addresses)
        ui = (ROOT / 'src/interface/pc_display_ui.inc').read_text(encoding='utf-8')
        functions = []
        for name in ('static void ui_place', 'static int aim_full_view', 'void pc_display_weapon_effect'):
            match = re.search(re.escape(name) + r'\([^\n]*\)\n\{.*?\n\}', ui, re.S)
            assert match
            functions.append(match.group())
        source = (ROOT / 'tests/graphics/ghost_actor_original.c').read_text(encoding='utf-8')
        source = source.replace('/* PRODUCTION */', '\n'.join(functions))
        source = source.replace('/* ORIGINAL */', routine).replace('/* CONSTANTS */', constants)
        compile_and_run(tmp_path, source)
    finally:
        vars(config).update(previous)
