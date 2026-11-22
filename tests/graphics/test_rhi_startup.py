"""Run the shared startup transaction with resource-owning backend spies."""
from pathlib import Path
import os
import subprocess
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('windows,override,expected',[(True,None,1),(False,None,0),
    (True,0,0),(True,1,1),(False,0,0),(False,1,1)])
def test_platform_acceptance_is_explicit_and_native_hosts_stay_disabled(tmp_path,windows,override,expected):
    compiler,env=native_compiler(tmp_path)
    source=tmp_path/'acceptance.c'
    source.write_text('#include "graphics/rhi_platform_acceptance.h"\n'
                      f'int main(void){{return TSFP_OPENGL_RECOVERY_ACCEPTED!={expected};}}\n')
    exe=tmp_path/('acceptance.exe' if os.name=='nt' else 'acceptance')
    flags=(['/U_WIN32'] if not windows else ['/D_WIN32']) if os.name=='nt' else (['-D_WIN32'] if windows else ['-U_WIN32'])
    if override is not None:flags.append(('/D' if os.name=='nt' else '-D')+f'TSFP_OPENGL_RECOVERY_ACCEPTED={override}')
    command=([compiler,'/nologo',f'/I{ROOT/"src"}',*flags,str(source),f'/Fe:{exe}',f'/Fo:{tmp_path}/acceptance.obj'] if os.name=='nt'
             else [compiler,f'-I{ROOT/"src"}',*flags,str(source),'-o',str(exe)])
    result=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    assert subprocess.run([str(exe)],timeout=10).returncode==0

def test_startup_transaction(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('startup.exe' if os.name=='nt' else 'startup')
    includes=[ROOT/'src',ROOT/'external/ts3FPrecomp/src/d3d']
    source=ROOT/'tests/graphics/rhi_startup.c'
    if os.name=='nt':
        command=[compiler,'/nologo','/std:c11',*[f'/I{p}' for p in includes],str(source),f'/Fe:{exe}',f'/Fo:{tmp_path}/startup.obj']
    else:
        command=[compiler,'-std=c11',*[f'-I{p}' for p in includes],str(source),'-o',str(exe)]
    compiled=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True)
    assert compiled.returncode==0,compiled.stdout+compiled.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'Vulkan and OpenGL initialization failed' in result.stderr
    assert 'startup recovery selected opengl' in result.stderr
