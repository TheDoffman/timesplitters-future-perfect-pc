"""Run the production restart adapter across two real processes."""
import subprocess
import sys
import time
from pathlib import Path
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
def test_restart_waits_for_shutdown_and_preserves_environment(tmp_path):
    compiler,environment=native_compiler(tmp_path)
    exe=tmp_path/'restart test.exe'
    source=ROOT/'tests/graphics/game_restart.c'
    command=([compiler,'/nologo','/std:c11','/W3','/I'+str(ROOT/'src'),str(source),
        '/Fe:'+str(exe),'/link','user32.lib'] if sys.platform=='win32' else
        [compiler,'-std=gnu11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(source),'-o',str(exe)])
    result=subprocess.run(command,cwd=tmp_path,env=environment,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    receipt=tmp_path/'restart result.txt'
    result=subprocess.run([str(exe),str(receipt)],cwd=tmp_path,env=environment,timeout=15)
    assert result.returncode==0
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        if receipt.exists() and receipt.read_text()=='CHILD':return
        time.sleep(.05)
    pytest.fail('Replacement did not finish after parent shutdown')
