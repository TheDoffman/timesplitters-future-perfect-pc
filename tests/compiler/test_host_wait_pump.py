"""Service native desktop work without changing ordinary worker wait semantics."""
from pathlib import Path
import os
import subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(os.name=='nt',reason='Windows uses the OS wait implementation')
def test_native_owner_wait_dispatch(tmp_path):
    compiler,env=native_compiler(tmp_path)
    runtime=ROOT/'external/ts3FPrecomp'
    providers=[ROOT/'src/runtime'/name for name in
        ('shared_views.c','owned_arena.c','mmio_views.c','guest_arena_service.c')]
    exe=tmp_path/'wait-pump'
    command=[compiler,'-std=c11','-O2','-fsanitize=undefined','-fno-sanitize-recover=all',
        '-I'+str(runtime/'src'),'-I'+str(ROOT/'src/runtime'),
        str(ROOT/'tests/compiler/host_wait_pump.c'),str(runtime/'src/platform/win32_compat.c'),
        *map(str,providers),str(ROOT/'src/native/shared_host.c'),'-pthread','-lm','-o',str(exe)]
    result=subprocess.run(command,env=env,cwd=tmp_path,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'owner dispatch, unlocked callback, recursion guard, deadlines' in result.stdout
