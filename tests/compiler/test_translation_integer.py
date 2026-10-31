"""Original native result/carry/snapshot and production consumer contracts."""

from project_paths import source_file as repo_file

import os
import json
from pathlib import Path
import subprocess
import shutil
import sys
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_integer as integer
from translation_decode import Operand
from native_compiler import native_compiler
from test_translation_moves import _production_macros


def test_invalid_integer_operands_fail_before_emission():
    context=SimpleNamespace(needs_cf=True)
    instruction=SimpleNamespace(address=0x1000)
    bad=([], [Operand('imm',imm=1)], [Operand('reg',reg='fs')],
         [Operand('reg',reg='st(0)')], [Operand('reg',reg='xmm0')],
         [Operand('mem',mem_size=8)])
    for operands in bad:
        with pytest.raises(ValueError):integer.neg(context,instruction,operands)
    for operands in ([Operand('mem',mem_size=4)]*2,
                     [Operand('reg',reg='eax'),Operand('reg',reg='ax')],
                     [Operand('reg',reg='eax'),Operand('unknown')]):
        with pytest.raises(ValueError):integer.alu_binop(context,instruction,operands,'add')


def test_copied_integer_lowerer_runs_without_toolkit(tmp_path):
    for name in ('translation_integer.py','translation_operands.py'):
        shutil.copyfile(repo_file(ROOT, 'tools', name),tmp_path/name)
    code='''import sys
sys.path.insert(0,sys.argv[1])
from types import SimpleNamespace as S
import translation_integer as integer
result=integer.adc(S(needs_cf=True),S(address=0),[S(type='reg',reg='eax'),S(type='imm',imm=1)])
assert 'uint32_t _alu_carry = (_cf != 0);' in result
assert not any(name.startswith('tools.recomp') for name in sys.modules)
'''
    result=subprocess.run([sys.executable,'-I','-c',code,str(tmp_path)],cwd=tmp_path,
                          capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr


def test_integer_results_carries_and_snapshots_execute(tmp_path):
    operations=('add','sub','and','or','xor','adc','sbb','inc','dec','neg','not')
    context=SimpleNamespace(needs_cf=True)
    instruction=SimpleNamespace(address=0x1000)
    def statements(operation,bits,immediate=None):
        left={8:'al',16:'ax',32:'eax'}[bits]
        right={8:'bl',16:'bx',32:'ebx'}[bits]
        ops=[Operand('reg',reg=left)]
        if operation in operations[:7]:
            ops.append(Operand('imm',imm=immediate) if immediate is not None else Operand('reg',reg=right))
        if operation in operations[:5]:return integer.alu_binop(context,instruction,ops,operation)
        if operation in ('inc','dec'):return integer.inc_dec(context,instruction,ops,operation)
        return getattr(integer,'bit_not' if operation=='not' else operation)(context,instruction,ops)
    functions=[]
    def function(name,lines):
        return f'''static State {name}(uint32_t a,uint32_t b,uint32_t carry) {{
uint32_t eax=a,ebx=b,_fa=0x41414141u,_fb=0x42424242u; int32_t _fas=42,_fbs=43; int _cf=(int)carry;
{chr(10).join(lines)}
State result={{eax,_fa,_fb,_fas,_fbs,_cf}}; return result; }}
'''
    for bits in (8,16,32):
        for operation in operations:functions.append(function(f'{operation}{bits}',statements(operation,bits)))
    functions.append(function('immediate8',statements('add',8,0xffffffff)))
    mem=integer.alu_binop(context,instruction,[Operand('mem',mem_disp=0x100,mem_size=4),Operand('imm',imm=1)],'add')
    functions.append(function('memory32',mem))
    decrement=integer.inc_dec(context,instruction,[Operand('mem',mem_disp=0x200,mem_size=1)],'dec')
    functions.append(function('memory8',decrement))
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})
    code='''#include <stdint.h>
typedef struct { uint32_t value,fa,fb; int32_t fas,fbs; int cf; } State;
static volatile uint32_t storage;
static unsigned accesses;
static volatile uint32_t *tracked(uint32_t addr) { (void)addr; ++accesses; return &storage; }
#define MEM32(addr) (*tracked(addr))
static volatile uint8_t storage8;
static unsigned accesses8,hooks;
static volatile uint8_t *tracked8(uint32_t addr) { (void)addr; ++accesses8; return &storage8; }
static uint8_t load_hook(uint32_t site,uint8_t value) { if(site==0x1000)++hooks;return (uint8_t)(value+5); }
#define MEM8(addr) (*tracked8(addr))
#define RECOMP_LOAD_U8_AT(site,value) load_hook(site,value)
'''+macros+'\n'+''.join(functions)+'''
typedef State (*Operation)(uint32_t,uint32_t,uint32_t);
static Operation implementations[3][11]={
'''+',\n'.join('{'+','.join(f'{op}{bits}' for op in operations)+'}' for bits in (8,16,32))+'''};
static int verify(unsigned index,unsigned op,uint32_t a,uint32_t b,unsigned carry) {
unsigned bits=8u<<index; uint64_t modulo=1ull<<bits; uint32_t mask=(uint32_t)(modulo-1);
uint32_t left=a&mask,right=b&mask,expected=0; unsigned cf=carry;
switch(op) {
case 0:expected=(uint32_t)((uint64_t)left+right);cf=((uint64_t)left+right)>=modulo;break;
case 1:expected=left-right;cf=left<right;break;
case 2:expected=left&right;cf=0;break;
case 3:expected=left|right;cf=0;break;
case 4:expected=left^right;cf=0;break;
case 5:expected=(uint32_t)((uint64_t)left+right+carry);cf=((uint64_t)left+right+carry)>=modulo;break;
case 6:expected=left-right-carry;cf=(uint64_t)left<(uint64_t)right+carry;break;
case 7:expected=left+1u;break;
case 8:expected=left-1u;break;
case 9:expected=0u-left;cf=left!=0;break;
case 10:expected=~left;break;
}
expected&=mask;
State actual=implementations[index][op](a,b,carry);
if(actual.value!=((a&~mask)|expected) || actual.cf!=(int)cf) return 1;
if(op==10) return actual.fa!=0x41414141u || actual.fb!=0x42424242u || actual.fas!=42 || actual.fbs!=43;
int32_t signed_result=(int32_t)((int64_t)expected-((expected&(modulo>>1))?(int64_t)modulo:0));
if(actual.fa!=expected || actual.fas!=signed_result) return 2;
if(op==0 || op==1) {
int32_t signed_source=(int32_t)((int64_t)right-((right&(modulo>>1))?(int64_t)modulo:0));
if(actual.fb!=right || actual.fbs!=signed_source) return 3;
}
if((op==7 || op==8) && actual.fb!=(expected==((modulo>>1)-(op==8)))) return 4;
return 0;
}
int main(void) {
for(unsigned a=0;a<256;a++) for(unsigned b=0;b<256;b++) for(unsigned carry=0;carry<2;carry++)
 for(unsigned op=0;op<11;op++) if(verify(0,op,0xa5a50000u|a,b,carry)) return 10;
uint32_t seed=0x12345678u;
for(unsigned index=1;index<3;index++) for(unsigned n=0;n<10000;n++) {
seed=seed*1664525u+1013904223u;uint32_t a=seed;
seed=seed*1664525u+1013904223u;uint32_t b=seed;
for(unsigned op=0;op<11;op++) if(verify(index,op,a,b,n&1)) return 11;
}
const uint32_t edges[]={0,1,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff};
for(unsigned i=0;i<11;i++) for(unsigned j=0;j<11;j++) for(unsigned op=0;op<11;op++)
 for(unsigned index=0;index<3;index++) for(unsigned carry=0;carry<2;carry++)
  if(verify(index,op,edges[i],edges[j],carry)) return 12;
State immediate=immediate8(0,0,0);if(immediate.value!=255 || immediate.cf!=0) return 13;
storage=0xffffffffu; accesses=0; State memory=memory32(0,0,0);
if(storage!=0 || accesses!=2 || memory.fa!=0 || memory.cf!=1) return 14;
storage8=10; accesses8=hooks=0; memory=memory8(0,0,1);
if(storage8!=14 || accesses8!=2 || hooks!=1 || memory.fa!=14 || memory.cf!=1) return 15;
return 0;}
'''
    _compile_and_run(tmp_path,code)


def _compile_and_run(tmp_path,code):
    compiler,env=native_compiler(tmp_path)
    source=tmp_path/'integer.c';source.write_text(code)
    exe=tmp_path/('integer.exe' if os.name=='nt' else 'integer')
    args=[compiler,'/nologo','/std:c11','/O2','/WX',str(source),f'/Fe:{exe}'] if os.name=='nt' else [compiler,'-std=c11','-O2','-Werror',str(source),'-o',str(exe)]
    build=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert build.returncode==0,build.stdout+build.stderr
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr


def test_decoded_carry_sequences_execute_with_existing_block_lowering(tmp_path):
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/lifter.py').is_file():
        pytest.skip('Pinned block-lowering consumer unavailable')
    script='''import json
from tools.recomp.lifter import Lifter, lift_basic_block
from tools.recomp.disasm import BasicBlock
from translation_decode import Disassembler
cases={'neg_sbb':'f7d819c0','neg_adc':'f7d883d200','neg_push_sbb':'f7d85719c0',
       'cmp_sbb':'2c413c1a1ac9','add_adc':'01d083d200','sub_sbb':'29d083da00'}
output={}
for name,raw in cases.items():
 data=bytes.fromhex(raw)
 instructions=Disassembler().disassemble_function(data,0x1000,0x1000+len(data))
 lifter=Lifter()
 lifter.needs_cf=name in ('cmp_sbb','add_adc','sub_sbb')
 lines,_=lift_basic_block(lifter,BasicBlock(start=0x1000,instructions=instructions))
 output[name]='\\n'.join(lines)
print(json.dumps(output))
'''
    result=subprocess.run([sys.executable,'-c',script],cwd=runtime,
                          env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),
                          capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    bodies=json.loads(result.stdout)
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16','PUSH32'})
    code='''#include <stdint.h>
typedef struct { uint32_t a,d,c,sp,saved; int cf; } State;
#define MEM32(addr) stack[(addr)/4]
'''+macros+'\n'
    for name,body in bodies.items():
        code+=f'''static State {name}(uint32_t a,uint32_t b) {{
uint32_t eax=a,ebx=0,ecx=b,edx=b,esi=0,edi=0x12345678u,ebp=0,esp=32,stack[16]={{0}};
uint32_t _fa=0,_fb=0; int32_t _fas=0,_fbs=0; int _cf=0;
{body}
State result={{eax,edx,ecx,esp,stack[7],_cf}};return result; }}
'''
    code+='''static int verify(uint32_t a,uint32_t b) {
State r=neg_sbb(a,b);if(r.a!=(a?0xffffffffu:0) || r.cf!=(a!=0)) return 1;
r=neg_adc(a,b);uint64_t sum=(uint64_t)b+(a!=0);
if(r.a!=0u-a || r.d!=(uint32_t)sum || r.cf!=(sum>>32)) return 2;
r=neg_push_sbb(a,b);
if(r.a!=(a?0xffffffffu:0) || r.cf!=(a!=0) || r.sp!=28 || r.saved!=0x12345678u) return 3;
r=cmp_sbb(a,b);uint32_t low=((a&255)-65)&255;uint32_t carry=low<26;
if(r.a!=((a&~255u)|low) || r.c!=((b&~255u)|(carry?255:0)) || r.cf!=carry) return 4;
r=add_adc(a,b);sum=(uint64_t)a+b;uint64_t upper=(uint64_t)b+(sum>>32);
if(r.a!=(uint32_t)sum || r.d!=(uint32_t)upper || r.cf!=(upper>>32)) return 5;
r=sub_sbb(a,b);carry=a<b;
if(r.a!=a-b || r.d!=b-carry || r.cf!=(b<carry)) return 6;
return 0; }
int main(void) {
const uint32_t edges[]={0,1,127,128,255,32767,32768,65535,0x7fffffffu,0x80000000u,0xffffffffu};
for(unsigned i=0;i<11;i++) for(unsigned j=0;j<11;j++) {int result=verify(edges[i],edges[j]);if(result)return result;}
for(unsigned i=0;i<256;i++) {int result=verify(0xaaaa0000u|i,0xbbbb0000u|i);if(result)return result;}
uint32_t seed=0x12345678u;
for(unsigned i=0;i<10000;i++) {seed=seed*1664525u+1013904223u;uint32_t a=seed;
seed=seed*1664525u+1013904223u;int result=verify(a,seed);if(result)return result;}
return 0; }
'''
    _compile_and_run(tmp_path,code)


def test_installed_lifter_uses_project_integer_and_existing_consumers():
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/lifter.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    code='''from tools.recomp.lifter import Lifter
import translation_integer as own
for old,new in [('alu_binop','alu_binop'),('inc_dec','inc_dec'),('neg','neg'),('not','bit_not'),('sbb','sbb'),('adc','adc')]:
 assert getattr(Lifter,'_lift_'+old) is getattr(own,new)
import pytest
raise SystemExit(pytest.main(['tools/recomp','-q','--tb=short']))
'''
    result=subprocess.run([sys.executable,'-c',code],cwd=runtime,
                          env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),
                          capture_output=True,text=True,timeout=300)
    assert result.returncode==0,result.stdout+result.stderr
