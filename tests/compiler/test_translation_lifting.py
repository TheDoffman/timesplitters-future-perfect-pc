"""Decoded dispatch, live-condition propagation and executed guest sequences."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from types import SimpleNamespace as S

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_lifting as own
from translation_decode import Disassembler, Instruction, Operand
from test_translation_control import PRELUDE
from test_translation_moves import _production_macros
from test_translation_product import _compile


def installed(script):
    result=subprocess.run([sys.executable,'-c',script],cwd=ROOT/'external/ts3FPrecomp',
        env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    return result.stdout


def decode(raw):
    data=bytes.fromhex(raw)
    return Disassembler().disassemble_function(data,0x1000,0x1000+len(data))


def test_installed_bindings_and_toolkit_free_import(tmp_path):
    installed('''
from tools.recomp import lifter as l
import translation_lifting as p
for name,handler in {'lift_instruction':'instruction','_result_snapshot':'result_snapshot',
 '_unimplemented':'unsupported','_lift_setcc':'fallback_set','_lift_cmovcc':'fallback_move'}.items():
 assert getattr(l.Lifter,name) is getattr(p,handler),name
for name,handler in {'lift_basic_block':'block','try_match_cmp_jcc':'adjacent_condition',
 '_has_xmm_operand':'has_xmm','_is_rep_compare':'repeated_compare'}.items():
 assert getattr(l,name) is getattr(p,handler),name
''')
    # Copy the complete project translation family and direct supporting tools,
    # then import without the checkout or a toolkit in sys.path.
    files=list((ROOT/'tools/compiler').glob('translation_*.py'))
    files += [ROOT/'tools/compiler'/n for n in ('c_source_scan.py','x86_decode.py','xbe_image.py')]
    for path in files: shutil.copyfile(path,tmp_path/path.name)
    result=subprocess.run([sys.executable,'-I','-c',
        'import sys;sys.path.insert(0,sys.argv[1]);import translation_lifting;'
        'assert not any(n.startswith("tools.recomp") for n in sys.modules)',str(tmp_path)],
        capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stderr


@pytest.mark.parametrize('raw,method,extra',[
    ('89d8','_lift_mov',()),('0fb6c3','_lift_movzx',()),('0fbec3','_lift_movsx',()),
    ('8d4304','_lift_lea',()),('87d8','_lift_xchg',()),('f08718','_lift_xchg',()),
    ('50','_lift_push',()),('58','_lift_pop',()),('01d8','_lift_alu_binop',('add',)),
    ('40','_lift_inc_dec',('inc',)),('f7d8','_lift_neg',()),('f7d0','_lift_not',()),
    ('0fafc3','_lift_imul',()),('f7e3','_lift_muldiv',('mul',)),('f7fb','_lift_muldiv',('idiv',)),
    ('19d8','_lift_sbb',()),('11d8','_lift_adc',()),('d1e0','_lift_shift',('<<',)),
    ('d1e8','_lift_shift',('>>',)),('d1f8','_lift_sar',()),('d1c0','_lift_rotate',('rol',)),
    ('d1d0','_lift_rotate_carry',('rcl',)),('39d8','_lift_cmp',()),('85c0','_lift_test',()),
    ('e800000000','_lift_call',()),('c3','_lift_ret',()),('eb00','_lift_jmp',()),
    ('a5','_lift_string_op',('movsd',)),('f3a6','_lift_rep_string',('repe cmpsb',)),
    ('f20f10c1','_lift_sse',('movsd','ops')),('f20fc2c100','_lift_sse',('cmpeqsd','ops')),
    ('0ffcc1','_lift_mmx',('paddb','ops')),('d9e8','_lift_fpu',('fld1','ops')),
    ('0fc3','invalid',()),('0fb1cb','_lift_exchange',()),
    ('f00fc118','_lift_exchange',()),('0fbdc3','_lift_scan',('bsr',)),
])
def test_decoded_operand_routing(raw,method,extra):
    if method=='invalid':
        assert not decode(raw);return
    insn,=decode(raw)
    class Recorder:
        def __getattr__(self,name): return lambda *args:(name,args)
    got,args=own.instruction(Recorder(),insn)
    assert got==method
    expected=tuple(insn.operands if arg=='ops' else arg for arg in extra)
    assert args==((insn,*expected) if method in ('_lift_string_op','_lift_rep_string','_lift_sse','_lift_mmx','_lift_fpu') else (insn,insn.operands,*expected))


def test_flag_state_boundaries_and_loop_aliases():
    state=('cmp',[Operand('reg',reg='eax'),Operand('reg',reg='ebx')])
    for raw in ('89d8','0f28c1','0ffcc1','0fd4c1','0fe4c1','d9e8','f3a4','d1e0'):
        insn,=decode(raw)
        got=own.successor_state(state,insn)
        assert got is not None
        if raw!='d1e0': assert got is state
    for raw in ('9d','0fa2','0f31','f7f3'):
        assert own.successor_state(state,decode(raw)[0]) is None
    for raw in ('f00fc118','f00fb118'):
        insn,=decode(raw);assert own.successor_state(state,insn)[0]==insn.mnemonic.removeprefix('lock ')
    for raw in ('c1e000','c1c020'):
        assert own.successor_state(state,decode(raw)[0]) is state
    assert own.successor_state(state,decode('a6')[0])[0]=='cmp'
    assert own.successor_state(state,decode('f3a6')[0])[0]=='repe cmpsb'
    assert own.successor_state(state,decode('0f2fc1')[0])[0]=='comiss'
    assert own.successor_state(state,decode('dff1')[0])[0]=='fcompi'
    assert own.successor_state(state,decode('9e')[0])[0]=='sahf'
    for name in ('loope','loopz','loopne','loopnz'):
        ctx=S(_lift_loop=lambda instruction,value:value)
        result=own._consumer(ctx,Instruction(0x1000,2,name,'',''),state)
        assert result==own.conditions.make_condition('je' if name in ('loope','loopz') else 'jne',*state)[0]
    assert own.adjacent_condition(decode('39d87400'),-1) is None
    assert own.adjacent_condition(decode('39d8e300'),0) is None


def test_compiled_decoded_block_flags_survive_overwrites_and_zero_count(tmp_path):
    cases={
        'captured':'39d8b82a0000000f94c2c3',
        'normalized':'85c0b82a0000000f94c2c3',
        'fused_reuse':'39d87405b901000000b82a0000000f94c2c3',
        'zero_rep':'31c0f3a60f94c2c3',
        'neg_carry':'f7d8535b19c0c3',
        'conditional_move':'39d8b82a0000000f44d3c3',
        'byte_snapshot':'38d8b82a0000000f94c2c3',
        'word_snapshot':'6639d8b82a0000000f94c2c3',
        'integer_lahf':'39d89fc3',
    }
    script='''
import json
from tools.recomp.translator import FunctionTranslator
result={}
for name,raw in CASES.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data),'end':0x1000+len(data)}
 tr=FunctionTranslator(b'',{0x1000:info});tr._read_func_bytes=lambda address,end:data
 result[name]=tr.translate_function(0x1000,info)
print(json.dumps(result))
'''.replace('CASES',repr(cases))
    decoded=json.loads(installed(script))
    code=PRELUDE+'\n#define g_esp esp\n#define RECOMP_FAST_ENTRY(site) 0\nstatic int g_df;\n#define MEM8(p) memory[(p)]\n'
    code+=_production_macros({'LO8','HI8','SET_LO8','SET_HI8','POP32','RECOMP_DF_STEP'})+'\n'
    code+='\n'.join(decoded.values())
    code+=r'''
int main(void){for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){
reset();eax=a;ebx=b;captured();CHECK((edx&255)==(a==b));
reset();eax=a;normalized();CHECK((edx&255)==(a==0));
reset();eax=a;ebx=b;fused_reuse();CHECK((edx&255)==(a==b)&&ecx==(a!=b));
reset();eax=a;neg_carry();CHECK(eax==(a?0xffffffffu:0)&&esp==516);
reset();eax=a;ebx=b;edx=999;conditional_move();CHECK(edx==(a==b?b:999));
reset();eax=a|0xdead0000u;ebx=b|0xbeef0000u;byte_snapshot();CHECK((edx&255)==(a==b));
reset();eax=a|0xdead0000u;ebx=b|0xbeef0000u;word_snapshot();CHECK((edx&255)==(a==b));
reset();eax=a;ebx=b;integer_lahf();CHECK(((eax>>14)&1)==(a==b)&&((eax>>8)&1)==(a<b));
}reset();ecx=0;esi=111;edi=222;zero_rep();CHECK(edx==1&&esi==111&&edi==222);
puts("PASS captured comparison, normalized/fused flags, NEG carry, LAHF and zero-count REP");return 0;}
'''
    path=tmp_path/'lifting.c';path.write_text(code,encoding='utf-8')
    run=subprocess.run([str(_compile(tmp_path,path))],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    # Removing the incoming ZF preload must be detected by the same executable.
    broken,count=re.subn(r'^\s*_flags = [^\n]*ZF in:[^\n]*\n','',code,flags=re.M)
    assert count==1
    path.write_text(broken,encoding='utf-8')
    run=subprocess.run([str(_compile(tmp_path,path))],capture_output=True,text=True,timeout=60)
    assert run.returncode==1 and 'check line' in run.stderr


def test_unsupported_diagnostic_and_result_snapshots_compile(tmp_path):
    ctx=S(unimplemented={})
    insn=Instruction(0x1234,1,'bad','a"\\\n*/\u03bb','ff')
    diagnostic=own.unsupported(ctx,insn,'bad','*/')
    assert ctx.unimplemented=={'bad':[0x1234]}
    code='#include <stdint.h>\n#include <string.h>\nstatic unsigned seen;\n#define RECOMP_UNIMPL(t,a) (seen=(a))\n'
    code+=_production_macros({'LO8','LO16'})+'\n'
    code+='int main(void){uint32_t eax=0xffff0080,ebx=0x8001,_fa=0,_fb=0;int32_t _fas=0,_fbs=0;\n'
    code+='\n'.join(diagnostic)+'\n'
    code+=own.result_snapshot(ctx,[Operand('reg',reg='al')],'and')+'\n'
    code+='if(_fa!=128||_fas!=-128||seen!=0x1234)return 1;\n'
    code+=own.result_snapshot(ctx,[Operand('reg',reg='ax'),Operand('reg',reg='bx')],'add',True)+'\n'
    code+='return _fb!=0x8001||_fbs!=-32767;}\n'
    path=tmp_path/'diagnostic.c';path.write_text(code,encoding='utf-8')
    assert subprocess.run([str(_compile(tmp_path,path))],timeout=30).returncode==0
