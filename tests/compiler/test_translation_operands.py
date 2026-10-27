"""Original operand-expression fixtures and installed translator contracts."""


import os
from pathlib import Path
import subprocess
import sys
import re
import shutil

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import translation_operands as expressions
from translation_decode import Operand, Disassembler
from native_compiler import native_compiler


@pytest.mark.parametrize('name,parent,macro,width',[
    ('al','eax','LO8',1),('ah','eax','HI8',1),('ax','eax','LO16',2),
    ('bl','ebx','LO8',1),('ch','ecx','HI8',1),('dx','edx','LO16',2),
    ('si','esi','LO16',2),('di','edi','LO16',2),('bp','ebp','LO16',2),('sp','esp','LO16',2)])
def test_subregister_mapping(name,parent,macro,width):
    assert expressions._fmt_reg(name)==f'{macro}({parent})'
    assert expressions._fmt_set_reg(name,'value')==f'SET_{macro}({parent}, value);'
    assert expressions._operand_width(Operand('reg',reg=name))==width


def test_address_scale_fs_and_absolute_wrap():
    op=Operand('mem',mem_seg='fs',mem_base='eax',mem_index='ecx',mem_scale=4,mem_disp=-16,mem_size=4)
    assert expressions._fmt_mem(op)=='XBOX_FS_BASE + eax + ecx * 4 + -16'
    assert expressions._fmt_mem(Operand('mem',mem_disp=-1))=='0xFFFFFFFFu'
    op.mem_scale=3
    with pytest.raises(ValueError):expressions._fmt_mem(op)


def test_integer_and_floating_accessors_do_not_guess_width():
    for width in (1,2,4):
        op=Operand('mem',mem_disp=0x1000,mem_size=width)
        assert expressions._fmt_mem_read(op)==f'MEM{width*8}(0x1000)'
    with pytest.raises(ValueError):expressions._mem_accessor(8)
    with pytest.raises(ValueError):expressions._fmt_fpu_operand(Operand('mem',mem_size=10))
    assert expressions._fmt_fpu_operand(Operand('reg',reg='st(7)'))=='g_fp_stack[(g_fp_top + 7) & 7]'
    assert expressions._fmt_fpu_operand(Operand('reg',reg='st(8)')) is None


def test_invalid_register_and_operand_fail_before_c_emission():
    with pytest.raises(ValueError):expressions._fmt_reg('eax);abort(')
    with pytest.raises(ValueError):expressions._fmt_operand_write(Operand('imm',imm=1),'0')
    with pytest.raises(ValueError):expressions._fmt_operand_read(Operand('unknown'))
    with pytest.raises(ValueError):expressions._fmt_imm(2**32)


@pytest.mark.parametrize('fields',[
    dict(mem_base='xmm0'),dict(mem_base='al'),dict(mem_index='ecx',mem_scale=0),
    dict(mem_index='ecx',mem_scale=True),dict(mem_disp=2**32),dict(mem_disp=True),
    dict(mem_seg='invalid')])
def test_invalid_address_metadata_is_not_silently_reinterpreted(fields):
    with pytest.raises(ValueError):expressions._fmt_mem(Operand('mem',**fields))


def test_decoder_preserves_address_override_and_fs_order():
    ins=Disassembler().disassemble_function(bytes.fromhex('64678b00'),0x1000,0x1004)
    op=ins[0].operands[1]
    assert op.mem_addr_size==2
    assert expressions._fmt_mem(op)=='XBOX_FS_BASE + (uint16_t)(LO16(ebx) + LO16(esi))'


def test_register_widths_and_explicit_unmodeled_register_policy():
    for name,width in (('eax',4),('xmm2',16),('mm1',8),('st(1)',10)):
        assert expressions._operand_width(Operand('reg',reg=name))==width
    assert expressions._operand_width(Operand('imm',imm=4)) is None
    for name in ('fs','cr3','dr7','tr0'):
        assert expressions._fmt_reg(name).startswith('0 /*')
        assert expressions._fmt_set_reg(name,'3').startswith('/* mov ')


def test_copied_expression_module_is_toolkit_independent(tmp_path):
    shutil.copyfile(ROOT/'tools/compiler/translation_operands.py',tmp_path/'translation_operands.py')
    code='''import sys
sys.path.insert(0,sys.argv[1])
from types import SimpleNamespace
import translation_operands as own
assert own._fmt_mem(SimpleNamespace(mem_seg='fs',mem_base='eax',mem_index=None,
    mem_scale=1,mem_disp=4))=='XBOX_FS_BASE + eax + 4'
assert not any(name.startswith('tools.recomp') for name in sys.modules)
'''
    result=subprocess.run([sys.executable,'-I','-c',code,str(tmp_path)],cwd=tmp_path,
                          capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr


def test_installed_lifter_uses_project_expressions_and_runs_consumers():
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/lifter.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    code='''from tools.recomp import lifter
import translation_operands as own
for name in vars(own):
 if name.startswith('_fmt_') or name in ('_operand_width','_mem_accessor','_smem_accessor'):
  assert getattr(lifter,name) is getattr(own,name),name
import pytest
raise SystemExit(pytest.main(['tools/recomp','-q','--tb=short']))
'''
    result=subprocess.run([sys.executable,'-c',code],cwd=runtime,
                          env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),
                          capture_output=True,text=True,timeout=300)
    assert result.returncode==0,result.stdout+result.stderr


def test_emitted_subregister_writes_and_address_math_execute(tmp_path):
    compiler,env=native_compiler(tmp_path)
    blocks=[]
    for name,(parent,bits,macro) in expressions._PARTS.items():
        shift=8 if macro=='HI8' else 0
        mask=((1<<bits)-1)<<shift
        blocks.append(f'''{{ uint32_t {parent}=seed;
{expressions._fmt_set_reg(name,'value')}
if ({parent} != ((seed & 0x{(0xffffffff ^ mask):08X}u) | ((value << {shift}) & 0x{mask:08X}u))) return 1;
if ({expressions._fmt_reg(name)} != (value & {(1<<bits)-1}u)) return 2;
}}''')
    op=Operand('mem',mem_seg='fs',mem_base='eax',mem_index='ecx',mem_scale=8,mem_disp=-32,mem_size=4)
    narrow=Operand('mem',mem_seg='fs',mem_base='bx',mem_index='si',mem_disp=-32,mem_addr_size=2)
    code='''#include <stdint.h>
#define LO8(x) ((uint8_t)(x))
#define HI8(x) ((uint8_t)((x)>>8))
#define LO16(x) ((uint16_t)(x))
#define SET_LO8(x,v) ((x)=((x)&0xffffff00u)|((uint32_t)(v)&255u))
#define SET_HI8(x,v) ((x)=((x)&0xffff00ffu)|(((uint32_t)(v)&255u)<<8))
#define SET_LO16(x,v) ((x)=((x)&0xffff0000u)|((uint32_t)(v)&65535u))
#define XBOX_FS_BASE 0x7ff00000u
static double g_fp_stack[8];
static unsigned g_fp_top;
int main(void) {
uint32_t seed=0x89abcdefu,value=0;
for(int n=0;n<1000;n++) {
seed=seed*1664525u+1013904223u; value=seed^0xfedcba98u;
'''+ '\n'.join(blocks)+f'''
{{uint32_t eax=seed,ecx=value;
uint32_t got={expressions._fmt_mem(op)};
if(got!=(uint32_t)((uint64_t)0x7ff00000u+seed+(uint64_t)value*8-32)) return 3;
}}
{{uint32_t ebx=seed,esi=value;
uint32_t got={expressions._fmt_mem(narrow)};
if(got!=0x7ff00000u+(uint16_t)((seed&65535u)+(value&65535u)-32)) return 6;
}}
}}
g_fp_top=7;
{expressions._fmt_set_reg('st(2)','3.25')}
if(g_fp_stack[1]!=3.25 || {expressions._fmt_reg('st(2)')}!=3.25) return 4;
return 0;}}
'''
    source=tmp_path/'expressions.c';source.write_text(code)
    exe=tmp_path/('expressions.exe' if os.name=='nt' else 'expressions')
    args=[compiler,'/nologo','/std:c11','/WX',str(source),f'/Fe:{exe}'] if os.name=='nt' else [compiler,'-std=c11','-Werror',str(source),'-o',str(exe)]
    build=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert build.returncode==0,build.stdout+build.stderr
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert run.returncode==0,run.stdout+run.stderr


def test_emitted_memory_accesses_use_production_pointer_and_width_macros(tmp_path):
    header=ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h'
    if not header.is_file():
        pytest.skip('Pinned memory ABI consumer unavailable')
    wanted={'XBOX_PTR','MEM8','MEM16','MEM32','MEM64','SMEM8','SMEM16','SMEM32','SMEM64','MEMF','MEMD'}
    definitions={}
    for line in header.read_text(encoding='utf-8').splitlines():
        match=re.match(r'#define (\w+)\(',line)
        if match and match[1] in wanted:definitions[match[1]]=line
    assert definitions.keys()==wanted
    operations=[]
    for width,offset in ((1,0),(2,8),(4,16)):
        op=Operand('mem',mem_disp=-2147483648+offset,mem_size=width)
        operations.append(f'''{expressions._fmt_mem_write(op,'value')}
{{ uint{width*8}_t expected=(uint{width*8}_t)0xfedcba98u;
if(memcmp(bytes+{offset}, &expected, {width})) return 1;
if({expressions._fmt_mem_read(op)} != expected) return 2;
int{width*8}_t signed_value; memcpy(&signed_value,bytes+{offset},{width});
if({expressions._smem_accessor(width)}({expressions._fmt_mem(op)})!=signed_value) return 3;
}}''')
    for width,offset,value in ((4,32,'-1.25f'),(8,40,'3.125')):
        op=Operand('mem',mem_disp=-2147483648+offset,mem_size=width)
        operations.append(f'{expressions._fmt_fpu_operand(op)}={value};\n'
                          f'if({expressions._fmt_fpu_operand(op)}!={value}) return 4;')
    for value in (-1,-2147483648,-46137012):
        op=Operand('mem',mem_disp=value)
        operations.append(f'if(XBOX_PTR({expressions._fmt_mem(op)})!=XBOX_PTR({value})) return 5;')
    code='''#include <stdint.h>
#include <string.h>
static uintptr_t g_xbox_mem_offset;
'''+ '\n'.join(definitions.values())+'''
int main(void) {
uint64_t aligned[8]={0}; unsigned char *bytes=(unsigned char*)aligned;
uint32_t value=0xfedcba98u;
g_xbox_mem_offset=(uintptr_t)bytes-(uintptr_t)0x80000000u;
'''+ '\n'.join(operations)+ '\nreturn 0;}\n'
    compiler,env=native_compiler(tmp_path)
    source=tmp_path/'memory.c';source.write_text(code)
    exe=tmp_path/('memory.exe' if os.name=='nt' else 'memory')
    args=[compiler,'/nologo','/std:c11','/WX',str(source),f'/Fe:{exe}'] if os.name=='nt' else [compiler,'-std=c11','-Werror',str(source),'-o',str(exe)]
    build=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert build.returncode==0,build.stdout+build.stderr
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert run.returncode==0,run.stdout+run.stderr
