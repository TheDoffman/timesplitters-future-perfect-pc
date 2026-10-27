"""Original multiply/divide contracts, including native x86 result oracles."""

from project_paths import source_file as repo_file

import os
import json
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/compiler'))
import translation_product as product
from translation_decode import Operand
from native_compiler import native_compiler
from test_translation_moves import _production_macros


def _compile(tmp_path, source, extra=(), objects=()):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('product.exe' if os.name == 'nt' else 'product')
    command = ([compiler, '/nologo', '/std:c11', '/O2', '/WX', str(source), *map(str, extra), *objects, f'/Fe:{exe}']
               if os.name == 'nt' else
               [compiler, '-std=c11', '-O2', '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(source), *map(str, extra), *objects, '-o', str(exe)])
    run = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=90)
    assert run.returncode == 0, run.stdout+run.stderr
    return exe


def test_invalid_product_forms_fail_before_emission():
    c = SimpleNamespace(needs_cf=True); i = SimpleNamespace(address=0x1234)
    for operands in ([], [Operand('imm', imm=2)], [Operand('reg', reg='xmm0')],
                     [Operand('reg', reg='ax')]*2, [Operand('mem', mem_size=8)]):
        with pytest.raises(ValueError): product.muldiv(c, i, operands, 'div')
    for operands in ([Operand('reg', reg='al')]*2,
                     [Operand('mem', mem_size=4), Operand('reg', reg='eax')],
                     [Operand('reg', reg='eax'), Operand('reg', reg='cx')],
                     [Operand('reg', reg='eax')]*3):
        with pytest.raises(ValueError): product.imul(c, i, operands)


def test_product_arithmetic_aliases_and_faults_execute(tmp_path):
    context = SimpleNamespace(needs_cf=True); insn = SimpleNamespace(address=0x1234)
    operations = ('mul', 'imul', 'div', 'idiv', 'imul2', 'imul3')
    functions = []; assembly = ['.code']; declarations = []; rows = []; hw_rows = []
    def emit(op, width, operand=None):
        target = {8:'al',16:'ax',32:'eax'}[width]
        source = {8:'cl',16:'cx',32:'ecx'}[width]
        args = [operand or Operand('reg', reg=source)]
        if op in ('imul2','imul3'): args.insert(0, Operand('reg', reg=target))
        if op == 'imul3': args.append(Operand('imm', imm=-7))
        return (product.imul(context, insn, args) if op.startswith('imul') else
                product.muldiv(context, insn, args, op))
    def function(name, lines):
        return f'''static void {name}(uint32_t a,uint32_t d,uint32_t b) {{
eax=a;edx=d;ecx=b;ebp=0xabcdu;g_ebp=0;fault_reason=0;_cf=7;
_fa=0x41414141u;_fb=0x42424242u;_fas=42;_fbs=43;
if(setjmp(fault_jump)==0) {{
{chr(10).join(lines)}
}}
}}
'''
    for width in (8,16,32):
        row = []; hw_row = []
        for op in operations:
            if width == 8 and op in ('imul2','imul3'):
                row.append('0'); hw_row.append('0'); continue
            name = f'{op}_{width}'; row.append(name); hw_row.append('hw_'+name)
            functions.append(function(name, emit(op, width)))
            target = {8:'al',16:'ax',32:'eax'}[width]; source = {8:'cl',16:'cx',32:'ecx'}[width]
            instruction = (f'imul {target}, {source}, -7' if op == 'imul3' else
                           f'imul {target}, {source}' if op == 'imul2' else f'{op} {source}')
            assembly += [f'public hw_{name}',f'hw_{name} proc',
                         'mov r10,rcx','mov eax,edx','mov edx,r8d','mov ecx,r9d', instruction,
                         'setc r8b','seto r9b','mov dword ptr [r10],eax','mov dword ptr [r10+4],edx',
                         'movzx r8d,r8b','movzx r9d,r9b','mov dword ptr [r10+8],r8d',
                         'mov dword ptr [r10+12],r9d','ret',f'hw_{name} endp']
            declarations.append(f'extern void hw_{name}(State*,uint32_t,uint32_t,uint32_t);')
        rows.append('{'+','.join(row)+'}'); hw_rows.append('{'+','.join(hw_row)+'}')
    for width in (8,16,32):
        for source in ({8:('al','ah'),16:('ax','dx'),32:('eax','edx')}[width]):
            for op in ('div','idiv'):
                functions.append(function(f'{op}_{source}',emit(op,width,Operand('reg',reg=source))))
    functions.append(function('memory_div',emit('div',32,Operand('mem',mem_base='eax',mem_size=4))))
    assembly.append('end'); objects = []
    if os.name == 'nt':
        compiler, env = native_compiler(tmp_path)
        assembler = Path(compiler).with_name('ml64.exe'); assert assembler.is_file()
        asm = tmp_path/'reference.asm'; asm.write_text('\n'.join(assembly)+'\n',encoding='utf-8')
        obj = tmp_path/'reference.obj'
        run = subprocess.run([str(assembler),'/nologo','/c',f'/Fo{obj}',str(asm)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
        assert run.returncode == 0,run.stdout+run.stderr
        objects = [str(obj)]
    macros = _production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})
    code = '''#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <setjmp.h>
typedef struct {uint32_t a,d;unsigned cf,of;} State;
static uint32_t eax,edx,ecx,ebp,g_ebp,_fa,_fb;
static int32_t _fas,_fbs;static int _cf;static unsigned fault_reason;
static jmp_buf fault_jump;
static void tsfp_guest_divide_error(uint32_t site,unsigned reason) {
if(site!=0x1234u || g_ebp!=0xabcdu)longjmp(fault_jump,99);
fault_reason=reason+1;longjmp(fault_jump,1);}
static uint32_t memory_value=7;static unsigned reads;static uint32_t last_address;
static uint32_t *memory(uint32_t address){++reads;last_address=address;return &memory_value;}
#define MEM32(address) (*memory(address))
'''+macros+'\n'+''.join(functions)+'''
typedef void (*Operation)(uint32_t,uint32_t,uint32_t);
static Operation operations[3][6]={'''+','.join(rows)+'''};
'''
    if os.name == 'nt':
        code += '\n'.join(declarations)+'\ntypedef void (*Hardware)(State*,uint32_t,uint32_t,uint32_t);\nstatic Hardware hardware[3][6]={'+','.join(hw_rows)+'};\n'
    code += '''static int64_t signed_value(uint64_t value,unsigned width) {
uint64_t sign=1ull<<(width-1);
return value<sign ? (int64_t)value : -(int64_t)(sign-1)-1+(int64_t)(value-sign);
}
static int verify(unsigned index,unsigned op,uint32_t a,uint32_t d,uint32_t b,Operation impl) {
unsigned width=8u<<index,cf=7,fault=0;uint64_t modulo=1ull<<width,mask=modulo-1;
uint32_t want_a=a,want_d=d;uint64_t low=0,high=0;
if(op==0 || op==1 || op>=4) {
uint64_t left=op==5?(b&mask):(a&mask),right=op==5?((-7ull)&mask):(b&mask),value;
if(op==0){value=left*right;cf=value>mask;}
else {int64_t result=signed_value(left,width)*signed_value(right,width);
value=(uint64_t)result;cf=result<-(int64_t)(modulo/2) || result>(int64_t)(modulo/2)-1;}
if(op>=4)want_a=(a&~(uint32_t)mask)|((uint32_t)value&(uint32_t)mask);
else if(width==8)want_a=(a&0xffff0000u)|((uint32_t)value&0xffffu);
else {want_a=(a&~(uint32_t)mask)|((uint32_t)value&(uint32_t)mask);
want_d=(d&~(uint32_t)mask)|((uint32_t)(value>>width)&(uint32_t)mask);}
} else {
uint64_t raw=width==8?(a&0xffffu):(((uint64_t)d&mask)<<width)|(a&mask);
if(!(b&mask))fault=1;
else if(op==2){low=raw/(b&mask);high=raw%(b&mask);if(low>mask)fault=2;}
else {int64_t dividend=signed_value(raw,width*2),denominator=signed_value(b&mask,width);
if(dividend==INT64_MIN && denominator==-1)fault=2;
else {int64_t q=dividend/denominator,r=dividend%denominator;
if(q<-(int64_t)(modulo/2)||q>(int64_t)(modulo/2)-1)fault=2;
low=(uint64_t)q;high=(uint64_t)r;}}
if(!fault) {
if(width==8)want_a=(a&0xffff0000u)|((uint32_t)low&255u)|(((uint32_t)high&255u)<<8);
else {want_a=(a&~(uint32_t)mask)|((uint32_t)low&(uint32_t)mask);
want_d=(d&~(uint32_t)mask)|((uint32_t)high&(uint32_t)mask);}}
}
impl(a,d,b);
if(eax!=want_a||edx!=want_d||_cf!=(int)cf||fault_reason!=fault||
_fa!=0x41414141u||_fb!=0x42424242u||_fas!=42||_fbs!=43) {
printf("reference width=%u op=%u a=%x d=%x b=%x got=%x/%x/%d/%u expected=%x/%x/%u/%u\\n",width,op,a,d,b,eax,edx,_cf,fault_reason,want_a,want_d,cf,fault);return 1;}
'''
    if os.name == 'nt':
        code += '''if(!fault) {State hw={0};hardware[index][op](&hw,a,d,b);
if(hw.a!=want_a||hw.d!=want_d||((op<2||op>=4)&&(hw.cf!=cf||hw.of!=cf))) {
printf("hardware width=%u op=%u a=%x d=%x b=%x\\n",width,op,a,d,b);return 2;}}
'''
    code += '''return 0;}
int main(void) {
for(unsigned a=0;a<256;a++)for(unsigned b=0;b<256;b++)for(unsigned op=0;op<2;op++)
if(verify(0,op,0xa5a50000u|a,0x12345678u,b,operations[0][op]))return 1;
const uint32_t divisors[]={0,1,2,3,7,127,128,129,254,255};
for(unsigned a=0;a<65536;a++)for(unsigned b=0;b<10;b++)for(unsigned op=2;op<4;op++)
if(verify(0,op,0xa5a50000u|a,0x12345678u,divisors[b],operations[0][op]))return 2;
uint32_t seed=0x12345678u;
const uint32_t edge[]={0,1,2,7,127,128,255,32767,32768,65535,0x7fffffffu,0x80000000u,0xffffffffu};
for(unsigned index=1;index<3;index++)for(unsigned n=0;n<12000;n++) {
seed=seed*1664525u+1013904223u;uint32_t a=n<169?edge[n%13]:seed;
seed=seed*1664525u+1013904223u;uint32_t d=n<169?edge[n/13]:seed;
seed=seed*1664525u+1013904223u;uint32_t b=n<169?edge[(n+n/13)%13]:seed;
for(unsigned op=0;op<6;op++)if(verify(index,op,a,d,b,operations[index][op]))return 3;
/* Also force small high halves so wide division has plentiful valid cases. */
for(unsigned op=2;op<4;op++)if(verify(index,op,a,0,b,operations[index][op]))return 4;
}
'''
    for width in (8,16,32):
        for register in {8:('al','ah'),16:('ax','dx'),32:('eax','edx')}[width]:
            b = {'al':'a&255u','ah':'(a>>8)&255u','ax':'a&65535u','dx':'d&65535u','eax':'a','edx':'d'}[register]
            for op, index in (('div',2),('idiv',3)):
                width_index = {8:0,16:1,32:2}[width]
                code += f'''for(unsigned n=0;n<1024;n++) {{ seed=seed*1664525u+1013904223u;uint32_t a=seed;
seed=seed*1664525u+1013904223u;uint32_t d=seed;
if(verify({width_index},{index},a,d,{b},{op}_{register}))return 5;}}
'''
    code += '''reads=0;memory_div(100,0,0);
if(eax!=14||edx!=2||reads!=1||last_address!=100)return 6;
return 0;}
'''
    source=tmp_path/'product.c';source.write_text(code,encoding='utf-8')
    exe=_compile(tmp_path,source,objects=objects)
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=90)
    assert run.returncode==0,run.stdout+run.stderr


def test_fatal_divide_adapter_reports_site_and_uses_host_fault(tmp_path):
    if os.name=='nt':
        code='''#include <windows.h>
#include <stdint.h>
void tsfp_guest_divide_error(uint32_t,unsigned);
static int expected;
static int filter(EXCEPTION_POINTERS *p) {
EXCEPTION_RECORD *r=p->ExceptionRecord;
expected=r->ExceptionCode==EXCEPTION_INT_DIVIDE_BY_ZERO &&
(r->ExceptionFlags&EXCEPTION_NONCONTINUABLE) && r->NumberParameters==2 &&
r->ExceptionInformation[0]==0x1234 && r->ExceptionInformation[1]==1;
return EXCEPTION_EXECUTE_HANDLER;}
int main(void) {__try {tsfp_guest_divide_error(0x1234,1);return 10;}
__except(filter(GetExceptionInformation())) {return expected?0:11;}}
'''
    else:
        code='''#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
void tsfp_guest_divide_error(uint32_t,unsigned);
static void caught(int s){_Exit(s==SIGFPE?0:11);}
int main(void){signal(SIGFPE,caught);tsfp_guest_divide_error(0x1234,1);return 10;}
'''
    source=tmp_path/'fault.c';source.write_text(code,encoding='utf-8')
    exe=_compile(tmp_path,source,extra=(ROOT/'src/runtime/guest_integer_fault.c',))
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=15)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'site=0x00001234 reason=quotient-overflow' in run.stderr


def test_copied_product_lowerer_runs_without_toolkit(tmp_path):
    for name in ('translation_product.py','translation_integer.py','translation_operands.py'):
        shutil.copyfile(repo_file(ROOT, 'tools', name),tmp_path/name)
    code='''import sys
sys.path.insert(0,sys.argv[1])
from types import SimpleNamespace as S
import translation_product as p
out=p.muldiv(S(needs_cf=True),S(address=1),[S(type='reg',reg='eax')],'idiv')
assert any('tsfp_guest_divide_error' in line for line in out)
assert not any(name.startswith('tools.recomp') for name in sys.modules)
'''
    run=subprocess.run([sys.executable,'-I','-c',code,str(tmp_path)],cwd=tmp_path,capture_output=True,text=True,timeout=15)
    assert run.returncode==0,run.stdout+run.stderr


def test_installed_product_translation_compiles_conditions_and_fault_frames(tmp_path):
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/lifter.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    script='''import json
from tools.recomp.lifter import Lifter,_make_condition
from tools.recomp.translator import FunctionTranslator
import translation_product as p
assert Lifter._lift_imul is p.imul and Lifter._lift_muldiv is p.muldiv
for kind in ('mul','imul'):
 for condition in ('jb','jnae','jo'):
  assert _make_condition(condition,kind,[])[0]=='_cf'
 for condition in ('jae','jnb','jno'):
  assert _make_condition(condition,kind,[])[0]=='!_cf'
 for condition in ('je','js','jp','jbe'):
  assert _make_condition(condition,kind,[]) is None
cases={'carry_probe':'f7e10f92c30f93c7c3','overflow_probe':'0fafc10f90c30f91c7c3',
       'division_probe':'f7f0c3','signed_probe':'f7f9c3',
       'cmov_probe':'0fafc1bb00000000ba010000000f40daba020000000f41cac3',
       'branch_probe':'0fafc17006bb00000000c3bb01000000c3'}
output={}
for name,raw in cases.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)}
 t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 output[name]=t.translate_function(0x1000,info)
assert 'uint32_t ebp = 0' in output['division_probe']
print(json.dumps(output))
'''
    run=subprocess.run([sys.executable,'-c',script],cwd=runtime,env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    bodies=json.loads(run.stdout)
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})
    code='''#include <stdint.h>
#include <setjmp.h>
static uint32_t eax,edx,ecx,ebx,esp,g_ebp,g_seh_ebp;
static jmp_buf trap;
static unsigned reason;
static void tsfp_guest_divide_error(uint32_t site,unsigned why){reason=site==0x1000?why+1:99;longjmp(trap,1);}
#define RECOMP_FAST_ENTRY(site) 0
'''+macros+'\n'+'\n'.join(bodies.values())+'''
int main(void) {
const uint32_t values[]={0,1,7,0x7fffffffu,0x80000000u,0xffffffffu};
for(unsigned i=0;i<6;i++)for(unsigned j=0;j<6;j++) {
eax=values[i];ecx=values[j];ebx=0xabcd0000u;
carry_probe();unsigned carry=((uint64_t)values[i]*values[j])>0xffffffffu;
if((ebx&255u)!=carry || ((ebx>>8)&255u)!=(1-carry))return 1;
int64_t a=(int64_t)values[i]-(values[i]&0x80000000u?0x100000000ll:0);
int64_t b=(int64_t)values[j]-(values[j]&0x80000000u?0x100000000ll:0);
int of=a*b>0x7fffffffll || a*b<(-0x7fffffffll-1);
eax=values[i];ecx=values[j];overflow_probe();if((ebx&255u)!=of || ((ebx>>8)&255u)!=(unsigned)(1-of))return 2;
eax=values[i];ecx=values[j];cmov_probe();if(ebx!=(unsigned)of || ecx!=(of?values[j]:2u))return 9;
eax=values[i];ecx=values[j];branch_probe();if(ebx!=(unsigned)of)return 3;
}
eax=77;edx=0;esp=100;division_probe();if(eax!=1||edx!=0||esp!=104)return 4;
eax=0;edx=77;esp=100;g_seh_ebp=0x3456;reason=0;
if(!setjmp(trap)){division_probe();return 5;}
if(eax!=0||edx!=77||esp!=100||g_ebp!=0x3456||reason!=1)return 6;
eax=0;edx=0x80000000u;ecx=0xffffffffu;reason=0;
if(!setjmp(trap)){signed_probe();return 7;}
if(eax!=0||edx!=0x80000000u||reason!=2)return 8;
return 0;}
'''
    source=tmp_path/'consumer.c';source.write_text(code,encoding='utf-8')
    exe=_compile(tmp_path,source)
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr


def test_existing_translator_consumers():
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/lifter.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    run=subprocess.run([sys.executable,'-m','pytest','tools/recomp','-q','--tb=short'],cwd=runtime,env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=300)
    assert run.returncode==0,run.stdout+run.stderr
