"""Original data-transfer lowering fixtures and production consumer contracts."""
import os
from pathlib import Path
import re
import subprocess
import sys
import shutil
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import translation_moves as moves
from translation_decode import Disassembler, Operand
from native_compiler import native_compiler


def emit(raw, context=None):
    data=bytes.fromhex(raw)
    instructions=Disassembler().disassemble_function(data,0x1000,0x1000+len(data))
    assert len(instructions)==1
    ins=instructions[0]
    return '\n'.join(getattr(moves,ins.mnemonic)(context or SimpleNamespace(trace_exit_name=None),ins,ins.operands))


def test_bad_extension_and_lea_forms_fail_explicitly():
    context=SimpleNamespace()
    with pytest.raises(ValueError):moves.movsx(context,None,[Operand('reg',reg='eax'),Operand('reg',reg='ebx')])
    with pytest.raises(ValueError):moves.lea(context,None,[Operand('reg',reg='eax'),Operand('imm',imm=1)])
    with pytest.raises(ValueError):moves.push(context,None,[])


def test_trace_strings_are_escaped():
    code=emit('58',SimpleNamespace(trace_exit_name='a"b'))
    assert 'RECOMP_TRACE_ESP("a\\042b", "pop eax")' in code


def _production_macros(names):
    header=ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h'
    if not header.is_file():pytest.skip('Native runtime ABI unavailable')
    lines=iter(header.read_text(encoding='utf-8').splitlines())
    result={}
    for line in lines:
        match=re.match(r'#define (\w+)\(',line)
        if match and match[1] in names:
            value=line
            while line.endswith('\\'):
                line=next(lines);value+='\n'+line
            result[match[1]]=value
    assert result.keys()==names
    return '\n'.join(result.values())


def test_data_moves_execute_with_runtime_macros(tmp_path):
    macros=_production_macros({'XBOX_PTR','MEM8','MEM16','MEM32','LO8','HI8','LO16',
        'SET_LO8','SET_HI8','SET_LO16','SX8','SX16','ZX8','ZX16','PUSH32','POP32'})
    trace='caller "quoted"\\\n\U0001f642'
    source='''#include <stdint.h>
#include <string.h>
static uint64_t memory[0x6000];
static uintptr_t g_xbox_mem_offset;
static uint32_t g_ebp,g_seh_ebp;
static uint32_t loads,stores,after,last_store,byte_loads;
'''+macros+'''
#define XBOX_FS_BASE 0x70000000u
#define RECOMP_LOAD_GLOBAL_U32(site,addr,value) (++loads,(value)+1)
#define RECOMP_LOAD_U8_AT(site,value) (++byte_loads,(value))
#define RECOMP_STORE_GLOBAL_U32(site,addr,value) (++stores,(value)+2)
#define RECOMP_AFTER_STORE32(site,addr) (++after,last_store=MEM32(addr))
#define RECOMP_PUSH_IMM_U32_AT(site,value) (value)
#define RECOMP_TRACE_ESP(name,tag) do { if(memcmp(name,expected_trace,sizeof(expected_trace)) || strcmp(tag,"pop eax")) return 15; } while(0)
int main(void) {
uint32_t eax=0,ebx=0,ecx=0,esp=0,ebp=0;
g_xbox_mem_offset=(uintptr_t)memory;
MEM32(0x100)=40;
'''+emit('a100010000')+'''
if(eax!=41 || loads!=1) return 1;
'''+emit('a300020000')+'''
if(MEM32(0x200)!=43 || stores!=1 || after!=1 || last_store!=43) return 2;
esp=0x300;
'''+emit('8bec')+'''
if(ebp!=esp || g_ebp!=esp || g_seh_ebp!=esp) return 3;
for(uint32_t n=0;n<256;n++) {
eax=0x12340000u|(n<<8);
'''+emit('0fbecc')+'''
if(ecx!=(uint32_t)(int32_t)(int8_t)n) return 4;
}
eax=0x12345680u;
'''+emit('660fbec0')+'''
if(eax!=0x1234ff80u) return 5;
ecx=0x8888ffffu;
'''+emit('0fb7c1')+'''
if(eax!=65535u) return 6;
MEM16(0x280)=0x8000u;
'''+emit('0fbf0580020000')+'''
if(eax!=0xffff8000u) return 14;
ebx=0x100;
'''+emit('648d43fc')+'''
if(eax!=0xfc) return 7;
esp=0x300;
'''+emit('54')+'''
if(esp!=0x2fc || MEM32(esp)!=0x300) return 8;
MEM32(esp)=0x500;
'''+emit('5c')+'''
if(esp!=0x500) return 9;
esp=0x300;MEM32(esp)=0x89abcdefu;
'''+emit('8f442404')+'''
if(esp!=0x304 || MEM32(0x308)!=0x89abcdefu) return 10;
esp=0x300;
'''+emit('6654')+'''
if(esp!=0x2fe || MEM16(esp)!=0x300) return 11;
esp=0x1fffe;MEM16(esp)=0xaaaau;
'''+emit('665c')+'''
if(esp!=0x2aaaau) return 12;
esp=0x300;
'''+emit('66683412')+'''
if(esp!=0x2fe || MEM16(esp)!=0x1234) return 13;
'''+ 'const unsigned char expected_trace[]={'+','.join(str(b) for b in trace.encode('utf-8')+b'\0')+'};\n'+emit('58',SimpleNamespace(trace_exit_name=trace))+'''
return 0;}
'''
    compiler,env=native_compiler(tmp_path)
    path=tmp_path/'moves.c';path.write_text(source)
    exe=tmp_path/('moves.exe' if os.name=='nt' else 'moves')
    args=[compiler,'/nologo','/std:c11','/WX',str(path),f'/Fe:{exe}'] if os.name=='nt' else [compiler,'-std=c11','-Werror',str(path),'-o',str(exe)]
    build=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert build.returncode==0,build.stdout+build.stderr
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert run.returncode==0,run.stdout+run.stderr


def test_copied_lowerers_do_not_require_toolkit(tmp_path):
    for name in ('translation_moves.py','translation_operands.py'):
        shutil.copyfile(ROOT/'tools'/name,tmp_path/name)
    code='''import sys
sys.path.insert(0,sys.argv[1])
from types import SimpleNamespace as S
import translation_moves as moves
result=moves.mov(S(),S(address=0x1000),[S(type='reg',reg='eax'),S(type='imm',imm=123)])
assert result==['eax = 0x7B;']
assert not any(name.startswith('tools.recomp') for name in sys.modules)
'''
    result=subprocess.run([sys.executable,'-I','-c',code,str(tmp_path)],cwd=tmp_path,
                          capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr


def test_installed_lifter_uses_project_moves_and_existing_consumers():
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/lifter.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    code='''from tools.recomp.lifter import Lifter
import translation_moves as moves
for name in ('mov','movzx','movsx','lea','push','pop'):
 assert getattr(Lifter,'_lift_'+name) is getattr(moves,name)
import pytest
raise SystemExit(pytest.main(['tools/recomp','-q','--tb=short']))
'''
    result=subprocess.run([sys.executable,'-c',code],cwd=runtime,
                          env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),
                          capture_output=True,text=True,timeout=300)
    assert result.returncode==0,result.stdout+result.stderr
