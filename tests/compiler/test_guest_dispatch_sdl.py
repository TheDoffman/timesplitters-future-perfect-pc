"""The SDL owner must stay live during guest polling, not only kernel waits."""
from pathlib import Path
import os
import subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(os.name=='nt',reason='Native POSIX entry point; Windows has its own host loop')
def test_guest_polling_and_host_dispatch(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/'guest-dispatch'
    result=subprocess.run([compiler,'-std=c11','-O2','-pthread','-I'+str(ROOT/'src/native'),
        '-I'+str(ROOT/'external/ts3FPrecomp/third_party/SDL/include'),
        str(ROOT/'tests/compiler/guest_dispatch_sdl.c'),'-o',str(exe)],
        env=env,cwd=tmp_path,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'continuous owner dispatch during guest polling, TLS ownership' in result.stdout
