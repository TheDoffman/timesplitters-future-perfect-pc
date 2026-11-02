"""Execute decoded continuation instructions with production state/checkpoints."""
from pathlib import Path
import os
import subprocess

import pytest
from test_translation_function import translator
from test_translation_product import _compile
from translation_continuation import emit, emit_dispatch
from translation_decode import Disassembler

ROOT=Path(__file__).resolve().parents[2]


def generate(raw,start=0x1000):
    data=bytes.fromhex(raw)
    return emit(start,Disassembler().disassemble_function(data,start,start+len(data)))


def execute(tmp_path,programs,body,support='',mutation=None,refs=False,dispatch=False):
    names=('recomp_types.h','guest_status_flags.h','guest_state.c',
           'guest_context_execution.h','guest_context_execution.c','guest_x86_context.h')
    for name in names:(tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    generated='\n'.join(generate(raw,start) for start,raw in programs.items())
    if dispatch:
        generated+='\n'+emit_dispatch({start:Disassembler().disassemble_function(bytes.fromhex(raw),start,start+len(bytes.fromhex(raw))) for start,raw in programs.items()})
    if mutation:
        before,after=mutation;assert before in generated;generated=generated.replace(before,after)
    source=tmp_path/'continuation.c'
    source.write_bytes(('''
#define RECOMP_GENERATED_CODE 1
#define RECOMP_FAST_ENTRY(site) 0
'''+('#define RECOMP_GPR_REFS_SUPPORTED 1\n#define RECOMP_USE_GPR_REFS 1\n#define RECOMP_GPR_ANCHORS(site) 1\n' if refs else '')+'''
#include "recomp_types.h"
#include "guest_context_execution.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ptrdiff_t g_xbox_mem_offset;
static union {uint64_t alignment;unsigned char bytes[8192];} storage;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line%d: %s\\n",__LINE__,#x);exit(1);}}while(0)
'''+generated+support+'''
int main(void){
g_xbox_mem_offset=(ptrdiff_t)storage.bytes;
g_status_flags.value=2;g_status_flags.known=TSFP_STATUS_FLAGS|2;
g_esp=1000;
'''+body+'\nreturn 0;}\n').encode())
    flag='/DTSFP_RECORD_STATUS_FLAGS=1' if os.name=='nt' else '-DTSFP_RECORD_STATUS_FLAGS=1'
    exe=_compile(tmp_path,source,extra=[tmp_path/'guest_state.c',tmp_path/'guest_context_execution.c',flag])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)


def check(result):assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.parametrize('refs',[False,True])
def test_real_call_ret_stack_targets_and_return_classification(tmp_path,refs):
    check(execute(tmp_path,{
        0x1000:'e8fb0f0000c3', # CALL2000;RET
        0x2000:'b82a000000c20800',
        0x3000:'ff1424',       # CALL[ESP]
        0x4000:'ffe4',         # JMP ESP
        0x5000:'55 89e5 83ec04 c9 c3',
    },'''
uint32_t next=0xdead;
MEM32(1000)=0x7777;
CHECK(tsfp_context_step_00001000(0x1000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==0x2000 && g_esp==996 && MEM32(996)==0x1005);
CHECK(tsfp_context_step_00002000(next,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_eax==42 && next==0x2005);
CHECK(tsfp_context_step_00002000(next,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(next==0x1005 && g_esp==1008);
g_esp=1000;
CHECK(tsfp_context_step_00001000(0x1005,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(next==0x7777 && g_esp==1004);
g_esp=1000;MEM32(1000)=0x2000;
CHECK(tsfp_context_step_00003000(0x3000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==0x2000 && g_esp==996 && MEM32(996)==0x3003);
g_esp=1000;
CHECK(tsfp_context_step_00004000(0x4000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==1000 && g_esp==1000);
MEM32(1000)=0;g_ebp=0xabcdef;next=0x5000;
while(tsfp_context_step_00005000(next,&next)!=TSFP_CONTEXT_STEP_RETURN){}
CHECK(next==0 && g_esp==1004 && g_ebp==0xabcdef && MEM32(996)==0xabcdef);
''',refs=refs))


@pytest.mark.parametrize('refs',[False,True])
def test_enter_at_jcc_uses_restored_flags_without_reexecuting_compare(tmp_path,refs):
    check(execute(tmp_path,{0x1000:'39d8 7405 b801000000 c3',0x2000:'0f92c0 0f43d3 c3',0x3000:'67e102 90 c3'},'''
uint32_t next=0;g_eax=10;g_ebx=20;
g_status_flags.value=TSFP_FLAG_ZF|2;g_status_flags.known=TSFP_STATUS_FLAGS|2;
CHECK(tsfp_context_step_00001000(0x1002,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==0x1009 && g_eax==10 && (g_status_flags.value&TSFP_FLAG_ZF));
CHECK(tsfp_context_step_00001000(0x1000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==0x1002 && !(g_status_flags.value&TSFP_FLAG_ZF));
CHECK(tsfp_context_step_00001000(next,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==0x1004);
g_status_flags.value=3;g_status_flags.known=3;
CHECK(tsfp_context_step_00002000(0x2000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK((g_eax&255)==1 && next==0x2003);
g_status_flags.value=2;g_edx=7;g_ebx=42;
CHECK(tsfp_context_step_00002000(next,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_edx==42 && next==0x2006);
g_ecx=0x12340002;g_status_flags.value=66;g_status_flags.known=66;
CHECK(tsfp_context_step_00003000(0x3000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_ecx==0x12340001 && next==0x3005 && g_status_flags.value==66);
next=0xdead;g_status_flags.known=2;
CHECK(tsfp_context_step_00001000(0x1002,&next)==TSFP_CONTEXT_STEP_ERROR);
CHECK(next==0xdead && g_eax==1);
''',refs=refs))


def test_pc_membership_and_invalid_entry_preserve_complete_state(tmp_path):
    check(execute(tmp_path,{0x1000:'b801020304 c3'},'''
uint32_t next=0xdead;unsigned char before[sizeof storage];
g_eax=42;g_ebp=0x1234;memcpy(before,&storage,sizeof storage);
CHECK(tsfp_context_has_00001000(0x1000) && tsfp_context_has_00001000(0x1005));
for(uint32_t pc=0x1001;pc<0x1005;pc++){
 CHECK(!tsfp_context_has_00001000(pc));
 CHECK(tsfp_context_step_00001000(pc,&next)==TSFP_CONTEXT_STEP_ERROR);
}
CHECK(!tsfp_context_has_00001000(0));
CHECK(tsfp_context_step_00001000(0x1000,NULL)==TSFP_CONTEXT_STEP_ERROR);
CHECK(g_eax==42 && g_ebp==0x1234 && g_esp==1000 && next==0xdead);
CHECK(!memcmp(before,&storage,sizeof storage));
'''))


@pytest.mark.parametrize('refs',[False,True])
def test_decoded_steps_resume_an_edited_context_through_real_checkpoint(tmp_path,refs):
    check(execute(tmp_path,{0x1000:'b801000000 e8f66f0000 b802000000 c3'},'''
tsfp_x86_context_state initial={0};tsfp_context_execution_ops ops={0};
initial.r_eip=0x1000;initial.r_esp=1000;initial.r_eax=77;initial.r_eflags=2;
MEM32(1000)=0;ops.allow_pc=allow_pc;ops.allow_state=allow_state;
ops.apply=apply;ops.step=dispatch;ops.same_stack=same_stack;
CHECK(tsfp_context_execute(&ops,&initial,100,2000,0)==TSFP_CONTEXT_EXECUTE_COMPLETE);
CHECK(!old_after && transfers==1 && applications==2 && g_eax==0x87654321 && g_esp==1004);
CHECK(!tsfp_context_execution_depth());
''',support='''
static unsigned old_after,transfers,applications;
static int allow_pc(void *p,uint32_t pc){(void)p;return pc==0x8000 || tsfp_context_generated_admit(pc);}
static int allow_state(void *p,const tsfp_x86_context_state *s){(void)p;return s->r_esp>=100 && s->r_esp<=1996;}
static int same_stack(void *p,uintptr_t tag){(void)p;return tag!=0;}
static void apply(void *p,const tsfp_x86_context_state *s){
 (void)p;++applications;g_eax=s->r_eax;g_esp=s->r_esp;g_ebp=s->r_ebp;
 g_status_flags.value=s->r_eflags;g_status_flags.known=TSFP_STATUS_FLAGS|2;
}
static int dispatch(void *p,uint32_t pc,uint32_t *next){
 (void)p;if(pc!=0x8000)return tsfp_context_generated_step(pc,next);
 {tsfp_x86_context_state edited={0};++transfers;CHECK(g_eax==1 && g_esp==996 && MEM32(996)==0x100a);
  edited.r_eip=0x100f;edited.r_esp=1000;edited.r_eax=0x87654321;edited.r_eflags=2;
  (void)tsfp_context_transfer(&edited);++old_after;return TSFP_CONTEXT_STEP_ERROR;}
}
''',dispatch=True,refs=refs))


@pytest.mark.parametrize('raw',['66c3','cb','66ffd0','ea001000000800','0f0b','8cd8'])
def test_unmodeled_control_selector_and_opcode_forms_fail_explicitly(raw):
    with pytest.raises(ValueError):generate(raw)


def test_opt_in_function_method_preserves_normal_translation_and_lifter_state(monkeypatch):
    t,info=translator('39d87405b801000000c3')
    before=t.translate_function(0x1000,info)
    old={name:repr(value) for name,value in vars(t.lifter).items()}
    monkeypatch.setattr('translation_helpers.cxx_throw',lambda *a,**k:pytest.fail('Repeated whole-image helper discovery'))
    generated=t.translate_context_function(0x1000,info)
    assert 'case 0x00001002u:' in generated
    assert old=={name:repr(value) for name,value in vars(t.lifter).items()}
    assert before==t.translate_function(0x1000,info)
    t.force_returns[0x1000]=42
    with pytest.raises(ValueError,match='policy adapter'):t.translate_context_function(0x1000,info)


@pytest.mark.parametrize('raw',[
    '01d8 11d8 19d8 40 48 f7d8 c3',
    'c1e000 d1e0 d1e8 d1f8 c3',
    'd1c0 d1c8 d1d0 d1d8 c3',
    '0fa4d803 0facd803 c3',
    '0fbaf001 0fbae802 0fbaf803 c3',
    '0fbcc3 0fbdcb c3',
    'f7e3 0fafc3 6bc0f9 c3',
    '0fc1d8 0fb1d8 c3',
    '660fc1d8 660fb1d8 c3',
    '39d8 0f92c1 0f43d3 c3',
    '9e 9f c3',
    'f3a4 c3',
    'f3a6 c3',
    'd9e8 d9ee dff1 ddd8 c3',
    '0f2ec1 c3',
    '660f2ec1 c3',
])
def test_cold_steps_match_normal_production_lifters_registers_flags_and_ram(tmp_path,raw):
    t,info=translator(raw)
    normal=t.translate_function(0x1000,info)
    check(execute(tmp_path,{0x1000:raw},'''
unsigned char memory[sizeof storage];uint32_t before[60],after[60],next=0x1000;
initialize();guest_body();capture(before);memcpy(memory,&storage,sizeof storage);
initialize();
for(unsigned limit=0;;limit++){
 CHECK(limit<100);int event=tsfp_context_step_00001000(next,&next);
 CHECK(event!=TSFP_CONTEXT_STEP_ERROR);if(event==TSFP_CONTEXT_STEP_RETURN)break;
}
capture(after);for(unsigned i=0;i<60;i++)if(before[i]!=after[i])fprintf(stderr,"state[%u] normal=%08x cold=%08x\\n",i,before[i],after[i]);CHECK(!memcmp(before,after,sizeof before));
CHECK(!memcmp(memory,&storage,sizeof storage));CHECK(next==0x7777);
''',support=normal+'''
static void initialize(void){
 memset(&storage,0xa5,sizeof storage);g_eax=0x12345678;g_ebx=7;g_ecx=3;g_edx=0;
 g_esi=300;g_edi=400;g_esp=1000;g_ebp=0x1234;g_df=1;
 MEM32(1000)=0x7777;g_status_flags.value=3;g_status_flags.known=TSFP_STATUS_FLAGS|2;
 g_fp_top=0;g_fp_cmp=0;for(unsigned i=0;i<8;i++)g_fp_stack[i]=0.0;
 g_xmm0.d[0]=1.0;g_xmm1.d[0]=2.0;
}
static void capture(uint32_t *v){
 v[0]=g_eax;v[1]=g_ebx;v[2]=g_ecx;v[3]=g_edx;v[4]=g_esi;v[5]=g_edi;
 v[6]=g_esp;v[7]=g_ebp;v[8]=g_status_flags.value;v[9]=g_status_flags.known;
 v[10]=g_fp_top;v[11]=(uint32_t)g_fp_cmp;
 memcpy(v+12,g_fp_stack,sizeof g_fp_stack);
 memcpy(v+28,&g_xmm0,sizeof g_xmm0);
 memcpy(v+32,&g_xmm1,sizeof g_xmm1);
 memcpy(v+36,&g_xmm2,sizeof g_xmm2);
 memcpy(v+40,&g_xmm3,sizeof g_xmm3);
 memcpy(v+44,&g_xmm4,sizeof g_xmm4);
 memcpy(v+48,&g_xmm5,sizeof g_xmm5);
 memcpy(v+52,&g_xmm6,sizeof g_xmm6);
 memcpy(v+56,&g_xmm7,sizeof g_xmm7);

}
'''))


def test_cold_ebp_is_the_architectural_cell_through_frame_setup_and_restore(tmp_path):
    # Normal fast C keeps a function-local EBP and a published frame cache. Its
    # stale global after POP EBP is not an architectural-state oracle here.
    check(execute(tmp_path,{0x1000:'55 89e5 8d6c2408 896c2414 5d c3'},'''
uint32_t next=0;g_ebp=0x1234;
CHECK(tsfp_context_step_00001000(0x1000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_esp==996 && MEM32(996)==0x1234 && g_ebp==0x1234);
CHECK(tsfp_context_step_00001000(next,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_ebp==996);
CHECK(tsfp_context_step_00001000(next,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_ebp==1004);
CHECK(tsfp_context_step_00001000(next,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(MEM32(1016)==1004);
CHECK(tsfp_context_step_00001000(next,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_ebp==0x1234 && g_esp==1000);
'''))


def test_all_architectural_conditions_at_instruction_entries(tmp_path):
    programs={0x1000+16*c:bytes((0x70+c,2,0x90,0x90,0xc3)).hex() for c in range(16)}
    expected=[];values=[]
    for bits in range(64):
        cf,pf,af,zf,sf,of=[(bits>>n)&1 for n in range(6)]
        values.append(2|cf|pf*4|af*16|zf*64|sf*128|of*2048)
        base=(of,cf,zf,cf or zf,sf,pf,sf!=of,zf or sf!=of)
        expected.append([int(base[c//2])^(c&1) for c in range(16)])
    support='static int (*const steps[])(uint32_t,uint32_t *)={'+','.join(f'tsfp_context_step_{a:08X}' for a in programs)+'};\n'
    support+='static const uint32_t values[]={'+','.join(map(str,values))+'};\n'
    support+='static const unsigned char expected[64][16]={'+','.join('{'+','.join(map(str,row))+'}' for row in expected)+'};\n'
    check(execute(tmp_path,programs,'''
for(unsigned row=0;row<64;row++)for(unsigned c=0;c<16;c++){
 uint32_t next=0,pc=0x1000+c*16;g_status_flags.value=values[row];
 CHECK(steps[c](pc,&next)==TSFP_CONTEXT_STEP_YIELD);
 CHECK(next==pc+(expected[row][c]?4:2));CHECK(g_status_flags.value==values[row]);
}
''',support=support))


@pytest.mark.parametrize('mutation',[None,
    ('return TSFP_CONTEXT_STEP_RETURN;','return TSFP_CONTEXT_STEP_YIELD;'),
    ('esp += 4u;','esp += 8u;'),
    ('#define ebp g_ebp','#define ebp g_seh_ebp'),
    ('tsfp_status_condition(&g_status_flags,4,0)','tsfp_status_condition(&g_status_flags,5,0)'),
    ('default:return 0;','default:return 1;'),
])
def test_compiled_incorrect_continuations_rejected(tmp_path,mutation):
    result=execute(tmp_path,{0x1000:'39d8 7405 b801000000 c3',0x2000:'bd44332211 c3'},'''
uint32_t next=0;CHECK(!tsfp_context_has_00001000(0x1001));
g_status_flags.value=66;g_status_flags.known=66;
CHECK(tsfp_context_step_00001000(0x1002,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(next==0x1009);
CHECK(tsfp_context_step_00002000(0x2000,&next)==TSFP_CONTEXT_STEP_YIELD);
CHECK(g_ebp==0x11223344 && next==0x2005);
MEM32(1000)=0x7777;
CHECK(tsfp_context_step_00002000(next,&next)==TSFP_CONTEXT_STEP_RETURN);
CHECK(next==0x7777 && g_esp==1004);
''',mutation=mutation)
    if mutation:assert result.returncode!=0,'Compiled incorrect continuation provider passed'
    else:check(result)


def test_owned_dispatch_calls_returns_and_rejects_gaps_without_state_writes(tmp_path):
    check(execute(tmp_path,{0x1000:'e8fb0f0000 83c001 c3',0x2000:'b829000000 c3'},'''
uint32_t next=0x1000;MEM32(1000)=0;
CHECK(!tsfp_context_generated_admit(0) && !tsfp_context_generated_admit(0x1001));
CHECK(!tsfp_context_generated_admit(0x1010) && !tsfp_context_generated_admit(0x1fff));
CHECK(!tsfp_context_generated_admit(0x2001) && !tsfp_context_generated_admit(0xffffffff));
for(unsigned count=0;;count++){
 CHECK(count<10 && tsfp_context_generated_admit(next));
 int event=tsfp_context_generated_step(next,&next);
 CHECK(event!=TSFP_CONTEXT_STEP_ERROR);if(event==TSFP_CONTEXT_STEP_RETURN && !next)break;
}
CHECK(g_eax==42 && g_esp==1004);next=0xdead;
CHECK(tsfp_context_generated_step(0x1001,&next)==TSFP_CONTEXT_STEP_ERROR);
CHECK(tsfp_context_generated_step(0x1000,NULL)==TSFP_CONTEXT_STEP_ERROR);
CHECK(g_eax==42 && g_esp==1004 && next==0xdead);
''',dispatch=True))


def test_dispatch_rejects_overlapping_ownership_and_empty_inputs():
    decoder=Disassembler()
    first=decoder.disassemble_function(bytes.fromhex('b801020304c3'),0x1000,0x1006)
    overlap=decoder.disassemble_function(bytes.fromhex('90c3'),0x1003,0x1005)
    for functions in ({},{0x1000:[]},{0x1000:first,0x1003:overlap},{0x999:first}):
        with pytest.raises(ValueError):emit_dispatch(functions)


def test_opt_in_bundle_uses_function_recovery_and_keeps_normal_output_identical():
    t,info=translator('e8fb0f0000c3')
    raw={0x1000:bytes.fromhex('e8fb0f0000c3'),0x2000:bytes.fromhex('b82a000000c3')}
    functions={a:dict(end=a+len(b),name=f'guest_{a:x}') for a,b in raw.items()}
    t.func_db=functions;t._read_func_bytes=lambda start,end:raw[start]
    before=t.translate_function(0x1000,functions[0x1000])
    generated=t.translate_context_functions(functions)
    assert 'int tsfp_context_generated_step(' in generated
    assert 'tsfp_context_has_00001000' in generated and 'tsfp_context_has_00002000' in generated
    assert before==t.translate_function(0x1000,functions[0x1000])
