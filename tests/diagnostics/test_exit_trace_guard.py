"""Exit diagnostics must preserve normal/failure status even when tracing faults."""
from pathlib import Path
import os
import subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
pytestmark=pytest.mark.skipif(os.name!='nt',reason='Windows exit tracing uses SEH and Win32 hooks')


def test_exit_trace_faults_preserve_requested_status(tmp_path):
    compiler,env=native_compiler(tmp_path)
    runtime=ROOT/'external/ts3FPrecomp'
    output=tmp_path/'exit-trace.exe'
    command=[compiler,'/nologo','/std:c11',f'/I{runtime}/src',f'/I{runtime}/src/kernel',
             f'/I{runtime}/include',str(ROOT/'tests/diagnostics/exit_trace_guard.c'),
             f'/Fe:{output}',f'/Fo:{tmp_path}/exit-trace.obj','/link','dbghelp.lib','psapi.lib']
    result=subprocess.run(command,env=env,cwd=tmp_path,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    for mode in range(4):
        for code in (0,2):
            result=subprocess.run([str(output),str(mode),str(code)],capture_output=True,text=True,timeout=10)
            assert result.returncode==code,result.stdout+result.stderr
            assert ('trace unavailable (access violation)' in result.stderr)==(mode!=0 and code!=0)
            assert f'ExitProcess(0x{code:08X})' in result.stderr

    result=subprocess.run([str(output),'4','0'],capture_output=True,text=True,timeout=10)
    assert result.returncode & 0xffffffff == 0xc0000005
    assert 'trace unavailable' not in result.stderr
