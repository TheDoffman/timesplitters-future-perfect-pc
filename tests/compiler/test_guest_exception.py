"""Execute the original registration-chain provider against guest byte records."""
from pathlib import Path
import os,subprocess
import pytest
from test_translation_product import _compile
ROOT=Path(__file__).resolve().parents[2]


def execute(tmp_path,mutation=None):
    for name in ('guest_exception.c','guest_exception.h','guest_x86_context.c','guest_x86_context.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        p=tmp_path/'guest_exception.c';s=p.read_text(encoding='utf-8');a,b=mutation
        assert s.count(a)==1;s=s.replace(a,b);p.write_bytes(s.encode())
    source=tmp_path/'exception_contract.c';source.write_bytes((ROOT/'tests/compiler'/source.name).read_bytes())
    warnings=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,source,extra=[tmp_path/'guest_exception.c',tmp_path/'guest_x86_context.c',*warnings])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)


def test_original_raise_and_unwind_guest_records(tmp_path):
    r=execute(tmp_path);assert r.returncode==0,r.stdout+r.stderr


@pytest.mark.parametrize('mutation',[
    ('put(w.context+176,value);','put(w.context+176,value^1u);'),
    ('put(w.record+12,word(w.context+184));current=w.initial_head;','put(w.record+12,0);current=w.initial_head;'),
    ('put(w->guard,saved);','put(w->guard,UINT32_MAX);'),
    ('put(w->head,saved);','put(w->head,registration);'),
    ('current!=target && current!=UINT32_MAX','current!=UINT32_MAX'),
    ('if(word(w.record+4)&TSFP_EXCEPTION_NONCONTINUABLE)','if(0)'),
    ('state=w->baseline;', 'memset(&state,0,sizeof state);'),
    ('if(current==nested)', 'if(0)'),
    ('*out=state;return result;','state.r_eax=0;*out=state;return result;'),
])
def test_incorrect_compiled_exception_providers_fail(tmp_path,mutation):
    r=execute(tmp_path,mutation);assert r.returncode!=0,'Broken provider passed architectural contract'


@pytest.mark.parametrize('refs',[False,True])
def test_translated_handler_edits_context_and_resumes_real_guest_pc(tmp_path,refs):
    from test_translation_continuation import execute as translated,check
    from test_translation_function import translator
    # cdecl handler: edit CONTEXT EIP/EAX, then ContinueExecution.
    raw='8b44240c c780b800000014100000 c780b00000002a000000 31c0 c3'
    t,_=translator(raw);info={'end':0x3000+len(bytes.fromhex(raw)),'name':'exception_handler'}
    t.func_db={0x3000:info};handler=t.translate_function(0x3000,info)
    files=('guest_exception.c','guest_exception.h','guest_x86_context.c',
           'guest_callback_call.c','guest_callback_call.h')
    for name in files:(tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    support=handler+'''
#include "guest_exception.h"
#include "guest_callback_call.h"
static void *exception_memory(void *opaque,uint32_t address,uint32_t bytes,int write){
 (void)opaque;(void)write;
 return address && (uint64_t)address+bytes<=sizeof storage.bytes?storage.bytes+address:NULL;
}
static tsfp_guest_entry_t exception_lookup(void *opaque,uint32_t pc){
 (void)opaque;return pc==0x3000?exception_handler:NULL;
}
static void *exception_output(void *opaque,uint32_t address,uint32_t bytes){
 return exception_memory(opaque,address,bytes,1);
}
static int exception_admit(void *opaque,uint32_t pc){
 (void)opaque;return pc==0x3000 || pc==0x6000;
}
static int exception_call(void *opaque,uint32_t pc,uint32_t record,uint32_t frame,
                          uint32_t context,uint32_t dispatcher,uint32_t *result){
 tsfp_guest_callback_ops callbacks={NULL,exception_lookup,exception_output};
 uint32_t args[]={record,frame,context,dispatcher};(void)opaque;
 CHECK(MEM32(0x100)==0x380 && MEM32(0x380)==0x1800 && MEM32(0x388)==frame);
 return tsfp_guest_callback_cdecl(&callbacks,pc,args,4,result);
}
static int checkpoint_pc(void *opaque,uint32_t pc){
 (void)opaque;return pc==0x2000 || tsfp_context_generated_admit(pc);
}
static int checkpoint_state(void *opaque,const tsfp_x86_context_state *s){
 (void)opaque;return s->r_esp>=0x300 && s->r_esp+4<=sizeof storage.bytes;
}
static int checkpoint_stack(void *opaque,uintptr_t tag){(void)opaque;(void)tag;return 1;}
static void checkpoint_apply(void *opaque,const tsfp_x86_context_state *s){
 (void)opaque;g_eax=s->r_eax;g_ebx=s->r_ebx;g_ecx=s->r_ecx;g_edx=s->r_edx;
 g_esi=s->r_esi;g_edi=s->r_edi;g_ebp=s->r_ebp;g_esp=s->r_esp;
}
static int checkpoint_step(void *opaque,uint32_t pc,uint32_t *next){
 (void)opaque;
 if(pc==0x2000){
  tsfp_exception_ops ops={NULL,exception_memory,exception_admit,exception_call};
  tsfp_exception_request request={0};tsfp_x86_context_state s={0},resume;
  CHECK(MEM32(g_esp+4)==0x200);
  s.r_eax=g_eax;s.r_ebx=g_ebx;s.r_ecx=g_ecx;s.r_edx=g_edx;
  s.r_esi=g_esi;s.r_edi=g_edi;s.r_ebp=g_ebp;s.r_esp=g_esp+8;s.r_eip=MEM32(g_esp);s.r_eflags=2;
  CHECK(tsfp_x86_context_encode(storage.bytes+0x800,716,&s,0x10007));
  request.head=0x100;request.record=0x200;request.context=0x800;
  request.guard=0x380;request.guard_handler=0x6000;request.dispatcher=0x350;
  request.stack_low=0x300;request.stack_high=sizeof storage.bytes;
  request.baseline=&s;
  CHECK(tsfp_exception_raise(&ops,&request,&resume)==TSFP_EXCEPTION_CONTINUED);
  CHECK(resume.r_eip==0x1014 && resume.r_eax==42 && resume.r_esp==1000);
  tsfp_context_transfer(&resume);CHECK(0);
 }
 return tsfp_context_generated_step(pc,next);
}
'''
    check(translated(tmp_path,{0x1000:'b8070000006800020000e8f10f0000b801000000c3'},'''
tsfp_context_execution_ops ops={0};tsfp_x86_context_state initial={0};
MEM32(0x100)=0x1800;MEM32(0x1800)=0xffffffff;MEM32(0x1804)=0x3000;
memset(storage.bytes+0x200,0,80);MEM32(0x200)=0xe0424771;
MEM32(1000)=0;initial.r_eip=0x1000;initial.r_esp=1000;
ops.allow_pc=checkpoint_pc;ops.allow_state=checkpoint_state;ops.apply=checkpoint_apply;
ops.step=checkpoint_step;ops.same_stack=checkpoint_stack;
CHECK(tsfp_context_execute(&ops,&initial,0x300,sizeof storage.bytes,0)==TSFP_CONTEXT_EXECUTE_COMPLETE);
CHECK(g_eax==42 && g_esp==1004 && MEM32(0x100)==0x1800);
CHECK(!tsfp_context_execution_depth());
''',support=support,refs=refs,dispatch=True,run=True,
        extra=[tmp_path/name for name in ('guest_exception.c','guest_x86_context.c','guest_callback_call.c')]))
