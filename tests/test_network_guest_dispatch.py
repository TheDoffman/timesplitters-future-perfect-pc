"""Compile and execute each side of the shared guest networking boundary."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('source_name,native_fallback',[
    ('network_guest_dispatch.c',False),('network_guest_dispatch.c',True),
    ('network_bots.c',False)])
def test_guest_dispatch_and_fallback(tmp_path,source_name,native_fallback):
    requested=os.environ.get('CC')
    env=None
    if not requested and os.name=='nt':
        executable_compiler,env=native_compiler(tmp_path)
        compiler=[executable_compiler]
    else:
        compiler=shlex.split(requested) if requested else [shutil.which('cc') or shutil.which('cl') or '']
    if not compiler[0]:pytest.skip('A C compiler is required')
    msvc=Path(compiler[0]).name.lower() in ('cl','cl.exe')
    executable=tmp_path/('guest-dispatch.exe' if os.name=='nt' else 'guest-dispatch')
    source=ROOT/'tests'/source_name
    if msvc:
        command=[*compiler,'/nologo','/W4','/Fe:'+str(executable),'/Fo:'+str(tmp_path/'guest.obj')]
        if native_fallback:command.append('/DTSFP_TEST_NATIVE_FALLBACK')
        command.append(str(source))
    else:
        command=[*compiler,'-std=c11','-Wall','-Wextra']
        if native_fallback:command.append('-DTSFP_TEST_NATIVE_FALLBACK')
        command.extend([str(source),'-o',str(executable)])
    subprocess.run(command,cwd=tmp_path,env=env,check=True,capture_output=True,text=True)
    result=subprocess.run([str(executable)],check=True,capture_output=True,text=True)
    assert 'PASS:' in result.stdout
