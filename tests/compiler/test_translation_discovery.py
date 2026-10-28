"""Synthetic entry-discovery graphs and execution of recovered guest code."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_discovery as own
import translation_layout as layout
from translation_decode import Disassembler
from test_translation_lifting import installed
from test_translation_product import _compile


@pytest.fixture(autouse=True)
def mapped_image(monkeypatch):
    monkeypatch.setattr(layout,'_SECTIONS',(
        layout.Section('.text',0x1000,0x300,0,0x300,True),
        layout.Section('.rdata',0x2000,0x100,0x300,0x100,False)))


def metadata(start,end,**extra):
    return dict(_addr=start,start=hex(start),end=end,section='.text',**extra)


def image(*pieces):
    data=bytearray(b'\xcc'*0x400)
    for address,raw in pieces:
        value=bytes.fromhex(raw) if isinstance(raw,str) else raw
        offset=layout.va_to_file_offset(address);data[offset:offset+len(value)]=value
    return bytes(data)


def decode(raw,address=0x1000):
    data=bytes.fromhex(raw)
    return Disassembler().disassemble_function(data,address,address+len(data))


def context(data=None,db=None,**kwargs):
    return own.FunctionDiscovery(data or image(),db if db is not None else {},**kwargs)


def split_graph():
    data=image((0x1000,'85c9751cff248580100000'),(0x1020,'b80b000000c3'),
               (0x1040,'b816000000c3'),(0x1100,'c3'),
               (0x1080,struct.pack('<III',0x1020,0x1040,0)))
    db={0x1000:metadata(0x1000,0x100b,called_by=[0x1200],name='owner'),
        0x1020:metadata(0x1020,0x1026),0x1040:metadata(0x1040,0x1046),
        0x1060:metadata(0x1060,0x1061),0x1100:metadata(0x1100,0x1101,has_prologue=True)}
    return data,db


def test_context_identity_isolation_frame_and_convention():
    db={};labels={};abi={};sites={1:[2]};forced={1:2};trace=[1]
    first=context(db=db,label_db=labels,abi_db=abi,icall_sites=sites,force_returns=forced,trace_functions=trace,
                  trace_all_entries=True,seh_prolog=0x1040,seh_epilog=[])
    second=context()
    assert first.func_db is db and first.lifter.func_db is db
    assert first.label_db is labels and first.lifter.label_db is labels and first.abi_db is abi
    sites[3]=[4];forced[3]=4;trace.append(3)
    assert 3 not in first.icall_sites and 3 not in first.force_returns and first.trace_functions=={1}
    first.owned_function_starts.add(1);assert not second.owned_function_starts
    assert first._func_has_prologue(decode('5589e5c3')) and not first._func_has_prologue(decode('55c3'))
    assert first._func_owns_a_frame(decode('e83b000000c3'))
    assert not second._func_owns_a_frame(decode('e83b000000c3'))
    for info in ({'name':'x_thiscall_y'},{'calling_convention':'thiscall'}):
        assert first._determine_calling_convention(info)=='thiscall'
    assert first._determine_calling_convention({})=='cdecl'


def test_bounded_bytes_and_table_slots_reject_virtual_holes(monkeypatch):
    t=context(image((0x1000,'c20800')))
    assert t._read_func_bytes(0x1000,0x1003)==bytes.fromhex('c20800')
    for a,b in ((0xfff,0x1001),(0x1000,0x1000),(0x1001,0x1000),(0x12ff,0x1301)):
        assert t._read_func_bytes(a,b) is None
    monkeypatch.setattr(layout,'_SECTIONS',(layout.Section('.a',0x1000,2,0,2,True),layout.Section('.b',0x1003,3,3,3,True)))
    assert t._read_func_bytes(0x1000,0x1004) is None and t._word(0x1000) is None
    monkeypatch.setattr(layout,'_SECTIONS',(layout.Section('.a',0x1000,2,0,2,True),layout.Section('.b',0x1002,3,2,3,True)))
    assert t._read_func_bytes(0x1000,0x1003)==bytes.fromhex('c20800')
    monkeypatch.setattr(layout,'_SECTIONS',(layout.Section('.a',0x1000,2,0,2,True),layout.Section('.b',0x1002,3,20,3,True)))
    assert t._read_func_bytes(0x1000,0x1003) is None


def test_callback_bounds_aliases_and_clobbers():
    probe='be00200000bf0820000039feffd0c3'
    assert own.FunctionDiscovery._find_static_indirect_ranges(decode(probe))==[(0x2000,0x2008)]
    assert own.FunctionDiscovery._find_static_indirect_ranges(decode(probe.replace('ffd0','90')))==[]
    assert own.FunctionDiscovery._find_static_indirect_ranges(decode(probe),max_bytes=4)==[]
    assert own.FunctionDiscovery._find_static_indirect_ranges(decode('be0020000089f3bf0820000039fbffd0'))==[(0x2000,0x2008)]
    for clobber in ('46','8b36','31f6','87f3','66be0000','6646'):
        assert own.FunctionDiscovery._find_static_indirect_ranges(decode('be00200000bf08200000'+clobber+'39feffd0'))==[]


def test_callback_candidates_require_mapped_gap_return_and_section_evidence():
    data=image((0x1000,'be00200000bf1020000039feffd0c3'),(0x1050,'5589e5c3'),
               (0x1100,'c3'),(0x2000,struct.pack('<IIII',0,0x1050,0xffffffff,0x1050)))
    db={0x1000:metadata(0x1000,0x100f),0x1100:metadata(0x1100,0x1101)}
    t=context(data,db,label_db={0x1050:'callback'})
    assert t.discover_static_indirect_targets()=={0x1050}
    assert db[0x1050]['name']=='callback' and db[0x1050]['called_by']==[0x1000]
    assert db[0x1050]['end']==0x1100 and db[0x1050]['has_prologue']
    assert t._read_static_callback_table(0x2000,0x2010,[0x1000,0x1100])==[0x1050,0x1050]
    db[0x1100]['section']='.rdata'
    assert t._read_static_callback_table(0x2000,0x2010,[0x1000,0x1100]) is None
    assert t._read_static_callback_table(0x2001,0x2010,[0x1000,0x1100]) is None
    assert t._read_static_callback_table(0x2000,0x12004,[0x1000,0x1100]) is None
    assert t._read_static_callback_table(0x2000,0x2010,[]) is None


def test_local_and_bounded_tables_keep_order_duplicates_and_biased_slots():
    t=context(image((0x2000,struct.pack('<IIIIIII',0x1010,0x1020,0x1020,0,0,0x1030,0x1040))))
    assert t._read_local_jump_table(0x2004,0x1000,0x1100)==[0x1010,0x1020,0x1020]
    assert t._read_local_jump_table(0x200c,0x1000,0x1100)==[0x1010,0x1020,0x1020]
    assert t._read_local_jump_table(0x2010,0x1000,0x1100)==[0x1030,0x1040]
    assert t._read_local_jump_table(0x2018,0x1000,0x1100,max_entries=1)==[0x1030,0x1040]
    assert t._read_local_jump_table(0x2000,0x1000,0x1100,max_entries=1)==[]
    assert t._read_bounded_jump_table(0x200c,0x1000,0x2020)==[]
    assert t._read_bounded_jump_table(0x2010,0x1000,0x201c)==[0x1030,0x1040]
    assert t._read_local_jump_table(0x20ff,0x1000,0x1100)==[]


def test_cfg_fixed_point_reclaims_only_reached_weak_seeds():
    data,db=split_graph();t=context(data,db)
    t.discover_cfg_ownership()
    assert t.owned_function_starts=={0x1020,0x1040}
    recovered=t._recovered_cfg[0x1000]
    assert recovered['end']==0x1046 and recovered['jump_tables']=={0x1080:[0x1020,0x1040]}
    assert {i.address for i in recovered['instructions']}=={0x1000,0x1002,0x1004,0x1020,0x1025,0x1040,0x1045}
    t.disasm=None;t.discover_cfg_ownership() # Completed discovery is idempotent.
    assert 0x1060 not in t.owned_function_starts


def test_missing_bridge_does_not_claim_neighbor_and_partial_arms_fail():
    data,db=split_graph();data=bytes.fromhex('90909090')+data[4:]
    t=context(data,db);t.discover_cfg_ownership();assert not t.owned_function_starts
    assert not t._arm_is_whole(0x1000,[])
    assert not t._arm_is_whole(0x1000,decode('90'))
    assert not t._arm_is_whole(0x1000,decode('0f94c0c3'))
    assert not t._arm_is_whole(0x1000,decode('7501c3c3'))
    assert t._arm_is_whole(0x1000,decode('85c07501c3c3'))
    assert t._arm_is_whole(0x1000,decode('e8fb000000c3')) # known callable 0x1100
    t.owned_function_starts.add(0x1100)
    assert not t._arm_is_whole(0x1000,decode('e8fb000000c3'))
    assert not t._arm_is_whole(0x1000,decode('e8fb020000c3'))


def test_standalone_switch_entries_have_closed_paths():
    data=image((0x1000,'ff248580100000'),(0x1020,'b80b000000c3'),(0x1040,'b816000000c3'),
               (0x1080,struct.pack('<III',0x1020,0x1040,0)))
    t=context(data,{0x1000:metadata(0x1000,0x1007)})
    assert t.discover_jump_table_entries()=={0x1020,0x1040}
    assert t.func_db[0x1020]['called_by']==[0x1000] and t.func_db[0x1020]['end']==0x1026
    assert t.func_db[0x1040]['detection_method']=='jump_table_arm'
    assert t.discover_jump_table_entries()=={0x1020,0x1040}
    partial=bytearray(data);partial[0x40:0x46]=bytes.fromhex('0f94c0c39090')
    bad=context(bytes(partial),{0x1000:metadata(0x1000,0x1007)})
    assert bad.discover_jump_table_entries()=={0x1020}


@pytest.mark.parametrize('raw,wanted',[('90c20800',8),('c3',0),('750090c20c00',12),
    ('e800000000c20800',0),('eb00c20800',0),('90',0)])
def test_stub_cleanup_has_bounded_straight_line_evidence(raw,wanted):
    t=context(image((0x1000,raw)))
    assert t._stub_ret_bytes(0x1000)==wanted
    assert t._stub_ret_bytes(0x1000,max_insns=0)==0 and t._stub_ret_bytes(0x1300)==0
    if raw.startswith('90'): assert t._stub_ret_bytes(0x1000,max_insns=1)==0


def test_stub_cleanup_clips_to_remaining_file_bytes():
    t=context(b'\x90'*16+bytes.fromhex('c20800'))
    assert t._stub_ret_bytes(0x1010)==8
    assert t._stub_ret_bytes(0x1012)==0


def test_installed_recovered_function_executes_both_table_arms(tmp_path):
    data,db=split_graph()
    script=f'''
import json
from tools.recomp.translator import FunctionTranslator
import translation_layout as layout
import translation_discovery as own
for name in ('__init__','_recover_cfg','discover_cfg_ownership','discover_jump_table_entries','_read_func_bytes','_stub_ret_bytes'):
 assert getattr(FunctionTranslator,name) is getattr(own.FunctionDiscovery,name),name
layout._install([layout.Section('.text',0x1000,0x300,0,0x300,True)],0x1000,0x1000,'test')
data=bytes.fromhex({data.hex()!r});db={{int(k):v for k,v in json.loads({json.dumps(db)!r}).items()}}
t=FunctionTranslator(data,db);t.discover_cfg_ownership();assert t.owned_function_starts=={{0x1020,0x1040}}
print(json.dumps(t.translate_function(0x1000,db[0x1000])))
'''
    generated=json.loads(installed(script))
    code='''#include <stdint.h>
static uint32_t eax,ecx,esp,g_ebp,g_seh_ebp,memory[0x1100/4],unexpected_tail;
#define MEM32(address) memory[(address)/4]
#define RECOMP_ITAIL(address) (unexpected_tail=(address))
#define RECOMP_FAST_ENTRY(site) 0
'''+generated+'''
int main(void){memory[0x1080/4]=0x1020;memory[0x1084/4]=0x1040;ecx=0;eax=0;esp=100;owner();if(eax!=11||esp!=104)return 1;
ecx=0;eax=1;esp=100;owner();if(eax!=22||esp!=104)return 2;
ecx=1;eax=99;esp=100;owner();return eax!=11||esp!=104||unexpected_tail;}
'''
    # The independent TEST/branch emission needs no legacy condition macros.
    source=tmp_path/'discovery.c';source.write_text(code,encoding='utf-8')
    assert subprocess.run([str(_compile(tmp_path,source))],timeout=30).returncode==0


def test_toolkit_free_context_and_discovery(tmp_path):
    sources=list((ROOT/'tools/compiler').glob('translation_*'))
    sources += [ROOT/'tools/compiler'/name for name in ('c_source_scan.py','x86_decode.py','xbe_image.py','generation_output.py')]
    for source in sources:
        if source.is_file(): shutil.copyfile(source,tmp_path/source.name)
    script='import sys;sys.path.insert(0,sys.argv[1]);from translation_discovery import FunctionDiscovery;f=FunctionDiscovery(b"",{});assert not f.discover_static_indirect_targets();assert not any(n.startswith("tools.recomp") for n in sys.modules)'
    result=subprocess.run([sys.executable,'-I','-c',script,str(tmp_path)],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stderr
