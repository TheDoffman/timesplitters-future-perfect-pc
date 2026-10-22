"""Compile and execute each side of the shared guest networking boundary."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('source_name,native_fallback',[
    ('network_guest_dispatch.c',False),('network_guest_dispatch.c',True),
    ('network_bots.c',False)])
def test_guest_dispatch_and_fallback(tmp_path,source_name,native_fallback):
    compiler, environment=native_compiler(tmp_path)
    msvc=Path(compiler).name.lower() in ('cl','cl.exe')
    executable=tmp_path/('guest-dispatch.exe' if os.name=='nt' else 'guest-dispatch')
    source=ROOT/'tests'/source_name
    if msvc:
        command=[compiler,'/nologo','/W4','/Fe:'+str(executable),'/Fo:'+str(tmp_path/'guest.obj')]
        if native_fallback:command.append('/DTSFP_TEST_NATIVE_FALLBACK')
        command.append(str(source))
    else:
        command=[compiler,'-std=c11','-Wall','-Wextra']
        if native_fallback:command.append('-DTSFP_TEST_NATIVE_FALLBACK')
        command.extend([str(source),'-o',str(executable)])
    subprocess.run(command,cwd=tmp_path,env=environment,check=True,capture_output=True,text=True)
    result=subprocess.run([str(executable)],env=environment,check=True,capture_output=True,text=True)
    assert 'PASS:' in result.stdout
