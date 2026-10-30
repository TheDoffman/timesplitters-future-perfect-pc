"""Original function assembly: joins, stack captures and executed entry hooks."""
import ast
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_function as own
from translation_decode import Disassembler,Operand
from test_translation_product import _compile
from test_translation_lifting import installed


def decode(raw):
    data=bytes.fromhex(raw)
    return Disassembler().disassemble_function(data,0x1000,0x1000+len(data))


def translator(raw,**options):
    data=bytes.fromhex(raw);info=dict(end=0x1000+len(data),name='guest_body')
    t=own.FunctionTranslator(b'',{0x1000:info},**options)
    t._read_func_bytes=lambda start,end:data
    return t,info


def run(tmp_path,source):
    path=tmp_path/'function.c';path.write_text(source,encoding='utf-8')
    result=subprocess.run([str(_compile(tmp_path,path))],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr


def test_complete_installed_module_is_original_and_monkeypatches_reach_consumers():
    installed('''
from tools.recomp import translator as installed
import translation_function as own
assert installed is own
marker=object();saved=own._merge_flag_states
try:
 installed._merge_flag_states=lambda states:marker
 assert own._incoming_flag_state({1},{1:('test',[])},False) is marker
finally:own._merge_flag_states=saved
assert installed.FunctionTranslator.__module__=='translation_function'
''')
    tree=ast.parse((ROOT/'external/ts3FPrecomp/tools/recomp/translator.py').read_text(encoding='utf-8'))
    assert not any(isinstance(n,(ast.ClassDef,ast.FunctionDef)) for n in ast.walk(tree))


def test_flag_joins_require_known_widths_and_typed_zero_snapshots():
    reg=lambda name:Operand('reg',reg=name)
    left=('cmp',[reg('eax'),reg('ebx')]);right=('cmp',[reg('edx'),reg('ecx')])
    assert own._merge_flag_states([left,right])==left
    assert own._merge_flag_states([left,('cmp',[reg('al'),reg('bl')])]) is None
    assert own._merge_flag_states([left,None]) is None
    assert own._merge_flag_states([]) is None
    assert own._merge_flag_states([('sub',[reg('eax'),reg('ebx')]),('dec',[reg('eax')])])==('__zf_from_dest',[reg('eax')])
    assert own._merge_zero_flag([('dec',[reg('al')]),('dec',[reg('eax')])]) is None
    assert own._merge_zero_flag([('dec',[Operand('mem',mem_base='eax')])]) is None
    assert own._incoming_flag_state({1,2},{1:left,2:('test',right[1])},False)==('__zf_snapshot',[])
    assert own._incoming_flag_state({1,2},{1:left},False) is None
    assert own._incoming_flag_state({1},{1:left},True) is None
    scalar=('comiss',[reg('xmm0'),reg('xmm1')])
    assert own._merge_flag_states([scalar,('ucomisd',[reg('xmm2'),reg('xmm3')])])==scalar
    assert own._merge_flag_states([scalar,left]) is None


def test_register_inventory_and_carry_producer_consumers():
    t,_=translator('c3')
    assert t._find_used_registers(decode('668b04b789e5b401'))=={'eax','esi','edi','ebp','esp'}
    assert t._find_used_xmm(decode('0f28c10f6fc2'))=={'xmm0','xmm1','mm0','mm2'}
    for raw in ('9f','11d8','01d80f92c1','f3a67200','0fbaf001','f7e3'):
        assert t._function_needs_cf(decode(raw)),raw
    for raw in ('89d8','39d80f92c1','c1e00089d8'):
        assert not t._function_needs_cf(decode(raw)),raw


def test_block_probe_does_not_inflate_unsupported_diagnostics():
    t,info=translator('85c074040f0bebf8c3')
    generated=t.translate_function(0x1000,info)
    assert generated and t.lifter.unimplemented=={'ud2':[0x1004]}
    assert 'loc_00001008: ;' in generated


def test_compiled_indirect_failure_capture_preserves_frames_and_caller_cleanup(tmp_path):
    cases={
        'framed':['PUSH32(esp, edi);','edi = 0x222;','PUSH32(esp, edi);',
                  'PUSH32(esp, 0x1010); RECOMP_ICALL_SAFE(0, _icall_esp);','POP32(esp, edi);'],
        'caller_pop':['PUSH32(esp, 7);','PUSH32(esp, 9);',
                 'PUSH32(esp, 0x1020); RECOMP_ICALL_SAFE_AT(0, _icall_esp, 0x1000);',
                 'loc_00001020: ;','esp = esp + 8;'],
        'after_direct':['PUSH32(esp, 0x1030); completed(); /* call 0x00002000 */',
                        'PUSH32(esp, 7);','PUSH32(esp, 0x1040); RECOMP_ICALL_SAFE(0, _icall_esp);']}
    source='''#include <stdint.h>
static uint32_t memory[128],esp,edi,success,pop_bytes,direct_calls;
#define g_esp esp
#define PUSH32(s,v) ((s)-=4,memory[(s)/4]=(v))
#define POP32(s,v) ((v)=memory[(s)/4],(s)+=4)
#define RECOMP_ICALL_SAFE(target,saved) (success ? (esp+=pop_bytes) : (esp=(saved)))
#define RECOMP_ICALL_SAFE_AT(target,saved,site) RECOMP_ICALL_SAFE(target,saved)
static void completed(void){esp+=4;++direct_calls;}
'''
    for name,lines in cases.items():source+='static void '+name+'(void){\n'+'\n'.join(own._fixup_icall_esp_save(lines))+'\n}\n'
    source+='''int main(void){for(success=0;success<2;++success){
esp=128;edi=0x1234;pop_bytes=8;framed();if(esp!=128||edi!=0x1234)return 1;
esp=128;pop_bytes=4;caller_pop();if(esp!=128)return 2;
esp=128;pop_bytes=8;direct_calls=0;after_direct();if(esp!=128||direct_calls!=1)return 3;
}return 0;}
'''
    run(tmp_path,source)


def test_entry_trace_fast_return_register_hook_and_forced_exit_order(tmp_path):
    generated=[]
    for name,options in [('regular',dict(trace_all_entries=True)),('traced',dict(trace_functions={0x1000})),
                         ('forced',dict(trace_all_entries=True,force_returns={0x1000:42}))]:
        t,info=translator('b803000000c3',**options);info['name']=name
        generated.append(t.translate_function(0x1000,info))
    (tmp_path/'recomp_types.h').write_text('#undef RECOMP_REGISTER_SITE\n#define RECOMP_DECLARE_GPRS(site) record(3)\n',encoding='utf-8')
    source='''#include <stdint.h>
static uint32_t eax,esp;static int g_force_return,fast,events[8],count;
static void record(int n){events[count++]=n;}
#define RECOMP_GPR_REFS_SUPPORTED 1
#define RECOMP_TRACE_ENTER(name,site) record(1)
#define RECOMP_TRACE_EXIT(name,site) record(4)
#define RECOMP_FAST_ENTRY(site) (record(2),fast)
'''+ '\n'.join(generated)+'''
int main(void){esp=100;regular();if(eax!=3||esp!=104||count!=3||events[0]!=1||events[1]!=2||events[2]!=3)return 1;
count=0;fast=1;eax=9;esp=100;regular();if(eax!=9||esp!=100||count!=2||events[0]!=1||events[1]!=2)return 2;
count=0;esp=100;traced();if(eax!=3||esp!=104||count!=3||events[0]!=1||events[1]!=3||events[2]!=4)return 3;
count=0;esp=100;g_force_return=1;forced();return eax!=42||esp!=104||count!=2||events[0]!=1||events[1]!=3;}
'''
    run(tmp_path,source)


def test_x87_accessors_snapshot_input_and_share_circular_state(tmp_path):
    source='#include <stdint.h>\n#include <string.h>\nstatic double g_fp_stack[8];static uint32_t g_fp_top;\n'+'\n'.join(own.FP_STACK_MACROS)+'''
int main(void){const unsigned expected[]={7,0,1,2,3,4,5,6};
for(unsigned top=0;top<8;++top){g_fp_top=top;g_fp_stack[top]=9.25;fp_push(fp_top());
if(g_fp_top!=expected[top]||fp_top()!=9.25||fp_st1()!=9.25)return 1;
unsigned calls=0;fp_push(++calls);if(calls!=1||fp_top()!=1||fp_st1()!=9.25)return 2;
fp_pop();fp_pop();if(g_fp_top!=top)return 3;}
double minus_zero=-0.0;uint64_t bits;fp_push(minus_zero);memcpy(&bits,&g_fp_stack[g_fp_top],8);
return bits!=UINT64_C(0x8000000000000000);}
'''
    run(tmp_path,source)


def test_metadata_comments_are_escaped_and_empty_input_is_rejected():
    t,info=translator('c3',classification_db={0x1000:dict(category='bad */\nint injected;',source_file='*/')})
    code=t.translate_function(0x1000,info)
    assert 'bad * / int injected;' in code and '\nint injected;' not in code
    assert t.translate_function(0x1000,dict(end=0x1000)) is None
    t._read_func_bytes=lambda start,end:None
    assert t.translate_function(0x1000,info) is None


def test_standalone_translation_without_toolkit(tmp_path):
    sources=list((ROOT/'tools/compiler').glob('translation_*'))
    sources += [ROOT/'tools/compiler'/name for name in ('c_source_scan.py','x86_decode.py','xbe_image.py','generation_output.py')]
    for source in sources:
        if source.is_file():shutil.copyfile(source,tmp_path/source.name)
    script='''import sys
sys.path.insert(0,sys.argv[1])
import translation_function as own
import translation_layout as layout
data=bytes.fromhex('b803000000c3');info={'end':0x1006,'name':'standalone'}
layout._install([layout.Section('.text',0x1000,6,0,6,True)],0x1000,0,'fixture')
output=own.FunctionTranslator(data,{0x1000:info}).translate_function(0x1000,info)
assert ('eax = 0x3;' in output or 'eax = 3;' in output) and 'esp += 4;' in output
assert not any(n.startswith('tools.recomp') for n in sys.modules)
'''
    result=subprocess.run([sys.executable,'-I','-c',script,str(tmp_path)],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stderr
