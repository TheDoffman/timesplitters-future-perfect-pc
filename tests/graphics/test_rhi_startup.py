"""Run the shared startup transaction with resource-owning backend spies."""
from pathlib import Path
import os
import subprocess
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]

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
