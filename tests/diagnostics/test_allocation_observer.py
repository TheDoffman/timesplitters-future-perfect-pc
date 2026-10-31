import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
def test_allocation_observer(tmp_path):
    compiler,env=native_compiler(tmp_path)
    sources=[str(ROOT/'tests/diagnostics/allocation_observer.c'),str(ROOT/'src/runtime/guest_allocation.c')]
    include=ROOT/'src/runtime';exe=tmp_path/('observer.exe' if os.name=='nt' else 'observer')
    args=([compiler,'/nologo','/std:c11','/I'+str(include),*sources,'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c11','-Wall','-Wextra','-I'+str(include),*sources,'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
