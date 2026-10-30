import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
def test_bounded_guest_capture(tmp_path):
    compiler,env=native_compiler(tmp_path)
    source=ROOT/'tests/diagnostics/guest_capture.c'
    include=ROOT/'src/diagnostics'
    exe=tmp_path/('capture.exe' if os.name=='nt' else 'capture')
    args=([compiler,'/nologo','/std:c11','/D_CRT_SECURE_NO_WARNINGS','/I'+str(include),str(source),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c11','-Wall','-Wextra','-I'+str(include),str(source),'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
