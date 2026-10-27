"""Crane vignette wrapper and optional execution of the original US geometry."""


import hashlib
import importlib.util
from pathlib import Path
import re
import struct
import sys
import pytest
from test_cutscene_letterbox import compile_and_run
ROOT=Path(__file__).resolve().parents[2]
RUNTIME=ROOT/'external/ts3FPrecomp'

def test_crane_wrapper_registration():
    path=RUNTIME/'tools/recomp/manual_scan.py'
    spec=importlib.util.spec_from_file_location('crane_manual_scan',path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    _,wrapped,_=module.scan(str(RUNTIME/'titles/futureperfect/src/recomp_manual.c'))
    assert 0x137010 in wrapped

@pytest.mark.parametrize('platform', ['TSFP_PC_CONTROLS','TSFP_NATIVE_DISPLAY'])
def test_original_us_crane_overlay(tmp_path,platform):
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():pytest.skip('Local supported US XBE required; never included in CI')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    sys.path.insert(0,str(RUNTIME))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous=dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        info={'end':0x137199,'name':'sub_00137010_gen'}
        routines=FunctionTranslator(image,{0x137010:info}).translate_function(0x137010,info)
        addresses=sorted({int(a,16) for a in re.findall(r'MEMF\(0x([0-9A-Fa-f]+)\)',routines) if 0x470000<=int(a,16)<0x490000})
        constants='\n'.join(f'MEM32(0x{a:X})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08X};' for a in addresses)
        ui=(ROOT/'src/interface/pc_display_ui.inc').read_text()
        functions=[]
        for name in ('static void ui_place','static int aim_full_view','void pc_display_aim_panel'):
            match=re.search(re.escape(name)+r'\([^\n]*\)\n\{.*?\n\}',ui,re.S)
            assert match;functions.append(match.group())
        manual=(RUNTIME/'titles/futureperfect/src/recomp_manual.c').read_text()
        wrapper=re.search(r'void sub_00137010\(void\)\n\{.*?\n\}',manual,re.S).group()
        source=(ROOT/'tests/graphics/crane_original.c').read_text().replace('/* PRODUCTION */','\n'.join(functions))
        source=source.replace('/* ORIGINAL */',routines+'\n'+wrapper).replace('/* CONSTANTS */',constants)
        compile_and_run(tmp_path,'#define '+platform+'\n'+source)
    finally:vars(config).update(previous)


def test_native_crane_clip(tmp_path):
    source=(ROOT/'src/native/renderer_gl.c').read_text()
    match=re.search(r'static void native_apply_scissor\(void\) \{.*?\n\}',source,re.S)
    assert match
    fixture=(ROOT/'tests/graphics/crane_scissor.c').read_text()
    compile_and_run(tmp_path,fixture.replace('/* PRODUCTION */',match.group()))
