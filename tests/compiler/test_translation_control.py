"""Original control-transfer tests: guest stack, frames, dispatch and bounded tables."""
import json,os,shutil,struct,subprocess,sys
from pathlib import Path
from types import SimpleNamespace as S
import pytest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_control as own
import translation_layout as layout
from translation_decode import Instruction,Operand
from test_translation_product import _compile


def context(**changes):
    values=dict(func_start=0x1000,func_end=0x1100,func_db={},manual_functions=set(),
        xbe_data=b'',_pop_cache={},publishes_ebp=False,trace_exit_name=None,
        force_return_value=None,SEH_PROLOGS=set(),SEH_EPILOGS=set(),SETJMP_FN=None,
        LONGJMP_FN=None,icall_site_targets={},jump_table_targets={},imm_code_refs=set(),needs_cf=False)
    values.update(changes);c=S(**values)
    c._call_target_name=lambda address:f'target_{address:X}'
    c.callee_pop=lambda address:own.callee_cleanup(c,address)
    c._read_jump_table=lambda address:own.read_table(c,address)
    c._analyze_switch_table=lambda operands:own.switch_targets(c,operands)
    return c


def test_installed_control_handlers_use_project_implementation():
    script = r'''
import tools.recomp.lifter as module
import translation_control as own
bindings={'callee_pop':'callee_cleanup','_lift_call':'call','_lift_ret':'return_from',
 '_forced_tail':'forced_tail','_is_external_target':'external','_read_jump_table':'read_table',
 '_analyze_switch_table':'switch_targets','_lift_jmp':'jump','_lift_jcc':'branch','_lift_loop':'loop'}
for name,handler in bindings.items():assert getattr(module.Lifter,name) is getattr(own,handler),name
assert module._emit_cond_goto is own.conditional
'''
    run=subprocess.run([sys.executable,'-c',script],cwd=ROOT/'external/ts3FPrecomp',
        env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr


def instruction(name,target=None,raw='',address=0x1000):
    return Instruction(address,len(bytes.fromhex(raw)) or 5,name,'',raw,call_target=target if name=='call' else None,jump_target=target if name!='call' else None)


def function(name,lines):return f'static void {name}(void){{\n'+'\n'.join(lines)+'\n}\n'


def execute(tmp_path,source):
    path=tmp_path/'control.c';path.write_text(source,encoding='utf-8')
    run=subprocess.run([str(_compile(tmp_path,path))],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr+f' exit={run.returncode}'


@pytest.fixture(autouse=True)
def layout_snapshot():
    before=dict(vars(layout));yield;vars(layout).update(before)


def test_table_bounds_direction_and_local_targets():
    layout._install([layout.Section('.text',0x1000,256,0,256,True)],0x1000,0,'fixture')
    data=bytearray(256)
    def put(offset,values):data[offset:offset+4*len(values)]=b''.join(struct.pack('<I',v) for v in values)
    put(64,[0x1080,0x1090,0x1080,0xFFFF0000]);c=context(xbe_data=data)
    assert own.read_table(c,0x1040)==[0x1080,0x1090,0x1080]
    assert own.read_table(c,0x1040,2)==[0x1080,0x1090]
    assert own.read_table(c,0x1040,0)==[]
    assert own.read_table(c,0x1100)==[]
    assert own.read_table(c,0x10fe)==[]
    for skip in (1,2,3):
        data[:]=bytes(256);put(64+4*skip,[0x1080,0x1090]);assert own.read_table(c,0x1040)==[0x1080,0x1090]
    # A backwards table may contain its own end as a legitimate destination.
    data[:]=bytes(256);put(64,[0x104c,0x1080,0x1090]);assert own.read_table(c,0x104c)==[0x104c,0x1080,0x1090]
    # The displacement of an immediately preceding indexed jump is not a slot.
    for index in range(8):
        data[:]=bytes(256);data[57:60]=bytes((0xff,0x24,0x85|(index<<3)));put(60,[0x1040]);put(64,[0x1080]);assert own.read_table(c,0x1040)==[0x1080]
    c.jump_table_targets={0x1040:[0x1000,0x10ff,0x1100,0x1001]}
    op=Operand('mem',mem_index='eax',mem_scale=4,mem_disp=0x1040,mem_size=4)
    assert own.switch_targets(c,[op])==[0x1000,0x10ff]
    c.jump_table_targets[0x1040]=[0x1100,0x1000,0x1001];assert own.switch_targets(c,[op])==[]
    assert own.switch_targets(c,[Operand('reg',reg='eax')])==[]


def test_cleanup_consensus_and_standalone_import(tmp_path):
    layout._install([layout.Section('.text',0x1000,64,0,64,True)],0x1000,0,'fixture')
    for body,end,want in [(bytes.fromhex('c20800c20800'),0x1006,8),(bytes.fromhex('c20400c20800'),'1006',None),(bytes.fromhex('b8c2080000c3'),0x1006,0),(bytes.fromhex('c3'),0x1002,None),(b'\xc3','wrong',None)]:
        c=context(xbe_data=body,func_db={0x1000:{'end':end}})
        assert own.callee_cleanup(c,0x1000)==want
        c.xbe_data=b'\xc2\x04\x00';assert own.callee_cleanup(c,0x1000)==want
    for name in ('translation_control.py','translation_moves.py','translation_operands.py','translation_conditions.py','translation_decode.py','translation_hooks.py','translation_layout.py','c_source_scan.py','x86_decode.py','xbe_image.py'):
        shutil.copyfile(ROOT/'tools/compiler'/name,tmp_path/name)
    result=subprocess.run([sys.executable,'-I','-c',"import sys;sys.path.insert(0,sys.argv[1]);import translation_control as c;assert callable(c.call)",str(tmp_path)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr


PRELUDE=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
static uint32_t eax,ebx,ecx,edx,esi,edi,esp,ebp,g_ebp,g_seh_ebp,_icall_esp;
static int _flags,_cf,g_force_return;static unsigned reads,calls,hits,misses,traces,exits,observed_site;
static uint32_t seen_target,seen_ret,seen_frame,cleanup;static unsigned char memory[1024];
#define CHECK(x) do{if(!(x)){fprintf(stderr,"check line %d\n",__LINE__);return 1;}}while(0)
static uint32_t read32(uint32_t p){uint32_t v;if(p>1020)abort();memcpy(&v,memory+p,4);++reads;return v;}
static void write32(uint32_t p,uint32_t v){if(p>1020)abort();memcpy(memory+p,&v,4);}
#define MEM32(p) read32(p)
#define PUSH32(s,v) ((s)-=4,write32((s),(v)))
#define LO16(r) ((uint16_t)(r))
#define SET_LO16(r,v) ((r)=((r)&0xffff0000u)|(uint16_t)(v))
static void invoke(uint32_t target){++calls;seen_target=target;seen_ret=MEM32(esp);seen_frame=g_seh_ebp;esp+=4+cleanup;eax=target+1;g_seh_ebp=0x55667788;}
static void target_2000(void){invoke(0x2000);}
static void target_3000(void){invoke(0x3000);}
static void target_0(void){invoke(0);}
#define RECOMP_ABI_CALL(a,f) (f)()
#define RECOMP_ABI_CALL_POP(a,f,n) do{cleanup=(n);(f)();}while(0)
#define RECOMP_DIRECT_CALLEE_AT(s,f) (observed_site=(s),(f))
#define RECOMP_ICALL_SAFE(a,s) invoke(a)
#define RECOMP_ICALL_SAFE_AT(a,s,site) (observed_site=(site),invoke(a))
#define RECOMP_ICALL_GUARD_HIT() (++hits)
#define RECOMP_ICALL_GUARD_MISS() (++misses)
#define RECOMP_ITAIL(a) invoke(a)
#define RECOMP_TRACE_ESP(n,event) (++traces)
#define RECOMP_TRACE_EXIT(n,site) (++exits)
static void reset(void){eax=ebx=ecx=edx=esi=edi=0;esp=512;ebp=0xaabbccdd;g_ebp=g_seh_ebp=0;reads=calls=hits=misses=traces=exits=observed_site=0;cleanup=0;g_force_return=0;memset(memory,0,sizeof memory);}
'''


def test_call_stack_targets_frames_and_guard_fallback_execute(tmp_path):
    code=PRELUDE;c=context()
    code+=function('plain',own.call(c,instruction('call',0x2000),[]))
    code+=function('zero_target',own.call(c,instruction('call',0),[]))
    c=context(_pop_cache={0x2000:12},publishes_ebp=True,trace_exit_name='frame');code+=function('known',own.call(c,instruction('call',0x2000),[]))
    c=context(SEH_PROLOGS={0x3000});code+=function('seh',own.call(c,instruction('call',0x3000),[]))
    c=context(manual_functions={0x2000});code+=function('manual',own.call(c,instruction('call',0x2000),[]))
    op=Operand('mem',mem_base='esp',mem_disp=8,mem_size=4)
    c=context();code+=function('indirect',own.call(c,instruction('call'),[op]))
    c=context(func_db={0x2000:{},0x3000:{}},icall_site_targets={0x1000:[0x3000,0x2000,0x2000]});code+=function('guarded',own.call(c,instruction('call'),[op]))
    c.manual_functions={0x2000};code+=function('manual_observed',own.call(c,instruction('call'),[op]))
    code+=r'''
int main(void){reset();plain();CHECK(esp==512&&calls==1&&seen_target==0x2000&&seen_ret==0x1005&&observed_site==0x1000);
reset();zero_target();CHECK(calls==1&&seen_target==0&&seen_ret==0x1005);
reset();known();CHECK(esp==524&&g_ebp==ebp&&seen_frame==ebp&&traces==1);
reset();seh();CHECK(seen_frame==0xaabbccdd&&ebp==0x55667788&&esp==512);
reset();manual();CHECK(calls==1&&seen_target==0x2000&&seen_ret==0x1005);
reset();write32(520,0x3000);write32(516,0xbad);indirect();CHECK(reads==2&&seen_target==0x3000&&seen_ret==0x1005&&esp==512);
uint32_t targets[]={0x2000,0x3000,0x4000};for(unsigned i=0;i<3;++i){reset();write32(520,targets[i]);guarded();CHECK(calls==1&&seen_target==targets[i]&&seen_ret==0x1005&&esp==512&&hits==(i<2)&&misses==(i==2));}
reset();write32(520,0x2000);manual_observed();CHECK(calls==1&&seen_target==0x2000&&hits==0&&misses==0);
puts("PASS direct/manual/guarded calls, pre-push target capture, frame bridge and return VA");return 0;}
'''
    execute(tmp_path,code)


def test_existing_indirect_stack_recovery_postpass_execute(tmp_path):
    """A missing target must not leak or double-clean call arguments."""
    rows = {}
    op = Operand('mem', mem_base='esp', mem_disp=8, mem_size=4)
    for guarded in (False, True):
        for caller_cleans in (False, True):
            c = context(func_db={0x2000:{}},
                        icall_site_targets={0x1000:[0x2000]} if guarded else {})
            lines = ['PUSH32(esp, ebx);', 'PUSH32(esp, eax);']
            lines += own.call(c, instruction('call'), [op])
            if caller_cleans:
                lines += ['esp = esp + 4;']
            lines += ['POP32(esp, ebx);']
            rows[f'recover_{int(guarded)}_{int(caller_cleans)}'] = lines
    script = ('import json,sys; from tools.recomp.translator import _fixup_icall_esp_save; '
              'print(json.dumps({k:_fixup_icall_esp_save(v) for k,v in json.loads(sys.argv[1]).items()}))')
    run = subprocess.run([sys.executable, '-c', script, json.dumps(rows)],
                         cwd=ROOT/'external/ts3FPrecomp',
                         env=dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT)),
                         capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, run.stdout + run.stderr
    code = PRELUDE + r'''
#define g_esp esp
#define POP32(s,r) ((r)=read32(s),(s)+=4)
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(a,s,site) do{seen_target=(a);esp=(s);++calls;}while(0)
'''
    for name, lines in json.loads(run.stdout).items():
        code += function(name, lines)
    code += r'''
int main(void){void(*cases[])(void)={recover_0_0,recover_0_1,recover_1_0,recover_1_1};
for(unsigned i=0;i<4;++i){reset();ebx=0xface;eax=0x1111;write32(512,0xdead);write32(508,0xbad);cases[i]();CHECK(esp==512&&ebx==0xface&&seen_target==0xdead&&calls==1&&misses==(i>=2));}
puts("PASS production postpass restores stdcall/cdecl failures without consuming frame save");return 0;}
'''
    execute(tmp_path, code)


def test_returns_tails_conditional_manual_and_loop_widths_execute(tmp_path):
    code=PRELUDE
    c=context(SEH_EPILOGS={0x1000},trace_exit_name='exit',force_return_value=7)
    for n in (0,16,65535):code+=function(f'ret_{n}',own.return_from(c,instruction('ret'),[] if n==0 else [Operand('imm',imm=n)]))
    for manual in (False,True):
        c=context(manual_functions={0x2000} if manual else set(),trace_exit_name='tail',force_return_value=7)
        code+=function('tail_'+str(int(manual)),own.jump(c,instruction('jmp',0x2000),[]))
        code+=function('conditional_'+str(int(manual)),[own.conditional('_flags','je','equal',0x2000,c),'eax=99;'])
    c=context(imm_code_refs={0x1001,0x1002,0x3000});code+=function('local_indirect',own.jump(c,instruction('jmp'),[Operand('reg',reg='ecx')])+['eax=99;return;','loc_00001001: eax=11;return;','loc_00001002: eax=12;return;'])
    for bits in (16,32):
        for op,encoding in [('loop','e2fe'),('loope','e1fe'),('loopne','e0fe')]:
            i=instruction(op,0x1001,('67' if bits==16 else '')+encoding);lines=own.loop(context(),i,'_flags' if op!='loopne' else '!_flags')
            code+=function(f'{op}_{bits}',lines+['eax=0;return;','loc_00001001: eax=1;return;'])
    code+=r'''
int main(void){reset();g_force_return=1;ret_0();CHECK(eax==7&&esp==516&&g_seh_ebp==ebp&&exits==1);
reset();ret_16();CHECK(eax==0&&esp==532);reset();ret_65535();CHECK(esp==66051);
void(*tails[])(void)={tail_0,tail_1};void(*branches[])(void)={conditional_0,conditional_1};
for(unsigned i=0;i<2;++i){reset();g_force_return=1;tails[i]();CHECK(eax==7&&calls==1&&traces==1&&seen_frame==ebp);
reset();_flags=0;branches[i]();CHECK(calls==0&&eax==99);reset();_flags=1;g_force_return=1;branches[i]();CHECK(calls==1&&eax==7&&seen_frame==ebp);}
for(unsigned i=0;i<3;++i){reset();ecx=i==0?0x1001:i==1?0x1002:0x4000;local_indirect();CHECK(i<2?(eax==11+i&&calls==0):(seen_target==0x4000&&calls==1));}
uint32_t values[]={0,1,2,65535,65536,65537,0xffffffff};void(*loops[2][3])(void)={{loop_16,loope_16,loopne_16},{loop_32,loope_32,loopne_32}};
for(unsigned width=0;width<2;++width)for(unsigned n=0;n<7;++n)for(unsigned z=0;z<2;++z)for(unsigned op=0;op<3;++op){reset();ecx=values[n];_flags=(int)z;uint32_t want=width?ecx-1:(ecx&0xffff0000u)|((ecx-1)&0xffffu),count=width?want:want&0xffffu;loops[width][op]();CHECK(ecx==want&&eax==(unsigned)(count!=0&&(op==0||(op==1?z:!z)))&&_flags==(int)z);}
puts("PASS returns, forced/traced/manual tails, local dispatch and CX/ECX loops");return 0;}
'''
    execute(tmp_path,code)


def test_native_nonlocal_jump_resumes_the_calling_frame(tmp_path):
    code=PRELUDE+r'''
static jmp_buf slot;static int armed,escaped;static uint32_t saved_stack;
static jmp_buf* recomp_setjmp_slot(uint32_t guest){if(guest!=0x80)abort();armed=1;saved_stack=esp;return &slot;}
static int recomp_guest_longjmp(uint32_t guest,uint32_t value){if(guest!=0x80||!armed)return 0;esp=saved_stack;g_seh_ebp=0x1234;longjmp(slot,value?value:1);}
static void target_4000(void){esp+=4;eax=0;}
static void target_5000(void){++escaped;esp+=4;}
'''
    c=context(SETJMP_FN=0x4000,LONGJMP_FN=0x5000)
    code+=function('nonlocal_jump',own.call(c,instruction('call',0x5000),[])+['++escaped;'])
    code+=function('resume_frame',own.call(c,instruction('call',0x4000),[])+['if (!eax) { write32(esp,0x80); write32(esp+4,9); nonlocal_jump(); ++escaped; }'])
    code+=r'''
int main(void){reset();write32(esp,0x80);resume_frame();CHECK(eax==9&&ebp==0x1234&&esp==512&&escaped==0);
reset();armed=0;write32(esp,0x99);nonlocal_jump();CHECK(escaped==2&&esp==512);
puts("PASS native setjmp/longjmp unwinds intervening frames with guest fallback");return 0;}
'''
    execute(tmp_path,code)


def test_backward_table_with_fewer_than_four_bytes_after_displacement():
    # The table ends at a three-byte RET arm cluster. No forward word exists.
    base=0x1000;end=base+19
    data=bytes((0xff,0x24,0x8d))+struct.pack('<I',end)
    data+=b''.join(struct.pack('<I',end+i) for i in range(3))+b'\xc3'*3
    layout._install([layout.Section('.text',base,len(data),0,len(data),True)],base,0,'short-table-tail')
    assert own.read_table(context(xbe_data=data),end)==[end,end+1,end+2]


def test_decoded_control_transfers_through_production_translator(tmp_path):
    # Install this emitter only within a subprocess; exercise the real decoder,
    # block lifting and stack-recovery postpass against compiled guest programs.
    script = r'''
import json
import tools.recomp.lifter as module
import translation_control as own
bindings={'callee_pop':'callee_cleanup','_lift_call':'call','_lift_ret':'return_from',
 '_forced_tail':'forced_tail','_is_external_target':'external','_read_jump_table':'read_table',
 '_analyze_switch_table':'switch_targets','_lift_jmp':'jump','_lift_jcc':'branch','_lift_loop':'loop'}
for name,handler in bindings.items():setattr(module.Lifter,name,getattr(own,handler))
module._emit_cond_goto=own.conditional
from tools.recomp.translator import FunctionTranslator
result={}
cases={'direct':'e8fb0f0000c3','manual':'e8fb0f0000c3','indirect':'ff542408c3',
 'guarded':'ff542408c3','tail':'e9fb0f0000','conditional':'39d80f84f80f0000c3',
 'cx_loop':'b90200010067e2fdc3','ecx_loop':'b902000000e2fec3',
 'mixed_join':'e30439d8eb05e8f50f00007406ba00000000c3ba01000000c3'}
for name,raw in cases.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data),'end':0x1000+len(data)}
 db={0x1000:info,0x2000:{'name':'target_2000','size':1},0x3000:{'name':'target_3000','size':1}}
 tr=FunctionTranslator(b'',db,icall_sites={0x1000:[0x2000,0x3000]} if name=='guarded' else {})
 tr._read_func_bytes=lambda address,end:data if address==0x1000 else b'\xc3'
 if name in ('manual','conditional'):tr.lifter.manual_functions={0x2000}
 result[name]=tr.translate_function(0x1000,info)
print(json.dumps(result))
'''
    run=subprocess.run([sys.executable,'-c',script],cwd=ROOT/'external/ts3FPrecomp',
        env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    code=PRELUDE+'\n#define g_esp esp\n#define RECOMP_FAST_ENTRY(site) 0\n'
    decoded=json.loads(run.stdout)
    assert 'outgoing ZF snapshot' in decoded['mixed_join']
    code+='\n'.join(decoded.values())
    code+=r'''
int main(void){reset();direct();CHECK(calls==1&&seen_target==0x2000&&seen_ret==0x1005&&esp==516);
reset();manual();CHECK(calls==1&&seen_target==0x2000&&seen_ret==0x1005&&esp==516);
reset();write32(520,0x3000);indirect();CHECK(calls==1&&seen_target==0x3000&&seen_ret==0x1004&&esp==516);
for(unsigned i=0;i<3;++i){reset();write32(520,0x2000+i*0x1000);guarded();CHECK(calls==1&&seen_target==0x2000+i*0x1000&&seen_ret==0x1004&&esp==516&&hits==(i<2)&&misses==(i==2));}
reset();tail();CHECK(calls==1&&seen_target==0x2000&&esp==516);
for(unsigned i=0;i<2;++i){reset();eax=7;ebx=7+i;conditional();CHECK(calls==!i&&esp==516);}
reset();cx_loop();CHECK(ecx==0x10000&&esp==516);reset();ecx_loop();CHECK(ecx==0&&esp==516);
for(unsigned i=0;i<2;++i){reset();ecx=1;eax=3;ebx=3+i;mixed_join();CHECK(edx==!i&&esp==516&&calls==0);}
puts("PASS decoded call/manual/guarded/tail transfers and CX/ECX loops");return 0;}
'''
    execute(tmp_path,code)
    # The generation comparison exposed a dropped predecessor snapshot. Prove
    # the executable fixture rejects that exact omission, not just its spelling.
    import re
    broken,count=re.subn(r'^\s*_flags = [^\n]*outgoing ZF snapshot[^\n]*\n',
                         '',code,flags=re.M)
    assert count==1
    source=tmp_path/'missing-join-snapshot.c'
    source.write_text(broken,encoding='utf-8')
    failure=subprocess.run([str(_compile(tmp_path,source))],capture_output=True,text=True,timeout=30)
    assert failure.returncode==1 and 'check line' in failure.stderr
