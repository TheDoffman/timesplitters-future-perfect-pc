"""Minimum feature contracts, including older desktop core versions."""
from pathlib import Path
import os
import subprocess
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]


def test_required_opengl_capabilities(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('capabilities.exe' if os.name=='nt' else 'capabilities')
    source=ROOT/'tests/graphics/opengl_capabilities.c'
    if os.name=='nt':
        command=[compiler,'/nologo','/std:c11',f'/I{ROOT}/src/graphics',str(source),f'/Fe:{exe}',f'/Fo:{tmp_path}/capabilities.obj']
    else:
        command=[compiler,'-std=c11','-O2',f'-I{ROOT}/src/graphics',str(source),'-o',str(exe)]
    built=subprocess.run(command,env=env,cwd=tmp_path,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'minimum version, complete S3TC and mirror-once' in result.stdout
