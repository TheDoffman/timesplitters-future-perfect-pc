"""Failure injection around the production native drawable replacement."""
from pathlib import Path
import os
import subprocess
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]

def test_window_transaction(tmp_path):
    compiler,env=native_compiler(tmp_path)
    source=ROOT/'tests/graphics/opengl_window.c'
    includes=[ROOT/'src/native',ROOT/'external/ts3FPrecomp/third_party/SDL/include']
    exe=tmp_path/('window.exe' if os.name=='nt' else 'window')
    if os.name=='nt':
        command=[compiler,'/nologo','/std:c11',*[f'/I{p}' for p in includes],str(source),f'/Fe:{exe}',f'/Fo:{tmp_path}/window.obj']
    else:
        command=[compiler,'-std=c11',*[f'-I{p}' for p in includes],str(source),'-o',str(exe)]
    compiled=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True)
    assert compiled.returncode==0,compiled.stdout+compiled.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'staging, preservation and rollback' in result.stdout
    assert 'primary-window failure reaches shared startup without selecting a renderer' in result.stdout
