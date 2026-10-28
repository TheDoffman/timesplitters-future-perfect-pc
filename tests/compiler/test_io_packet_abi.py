from pathlib import Path
import os
import re
import subprocess
import sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile

def function(source,signature):
    start=source.index(signature+'\n{');opening=source.index('{',start);depth=1
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]',source[opening+1:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:opening+1+token.end()]
    raise AssertionError('Unterminated bridge function')

@pytest.mark.parametrize('mutation',[None,'cleanup','argument'])
def test_actual_next_packet_bridge_preserves_caller_stack(tmp_path,mutation):
    bridge=(ROOT/'external/ts3FPrecomp/src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    adapter=function(bridge,'static void bridge_IoStartNextPacket(void)')
    cleanup=function(bridge,'static int stdcall_args_for_ordinal(ULONG ordinal)')
    if mutation=='cleanup':
        old='case  81: return  4;';assert old in cleanup;cleanup=cleanup.replace(old,'case  81: return  8;')
    if mutation=='argument':
        old='(BOOLEAN)0);';assert old in adapter;adapter=adapter.replace(old,'(BOOLEAN)STACK_ARG(1));')
    source=(HERE/'io_packet_abi_contract.c').read_text(encoding='utf-8')
    source=source.replace('/*ACTUAL_BRIDGE*/',adapter).replace('/*ACTUAL_CLEANUP*/',cleanup)
    fixture=tmp_path/'packet.c';fixture.write_text(source,encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,fixture,extra=flags)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken guest stack contract passed'
    else:
        assert run.returncode==0,run.stdout+run.stderr
        assert 'PASS:' in run.stdout
