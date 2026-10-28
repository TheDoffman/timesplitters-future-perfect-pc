"""Original shift/rotate contracts with portable and native x86 references."""

from project_paths import source_file as repo_file

import os
from pathlib import Path
import subprocess
import shutil
import sys
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_bits as bits
from translation_decode import Operand
from native_compiler import native_compiler
from test_translation_moves import _production_macros

OPERATIONS=('shl','shr','sar','rol','ror','rcl','rcr','shld','shrd')


def emitted(operation,width):
    target={8:'al',16:'ax',32:'eax'}[width]
    source={16:'dx',32:'edx'}.get(width)
    operands=[Operand('reg',reg=target)]
    if operation in ('shld','shrd'):operands.append(Operand('reg',reg=source))
    operands.append(Operand('reg',reg='cl'))
    context=SimpleNamespace(needs_cf=True);instruction=SimpleNamespace(address=0x1000)
    if operation in ('shl','shr'):return bits.shift(context,instruction,operands,'<<' if operation=='shl' else '>>')
    if operation in ('shld','shrd'):return bits.double_shift(context,instruction,operands,operation)
    if operation in ('rol','ror'):return bits.rotate(context,instruction,operands,operation)
    if operation in ('rcl','rcr'):return bits.rotate_carry(context,instruction,operands,operation)
    return bits.sar(context,instruction,operands)


def test_invalid_forms_are_rejected():
    c=SimpleNamespace(needs_cf=True);i=SimpleNamespace(address=0)
    for operands in ([],[Operand('reg',reg='eax')],
                     [Operand('reg',reg='eax'),Operand('reg',reg='dl')],
                     [Operand('reg',reg='fs'),Operand('imm',imm=1)]):
        with pytest.raises(ValueError):bits.shift(c,i,operands,'<<')
    with pytest.raises(ValueError):bits.double_shift(c,i,[Operand('reg',reg='al'),Operand('reg',reg='dl'),Operand('imm',imm=1)],'shld')


def test_native_shift_rotate_results_carries_and_zero_count_state(tmp_path):
    functions=[];names=[];assembly=['.code'];declarations=[]
    for width in (8,16,32):
        for op in OPERATIONS:
            if width==8 and op in ('shld','shrd'):continue
            name=f'{op}{width}';names.append(name)
            body='\n'.join(emitted(op,width))
            functions.append(f'''static State {name}(uint32_t a,uint32_t b,uint32_t count,unsigned carry) {{
uint32_t eax=a,edx=b,ecx=count,_fa=0x41414141u;int32_t _fas=42;int _cf=(int)carry;
{body}
State result={{eax,_fa,_fas,_cf}};return result; }}
''')
            target={8:'al',16:'ax',32:'eax'}[width]
            source={16:'dx',32:'edx'}.get(width)
            operands=f'{target}, {source}, cl' if op in ('shld','shrd') else f'{target}, cl'
            assembly += [f'public hw_{name}',f'hw_{name} proc','mov eax,ecx','mov ecx,r8d',
                         'bt r9d,0',f'{op} {operands}','setc r10b','movzx r10d,r10b',
                         'shl r10,32','or rax,r10','ret',f'hw_{name} endp']
            declarations.append(f'extern uint64_t hw_{name}(uint32_t,uint32_t,uint32_t,uint32_t);')
    assembly.append('end')
    compiler,env=native_compiler(tmp_path)
    native=os.name=='nt'
    objects=[]
    if native:
        assembler=Path(compiler).with_name('ml64.exe')
        assert assembler.is_file(),'MSVC x64 assembler required for the hardware reference'
        source=tmp_path/'reference.asm';source.write_text('\n'.join(assembly)+'\n')
        obj=tmp_path/'reference.obj'
        run=subprocess.run([str(assembler),'/nologo','/c',f'/Fo{obj}',str(source)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
        assert run.returncode==0,run.stdout+run.stderr
        objects=[str(obj)]
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})
    tables=[];hw_tables=[]
    for width in (8,16,32):
        row=[f'{op}{width}' if not (width==8 and op in ('shld','shrd')) else '0' for op in OPERATIONS]
        tables.append('{'+','.join(row)+'}')
        hw_tables.append('{'+','.join('hw_'+name if name!='0' else '0' for name in row)+'}')
    hardware=('\n'.join(declarations)+'\nstatic Hardware hardware[3][9]={'+','.join(hw_tables)+'};') if native else ''
    code='''#include <stdint.h>
#include <stdio.h>
typedef struct {uint32_t value,fa;int32_t fas;int cf;} State;
typedef State (*Operation)(uint32_t,uint32_t,uint32_t,unsigned);
typedef uint64_t (*Hardware)(uint32_t,uint32_t,uint32_t,uint32_t);
'''+macros+'\n'+''.join(functions)+'\nstatic Operation operations[3][9]={'+','.join(tables)+'};\n'+hardware+'''
static int verify(unsigned index,unsigned op,uint32_t a,uint32_t b,unsigned count,unsigned carry) {
unsigned width=8u<<index,n=count&31u;uint32_t mask=(uint32_t)((1ull<<width)-1),v=a&mask,s=b&mask;
unsigned cf=carry;int update=(op<3 || op>=7) && n && (op<7 || n<=width);
if(op>=7 && n>width)n=0; /* explicit policy for undefined narrow double shifts */
for(unsigned j=0;j<n;j++) {
unsigned top=v>>(width-1),bottom=v&1u,incoming=cf;
switch(op) {
case 0:cf=top;v=(v<<1)&mask;break;
case 1:cf=bottom;v>>=1;break;
case 2:cf=bottom;v=(v>>1)|(top<<(width-1));break;
case 3:cf=top;v=((v<<1)|top)&mask;break;
case 4:cf=bottom;v=(v>>1)|(bottom<<(width-1));break;
case 5:cf=top;v=((v<<1)|incoming)&mask;break;
case 6:cf=bottom;v=(v>>1)|(incoming<<(width-1));break;
case 7:cf=top;v=((v<<1)|(s>>(width-1)))&mask;s=(s<<1)&mask;break;
case 8:cf=bottom;v=(v>>1)|((s&1u)<<(width-1));s>>=1;break;
}}
State actual=operations[index][op](a,b,count,carry);
int32_t signed_result=(int32_t)((int64_t)v-((v&(1u<<(width-1)))?(1ll<<width):0));
if(actual.value!=((a&~mask)|v) || actual.cf!=(int)cf || actual.fa!=(update?v:0x41414141u) || actual.fas!=(update?signed_result:42)) {
printf("reference mismatch width=%u op=%u a=%x b=%x count=%u carry=%u got=%x/%d want=%x/%u\\n",width,op,a,b,count,carry,actual.value,actual.cf,(a&~mask)|v,cf);return 1;}
'''
    if native:
        code+='''if(!(op>=7 && (count&31u)>width)) {
uint64_t observed=hardware[index][op](a,b,count,carry);
int carry_defined=!(op<2 && (count&31u)>=width);
if(actual.value!=(uint32_t)observed || (carry_defined && actual.cf!=(int)(observed>>32))) {
printf("hardware mismatch width=%u op=%u a=%x b=%x count=%u carry=%u got=%x/%d native=%x/%u\\n",width,op,a,b,count,carry,actual.value,actual.cf,(uint32_t)observed,(unsigned)(observed>>32));return 2;}}
'''
    code+='''return 0;}
int main(void) {
uint32_t seed=0x12345678u;
const uint32_t edges[]={0,1,127,128,255,32767,32768,65535,0x7fffffffu,0x80000000u,0xffffffffu};
for(unsigned index=0;index<3;index++) for(unsigned sample=0;sample<(index?1024u:256u);sample++) {
seed=seed*1664525u+1013904223u;uint32_t a=index?(sample<11?edges[sample]:seed):(0xa5a50000u|sample);
seed=seed*1664525u+1013904223u;uint32_t b=sample<11?edges[10-sample]:seed;
for(unsigned count=0;count<256;count++) for(unsigned carry=0;carry<2;carry++)
for(unsigned op=0;op<(index?9u:7u);op++) {int status=verify(index,op,a,b,count,carry);if(status)return status;}
}return 0;}
'''
    source=tmp_path/'bits.c';source.write_text(code)
    exe=tmp_path/('bits.exe' if native else 'bits')
    args=[compiler,'/nologo','/std:c11','/O2','/WX',str(source),*objects,f'/Fe:{exe}'] if native else [compiler,'-std=c11','-O2','-Werror',str(source),'-o',str(exe)]
    run=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr


def test_copied_bit_lowerer_runs_without_toolkit(tmp_path):
    for name in ('translation_bits.py','translation_integer.py','translation_operands.py'):
        shutil.copyfile(repo_file(ROOT, 'tools', name),tmp_path/name)
    code='''import sys
sys.path.insert(0,sys.argv[1])
from types import SimpleNamespace as S
import translation_bits as bits
ops=[S(type='reg',reg='eax'),S(type='imm',imm=32)]
assert bits.constant_noop(S(mnemonic='shl',operands=ops))
result=bits.shift(S(needs_cf=True),S(),ops,'<<')
assert 'if (_bit_count) {' in result
assert not any(name.startswith('tools.recomp') for name in sys.modules)
'''
    run=subprocess.run([sys.executable,'-I','-c',code,str(tmp_path)],cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr


def test_installed_handlers_and_flag_routing_with_existing_consumers():
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/lifter.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    code='''from tools.recomp.lifter import Lifter, lift_basic_block, _make_condition
from tools.recomp.translator import FunctionTranslator
from translation_decode import Disassembler, BasicBlock
import translation_bits as bits
for name in ('shift','sar','double_shift','shld','shrd','rotate','rotate_carry'):
 assert getattr(Lifter,'_lift_'+name) is getattr(bits,name)
decoder=Disassembler()
for raw in ('c1e000','c1e820','c1f800','0fa4d000','0facd000','c1c000','c1c800','c1d000','c1d800'):
 data=bytes.fromhex('83f805'+raw+'7400')
 instructions=decoder.disassemble_function(data,0x1000,0x1000+len(data))
 assert bits.constant_noop(instructions[1])
 _,state=lift_basic_block(Lifter(),BasicBlock(start=0x1000,instructions=instructions))
 assert state[0]=='cmp',(raw,state)
 if instructions[1].mnemonic in ('rcl','rcr'):
  assert FunctionTranslator._function_needs_cf(instructions), 'Carry rotate C still references _cf at count zero'
for raw in ('c1c001','c1c801','0fa4d001','0facd001'):
 data=bytes.fromhex(raw+'7200')
 instructions=decoder.disassemble_function(data,0x1000,0x1000+len(data))
 assert FunctionTranslator._function_needs_cf(instructions)
 assert _make_condition('jb',instructions[0].mnemonic,instructions[0].operands)[0]=='_cf'
import pytest
raise SystemExit(pytest.main(['tools/recomp','-q','--tb=short']))
'''
    run=subprocess.run([sys.executable,'-c',code],cwd=runtime,env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=300)
    assert run.returncode==0,run.stdout+run.stderr
