"""Numeric emission must work without the former scalar runtime helper layer."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest
from test_translation_product import _compile

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT/'external/ts3FPrecomp'
HEADER = RUNTIME/'templates/runtime/recomp_types.h'


@pytest.mark.parametrize('variant',['production','lost_sign','double_memory_read'])
def test_extension_reads_once_preserves_registers_and_handles_every_value(tmp_path,variant):
    from types import SimpleNamespace
    import translation_moves as moves
    from translation_decode import Operand
    regs = ['eax','ebx','ecx','edx','esi','edi','ebp','esp']
    words = ['ax','bx','cx','dx','si','di','bp','sp']
    bytes_ = ['al','bl','cl','dl','ah','bh','ch','dh']
    source = '''#include <stdio.h>
#define RECOMP_TLS
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
uint32_t g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_esp,g_fs_base;
static uint32_t ebp;
static uint32_t *cells[]={&eax,&ebx,&ecx,&edx,&esi,&edi,&ebp,&esp};
static uint32_t reads,read_value,expected_address,bad_address;
static uint8_t read_byte(uint32_t address){++reads;bad_address|=address!=expected_address;return (uint8_t)read_value;}
static uint16_t read_word(uint32_t address){++reads;bad_address|=address!=expected_address;return (uint16_t)read_value;}
#undef MEM8
#undef MEM16
#define MEM8(address) read_byte(address)
#define MEM16(address) read_word(address)
'''
    entries = []
    for width, names in [(8,bytes_),(16,words)]:
        for s,name in enumerate(names):
            for d,destination in enumerate(regs+words if width==8 else regs):
                for signed in (False,True):
                    function = f'extension_{len(entries)}'
                    operation = moves.movsx if signed and variant!='lost_sign' else moves.movzx
                    output = operation(SimpleNamespace(),SimpleNamespace(address=0x1000),
                        [Operand('reg',reg=destination),Operand('reg',reg=name)])
                    assert not re.search(r'\b[ZS]X(?:8|16)\(', '\n'.join(output))
                    source += f'static void {function}(void){{'+ '\n'.join(output)+'}\n'
                    entries.append((function,s%4 if width==8 else s,8 if width==8 and s>=4 else 0,
                                    width,d%8,16 if d>=8 else 32,int(signed)))
    memory_entries=[]
    for width in (1,2):
        for destination in (['eax','ax'] if width==1 else ['eax']):
            for signed in (False,True):
                for segment in (None,'fs'):
                    function=f'memory_{len(memory_entries)}'
                    operation=moves.movsx if signed and variant!='lost_sign' else moves.movzx
                    output=operation(SimpleNamespace(),SimpleNamespace(address=0x1000),
                        [Operand('reg',reg=destination),Operand('mem',mem_base='eax',mem_disp=3,mem_size=width,mem_seg=segment)])
                    source+=f'static void {function}(void){{'+'\n'.join(output)+'}\n'
                    memory_entries.append((function,width*8,16 if destination=='ax' else 32,int(signed),int(bool(segment))))
    source+='static const struct {void (*run)(void);unsigned src,shift,width,dst,target,sign;} cases[]={\n'
    source+=',\n'.join('{'+','.join(map(str,entry))+'}' for entry in entries)+'};\n'
    source+='static const struct {void (*run)(void);unsigned width,target,sign,fs;} memory_cases[]={\n'
    source+=',\n'.join('{'+','.join(map(str,entry))+'}' for entry in memory_entries)+'};\n'
    source+='''static uint32_t reference(uint32_t value,unsigned width,unsigned sign){
int64_t result=value;if(sign && value>=(1u<<(width-1)))result-=INT64_C(1)<<width;return (uint32_t)result;}
int main(void){unsigned long checked=0;
for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c){uint32_t mask=(1u<<cases[c].width)-1u;
for(uint32_t n=0;n<=mask;++n){uint32_t expected[8];
for(unsigned r=0;r<8;++r)*cells[r]=0x93A5C7E9u^((r+1u)*0x11111111u);
*cells[cases[c].src]=(*cells[cases[c].src]&~(mask<<cases[c].shift))|(n<<cases[c].shift);
for(unsigned r=0;r<8;++r)expected[r]=*cells[r];
uint32_t value=reference(n,cases[c].width,cases[c].sign);
expected[cases[c].dst]=cases[c].target==32?value:(expected[cases[c].dst]&0xffff0000u)|(value&0xffffu);
cases[c].run();for(unsigned r=0;r<8;++r)if(*cells[r]!=expected[r]){printf("register mismatch %u %u %u\\n",c,n,r);return 1;}++checked;}}
for(unsigned c=0;c<sizeof(memory_cases)/sizeof(memory_cases[0]);++c){
for(uint32_t n=0;n<(1u<<memory_cases[c].width);++n){
eax=0xa5000200u;g_fs_base=0x3100u;reads=bad_address=0;read_value=n;
expected_address=eax+3u+(memory_cases[c].fs?g_fs_base:0u);
uint32_t expected=reference(n,memory_cases[c].width,memory_cases[c].sign);
if(memory_cases[c].target==16)expected=(eax&0xffff0000u)|(expected&0xffffu);
memory_cases[c].run();if(eax!=expected||reads!=1||bad_address){puts("memory mismatch");return 2;}++checked;}}
printf("%lu extension cases passed\\n",checked);return 0;}
'''
    if variant=='double_memory_read':
        source=source.replace('#define MEM8(address) read_byte(address)',
                              '#define MEM8(address) (read_byte(address),read_byte(address))')
    path=tmp_path/'extensions.c';path.write_text(source,encoding='utf-8')
    include=RUNTIME/'templates/runtime'
    exe=_compile(tmp_path,path,extra=('/I'+str(include),) if os.name=='nt' else ('-I',str(include),'-lm'))
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
    if variant=='production':
        assert run.returncode==0 and '8718336 extension cases passed' in run.stdout,run.stdout+run.stderr
    else:
        expected=(1,'register mismatch') if variant=='lost_sign' else (2,'memory mismatch')
        assert run.returncode==expected[0] and expected[1] in run.stdout,run.stdout+run.stderr


def test_numeric_helpers_are_retired():
    text = HEADER.read_text(encoding='utf-8')
    assert not re.search(r'\b(?:CMP_(?:EQ|NE|B|AE|BE|A|L|GE|LE|G)|TEST_(?:Z|NZ|S)|'
                         r'RECOMP_(?:FCMP(?:_CC)?|PARITY8|FLAG_WIDTH|SIGNED)|'
                         r'recomp_(?:fxam|frndint|fist|parity8)|ADD32_CF|SUB32_CF|'
                         r'RO[LR](?:8|16|32)|RC_ROT|[SZ]X(?:8|16)|BSWAP(?:16|32))\b', text)


@pytest.mark.parametrize('variant',['production','isolated','wrong_direction','double_argument'])
def test_direction_displacement(tmp_path,variant):
    header = HEADER.read_text(encoding='utf-8')
    if variant == 'isolated':
        header = '#include <stdint.h>\n#define RECOMP_TLS\n' + next(
            line for line in header.splitlines() if line.startswith('#define RECOMP_DF_STEP('))
    mutations = {
        'wrong_direction': ('(g_df != 0)', '(g_df == 0)'),
        'double_argument': ('(0u - (uint32_t)(bytes))', '(0u - (uint32_t)(bytes) + (uint32_t)(bytes))'),
    }
    if variant in mutations:
        old,new = mutations[variant]
        assert old in header
        header = header.replace(old,new)
    (tmp_path/'under_test.h').write_text(header,encoding='utf-8')
    source = tmp_path/'direction.c'
    source.write_bytes((ROOT/'tests/compiler/scalar_support.c').read_bytes())
    exe = _compile(tmp_path,source,extra=() if os.name=='nt' else ('-lm',))
    run = subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if variant in mutations:
        assert run.returncode==1 and 'direction mismatch' in run.stdout,run.stdout+run.stderr
    else:
        assert run.returncode==0 and '262172 direction cases passed' in run.stdout,run.stdout+run.stderr


@pytest.mark.parametrize('variant',['production','host_rounding','lost_zero_sign','lost_indefinite'])
def test_preserved_rounding_oracle_executes_emitted_instructions(tmp_path,variant):
    # This source factory retains the previous regression's numeric assertions;
    # its operations now come from the production lifter instead of dead helpers.
    script = ('import json; from tools.recomp.test_lifter_frndint import rounding_source; '
              'print(json.dumps(rounding_source()))')
    result = subprocess.run([sys.executable,'-c',script],cwd=RUNTIME,
        env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    text = json.loads(result.stdout)
    mutations = {
        'host_rounding': ('unsigned _fp_mode = ((g_fp_control_word >> 10) & 3u);', 'unsigned _fp_mode = 0u;'),
        'lost_zero_sign': ('copysign(0.0, _x87_value)', '0.0'),
        'lost_indefinite': ('UINT64_C(0x80000000)', 'UINT64_C(0)'),
    }
    if variant in mutations:
        old,new = mutations[variant]
        assert old in text
        text = text.replace(old,new)
    source = tmp_path/'rounding.c'
    source.write_text(text,encoding='utf-8')
    include = RUNTIME/'templates/runtime'
    exe = _compile(tmp_path,source,extra=('/I'+str(include),) if os.name=='nt' else ('-I',str(include),'-lm'))
    run = subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if variant in mutations:
        failures = {n if os.name=='nt' else n%256 for n,line in enumerate(text.splitlines(),1)
                    if 'return __LINE__;' in line}
        assert run.returncode!=0 and run.returncode in failures,run.stdout+run.stderr
    else:
        assert run.returncode==0,run.stdout+run.stderr
