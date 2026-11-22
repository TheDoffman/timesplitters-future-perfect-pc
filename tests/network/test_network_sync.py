"""Compile the shared release telemetry without game assets or network access."""
import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
def test_network_sync_measurements(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('sync.exe' if os.name=='nt' else 'sync')
    src=ROOT/'tests/network/network_sync.c'
    args=([compiler,'/nologo','/std:c11','/W4','/WX','/I'+str(ROOT/'src'),str(src),'/Fe:'+str(exe)]
          if os.name=='nt' else [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(src),'-o',str(exe)])
    built=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    assert built.returncode==0,built.stdout+built.stderr
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert run.returncode==0,run.stdout+run.stderr
