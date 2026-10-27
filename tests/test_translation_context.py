"""Original context, bounded helper signatures and SDK-derived naming contracts."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from types import SimpleNamespace as S

import pytest

ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import generate_host_symbols as inventory
import translation_context as own
import translation_helpers as helpers
import translation_layout as layout
import translation_names as names
from test_translation_product import _compile
from test_translation_lifting import installed


@pytest.fixture(autouse=True)
def isolated_layout():
    before=dict(vars(layout));yield;vars(layout).update(before)


def image():
    layout._install([layout.Section('.text',0x1000,2048,64,2048,True)],0x1000,0,'own-helper-fixture')
    return bytearray(2112)


def place(data,address,body):
    offset=address-0x1000+64;data[offset:offset+len(body)]=body
    return {'size':len(body),'end':f'{address+len(body):X}'}


def test_sdk_inventory_parser_provenance_and_mangling():
    fixture='''Dump of file path
     Exports
       ordinal    name
                  CreateThread
                  _Internal
                  Decorated@4
                  ?Cpp@@Something
                  MessageBoxW
  Summary
      1000 .text
'''
    assert inventory.exports(fixture)=={'CreateThread','_Internal','MessageBoxW'}
    data=(ROOT/'tools/translation_host_symbols.txt').read_bytes()
    report=json.loads((ROOT/'tools/translation_host_symbols.json').read_text(encoding='utf-8'))
    assert report['sdk_version']=='10.0.26100.0'
    assert report['output_sha256']==hashlib.sha256(data).hexdigest()
    entries=data.decode('ascii').splitlines()
    assert entries==sorted(set(entries)) and len(entries)==report['symbol_count']
    assert all(name.isascii() and name.isidentifier() for name in entries)
    assert len(report['libraries'])==453
    assert all(Path(row['name']).name==row['name'] and len(row['sha256'])==64 for row in report['libraries'])
    for name in ('CreateThread','QueryPerformanceCounter','MessageBoxW','onexit','isnan','_Atomic','malloc','asm'):
        assert names.identifier(0x1234,name)==name+'_00001234'
    assert names.identifier(0x1234,None)=='sub_00001234'
    assert names.identifier(0x1234,'game_handler')=='game_handler'
    assert names.identifier(0x1234,'bad-name')=='bad_name_00001234'
    assert names.identifier(0x1234,'123')=='guest_123_00001234'


def test_context_isolation_precedence_overrides_and_arguments():
    data=image();prolog=bytes.fromhex('64a1000000008d6c2410c3');epilog=bytes.fromhex('64890d00000000c951c3')
    db={0x1000:place(data,0x1000,prolog),0x1040:place(data,0x1040,epilog)}
    first=own.Lifter(db,xbe_data=data,seh_prolog=0,manual_functions=[0x1234],setjmp_fn=0,longjmp_fn=0x5678)
    assert first.SEH_PROLOGS=={0} and first.SEH_EPILOGS=={0x1040}
    assert first.SETJMP_FN==0 and first.LONGJMP_FN==0x5678
    assert own.Lifter(db,xbe_data=data,seh_epilog=[]).SEH_PROLOGS=={0x1000}
    second=own.Lifter();first.unimplemented['bad']=[1];first._pop_cache[2]=3;first.manual_functions.add(7)
    assert not second.unimplemented and not second._pop_cache and not second.manual_functions
    first.func_db[4]={'name':'generated','call_name':'wrapper'};first.label_db[4]='wrong'
    first.func_db[5]={'name':'CreateThread'};first.label_db[6]='GetLastError'
    assert first._call_target_name(4)=='wrapper'
    assert first._call_target_name(5)=='CreateThread_00000005'
    assert first._call_target_name(6)=='GetLastError_00000006'
    assert first._call_target_name(7)=='sub_00000007'
    assert first.referenced_calls[4]=='wrapper'
    first.abi_db[4]={'calling_convention':'thiscall','estimated_params':2}
    assert first._build_call_args(4)=='(void*)(uintptr_t)ecx, 0 /* a1 */, 0 /* a2 */'
    assert first._build_call_args(9)==''
    for module,bindings in own._HANDLERS.items():
        for name,handler in bindings.items():assert getattr(own.Lifter,name) is getattr(module,handler)


def test_seh_bounds_metadata_variants_and_partial_signatures(capsys):
    data=image();db={}
    for address,frame in ((0x1000,12),(0x1040,16)):
        db[address]=place(data,address,bytes.fromhex('64a1000000008d6c24')+bytes([frame])+b'\xc3')
    db[0x1080]=place(data,0x1080,bytes.fromhex('64890d00000000c951c3'))
    db[0x1100]=place(data,0x1100,bytes.fromhex('64a100000000c3'))
    db[0x1140]=place(data,0x1140,bytes.fromhex('8d6c2410c3'))
    db[0x1040]['size']='bad';db[0x1080].pop('size')
    assert helpers.seh(db,data,True)==((0x1000,0x1040),(0x1080,))
    assert '0x00001040' in capsys.readouterr().err
    db[0x1000]['size']=129;db[0x1040]['end']='bad';db[0x1080]['size']=65
    assert helpers.seh(db,data)==((),())
    assert helpers.seh(db,None)==((),())
    assert helpers.seh({0x1000:{'size':11}},data[:70])==((),())
    assert helpers.seh({0x1000:{'size':-1,'end':'1010'}},data)==((),())


def test_nonlocal_helper_order_zero_address_and_required_extent(capsys):
    data=image();db={}
    for address,body in ((0x1100,'c7422030324356c3'),(0x1040,'c7422030324356c3'),(0x1180,'3d30324356c3')):
        db[address]=place(data,address,bytes.fromhex(body))
    assert helpers.nonlocal_jumps(db,data,True)==(0x1040,0x1180)
    assert 'setjmp 0x00001040' in capsys.readouterr().err
    db[0x1040]['end']='bad';db[0x1180]['end']=0x1180+513
    assert helpers.nonlocal_jumps(db,data)==(0x1100,None)
    assert helpers.nonlocal_jumps({0x1100:{'size':8}},data)==(None,None)
    layout._install([layout.Section('.text',0,32,0,32,True)],0,0,'zero-address')
    zero=bytes.fromhex('c7422030324356c3')+bytes(24)
    assert helpers.nonlocal_jumps({0:{'end':8}},zero,True)==(0,None)
    assert '0x00000000' in capsys.readouterr().err


def test_cxx_record_and_reference_bounds():
    data=image();record=struct.pack('<8I',0xe06d7363,1,0,0,3,0x19930520,0,0)
    place(data,0x1600,record)
    body=b'\x90\x68'+struct.pack('<I',0x1600)+b'\xc3'
    info=place(data,0x1000,body)
    assert helpers.exception_record(data,0x1600)
    assert helpers.cxx_throw({0x1000:info},data)==0x1000
    assert helpers.cxx_throw({0x1000:{'end':'1007'}},data)==0x1000
    for field in (0,16,20):
        copy=bytearray(data);copy[64+0x600+field]^=1
        assert not helpers.exception_record(copy,0x1600)
        assert helpers.cxx_throw({0x1000:info},copy) is None
    assert helpers.cxx_throw({0x1000:{'size':257}},data) is None
    assert not helpers.exception_record(data[:64+0x600+31],0x1600)
    assert not helpers.exception_record(data,None)
    assert helpers.cxx_throw({},data) is None
    assert helpers.cxx_throw({0x1000:info},None) is None
    # Physically present bytes past the mapped section are not guest record data.
    layout._install([layout.Section('.text',0x1000,0x610,64,0x610,True)],0x1000,0,'truncated-mapping')
    assert not helpers.exception_record(data,0x1600)


def test_bridge_and_standalone_context(tmp_path):
    installed('''
from tools.recomp import lifter
import translation_context as own
assert lifter is own
assert lifter.Lifter.__module__=='translation_context'
assert lifter.detect_seh_helpers.__module__=='translation_helpers'
assert lifter._func_ident.__module__=='translation_names'
''')
    for path in list((ROOT/'tools').glob('translation_*.py'))+[ROOT/'tools'/name for name in ('translation_host_symbols.txt','generation_output.py','c_source_scan.py','x86_decode.py','xbe_image.py')]:
        shutil.copyfile(path,tmp_path/path.name)
    result=subprocess.run([sys.executable,'-I','-c',
        'import sys;sys.path.insert(0,sys.argv[1]);import translation_context as c;'
        'assert c.Lifter()._call_target_name(1)=="sub_00000001";'
        'assert not any(n.startswith("tools.recomp") for n in sys.modules)',str(tmp_path)],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stderr


def test_record_cannot_cross_virtual_holes_or_noncontiguous_backing():
    data=bytearray(64)
    data[8:40]=struct.pack('<8I',0xe06d7363,1,0,0,3,0x19930520,0,0)
    # Both endpoints map at the expected offsets, but the middle does not.
    layout._install([layout.Section('a',0x1000,16,0,16,True),
                     layout.Section('b',0x1020,32,32,32,True)],0x1000,0,'hole')
    assert layout.va_to_file_offset(0x1008)==8 and layout.va_to_file_offset(0x1027)==39
    assert not helpers.exception_record(data,0x1008)
    layout._install([layout.Section('a',0x1000,16,0,16,True),
                     layout.Section('b',0x1010,48,16,48,True)],0x1000,0,'adjacent')
    assert helpers.exception_record(data,0x1008)
    layout._install([layout.Section('a',0x1000,16,0,16,True),
                     layout.Section('b',0x1010,48,32,48,True)],0x1000,0,'different-backing')
    assert not helpers.exception_record(data,0x1008)


def test_all_reserved_c_names_compile_and_collision_control_fails(tmp_path):
    headers='#include <math.h>\n#include <stdlib.h>\n#include <string.h>\n#include <setjmp.h>\n#include <stdint.h>\n'
    source=tmp_path/'names.c'
    source.write_text(headers+'\n'.join(f'void {names.identifier(0x1234,name)}(void);' for name in sorted(names.RESERVED))+'\nint main(void){return 0;}\n',encoding='utf-8')
    assert subprocess.run([str(_compile(tmp_path,source))],timeout=30).returncode==0
    source.write_text(headers+'\nvoid malloc(void){}\nint main(void){return 0;}\n',encoding='utf-8')
    with pytest.raises(AssertionError):_compile(tmp_path,source)
