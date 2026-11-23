"""Swap-control refusal must not prevent a usable driver from presenting."""
from pathlib import Path
import os
import subprocess
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]


def test_optional_swap_control(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('present.exe' if os.name=='nt' else 'present')
    source=ROOT/'tests/graphics/opengl_present.c'
    if os.name=='nt':
        command=[compiler,'/nologo','/std:c11',f'/I{ROOT}/src/graphics',str(source),f'/Fe:{exe}',f'/Fo:{tmp_path}/present.obj']
    else:
        command=[compiler,'-std=c11','-O2',f'-I{ROOT}/src/graphics',str(source),'-o',str(exe)]
    result=subprocess.run(command,env=env,cwd=tmp_path,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'preference preservation' in result.stdout
    assert result.stderr.count('swap interval')==2
    assert result.stderr.count('presentation failed')==2
