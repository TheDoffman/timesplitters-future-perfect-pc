"""Check Story quick-menu health visibility, with optional original US code."""


import hashlib
from pathlib import Path
import re
import struct
import sys

import pytest
from test_cutscene_letterbox import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / 'external/ts3FPrecomp'


def fixture_source():
    manual = (RUNTIME / 'titles/futureperfect/src/recomp_manual.c').read_text(encoding='utf-8')
    wrapper = re.search(r'void sub_002E5750\(void\)\n\{.*?\n\}', manual, re.S)
    assert wrapper, 'Apply runtime patches before testing'
    return (ROOT / 'tests/graphics/objectives_health.c').read_text(encoding='utf-8').replace('/* PRODUCTION_WRAPPER */', wrapper.group())


@pytest.mark.parametrize('broken', [False, True])
def test_objectives_visibility_and_state(tmp_path, broken):
    source = fixture_source()
    if broken:
        source = source.replace('FP_MEM32(menu) &= ~0x100u;', '(void)menu;')
    compile_and_run(tmp_path, source, fails=broken)


def test_original_us_objectives_menu(tmp_path):
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
        info = {'end': 0x2E58F3, 'name': 'sub_002E5750_gen'}
        code = FunctionTranslator(image, {0x2E5750: info}).translate_function(0x2E5750, info)
        constants = sorted({int(a, 16) for a in re.findall(r'MEMF\(0x([0-9A-Fa-f]+)\)', code) if 0x470000 <= int(a, 16) < 0x4D0000})
        setup = '\n'.join(f'MEM32(0x{a:X})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08X};' for a in constants)
        source = '#define ORIGINAL_MENU\n' + fixture_source()
        source = source.replace('/* ORIGINAL_ROUTINE */', code).replace('/* ORIGINAL_CONSTANTS */', setup)
        compile_and_run(tmp_path, source)
    finally:
        vars(config).update(previous)


@pytest.mark.parametrize('broken', [False, True])
def test_automatic_objectives_layout(tmp_path, broken):
    manual = (RUNTIME / 'titles/futureperfect/src/recomp_manual.c').read_text(encoding='utf-8')
    wrapper = re.search(r'void sub_0013AB60\(void\)\n\{.*?\n\}', manual, re.S)
    assert wrapper
    assert len(re.findall(r'void sub_0013AB60\(void\)\n\{', manual)) == 1
    source = (ROOT / 'tests/graphics/objectives_layout.c').read_text(encoding='utf-8').replace('/* PRODUCTION_LAYOUT */', wrapper.group())
    panel = re.search(r'void sub_002F5E60\(void\)\n\{.*?\n\}', manual, re.S)
    assert panel
    source = source.replace('/* PRODUCTION_PANEL */', panel.group())
    if broken:
        source = source.replace('memcpy(FP_GUEST(frame + 16u), &adjusted, sizeof adjusted);', '(void)adjusted;')
    compile_and_run(tmp_path, source, fails=broken)


@pytest.mark.parametrize('fault', ['unscaled-panel', 'camera-is-not-viewport'])
def test_panel_spacing_regression(tmp_path, fault):
    manual = (RUNTIME / 'titles/futureperfect/src/recomp_manual.c').read_text(encoding='utf-8')
    source = (ROOT / 'tests/graphics/objectives_layout.c').read_text(encoding='utf-8')
    for name, marker in [('sub_0013AB60','PRODUCTION_LAYOUT'),('sub_002F5E60','PRODUCTION_PANEL')]:
        wrapper = re.search(r'void ' + name + r'\(void\)\n\{.*?\n\}', manual, re.S)
        assert wrapper
        source = source.replace('/* ' + marker + ' */', wrapper.group())
    # Reproduce the previous mismatch: fitted indicators, full-width panel.
    if fault == 'unscaled-panel':
        source = source.replace('width *= ratio;', 'width *= 1.f;')
    else:
        source = source.replace('if (view) view += 0x224u;', '(void)view;')
    compile_and_run(tmp_path, source, fails=True)
