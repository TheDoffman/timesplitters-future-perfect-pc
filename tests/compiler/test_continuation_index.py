"""Shared decoded entries and compiled block continuation execution."""
from copy import deepcopy
import pytest
from continuation_index import InstructionIndex
from translation_decode import Disassembler
from test_translation_continuation import execute,check


def decode(address,raw):
    data=bytes.fromhex(raw)
    return Disassembler().disassemble_function(data,address,address+len(data))


def test_shared_entries_and_alternate_instruction_streams_are_exact():
    index=InstructionIndex();first=decode(0x1000,'b89090c390c3')
    index.add(0x1000,first)
    index.add(0x1001,decode(0x1001,'9090c3'))
    assert set(index.points)=={0x1000,0x1001,0x1002,0x1003,0x1005}
    index.add(0x1005,decode(0x1005,'c3'))
    assert index.shared==1 and index.points[0x1005][0]==0
    assert index.functions==[0x1000,0x1001,0x1005]


@pytest.mark.parametrize('kind',['bytes','operands','duplicate','ordering'])
def test_conflicts_reject_atomically_without_damaging_existing_ownership(kind):
    index=InstructionIndex();index.add(0x1000,decode(0x1000,'90909090c3'))
    before=deepcopy(vars(index));entry=0x1002;rows=decode(entry,'9090c3')
    if kind=='bytes':rows=decode(entry,'90c3')
    if kind=='operands':rows[0].mnemonic='cld'
    if kind=='duplicate':rows.append(rows[0])
    if kind=='ordering':entry=0x1000;rows=decode(entry,'90909090c3')
    with pytest.raises(ValueError):index.add(entry,rows)
    assert vars(index)==before


@pytest.mark.parametrize('refs',[False,True])
def test_compiled_shared_tail_and_overlapping_decode_dispatch(tmp_path,refs):
    check(execute(tmp_path,{0x1000:'b89090c390c3',0x1001:'9090c3',0x1005:'c3'},'''
uint32_t next=0x1001;MEM32(1000)=0x7777;g_eax=42;
CHECK(tsfp_context_generated_admit(0x1001));CHECK(!tsfp_context_generated_admit(0x1004));
CHECK(tsfp_context_generated_step(next,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(next==0x7777 && g_eax==42 && g_esp==1004);
g_esp=1000;next=0x1000;
CHECK(tsfp_context_generated_step(next,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(g_eax==0x90c39090 && next==0x7777 && g_esp==1004);
''',refs=refs,dispatch=True,run=True))


@pytest.mark.parametrize('refs',[False,True])
def test_local_control_flow_runs_until_actual_return_not_an_intermediate_pc(tmp_path,refs):
    check(execute(tmp_path,{0x1000:'b803000000eb02b0ff4083f8057cfac3'},'''
uint32_t next=0;MEM32(1000)=0x1009;
CHECK(tsfp_context_generated_step(0x1000,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(g_eax==5 && next==0x1009 && g_esp==1004);
''',refs=refs,dispatch=True,run=True))


def test_calls_yield_and_return_addresses_route_between_compiled_blocks(tmp_path):
    check(execute(tmp_path,{0x1000:'e8fb0f000083c001c3',0x2000:'b829000000c3'},'''
uint32_t next=0;MEM32(1000)=0;
CHECK(tsfp_context_generated_step(0x1000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==0x2000 && g_esp==996 && MEM32(996)==0x1005);
CHECK(tsfp_context_generated_step(next,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(next==0x1005 && g_eax==41 && g_esp==1000);
CHECK(tsfp_context_generated_step(next,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(!next && g_eax==42 && g_esp==1004);
''',dispatch=True,run=True))


@pytest.mark.parametrize('run',[False,True])
def test_static_branch_policy_is_preserved_at_restored_instruction_entries(tmp_path,run):
    name='run' if run else 'step'
    check(execute(tmp_path,{0x1000:'39d87405b801000000c3'},f'''
uint32_t next=0;g_eax=7;g_ebx=8;g_status_flags.value=66;g_status_flags.known=66;MEM32(1000)=0;
CHECK(tsfp_context_{name}_00001000(0x1002,&next)==TSFP_CONTEXT_STEP_{'RETURN' if run else 'YIELD'});
CHECK(next=={'0' if run else '0x1004'} && g_eax=={'1' if run else '7'});
''',run=run,policy='#define RECOMP_FLOAT_BRANCH_AT(site,taken) ((site)==0x1002u ? 0 : (taken))\n'))
