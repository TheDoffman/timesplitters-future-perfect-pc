"""Compile and execute each side of the shared guest networking boundary."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('native_fallback',[False,True])
def test_guest_dispatch_and_fallback(tmp_path,native_fallback):
    requested=os.environ.get('CC')
    compiler=shlex.split(requested) if requested else [shutil.which('cc') or shutil.which('cl') or '']
    if not compiler[0]:pytest.skip('A C compiler is required')
    msvc=Path(compiler[0]).name.lower() in ('cl','cl.exe')
    executable=tmp_path/('guest-dispatch.exe' if os.name=='nt' else 'guest-dispatch')
    source=ROOT/'tests/network_guest_dispatch.c'
    if msvc:
        command=[*compiler,'/nologo','/W4','/Fe:'+str(executable),'/Fo:'+str(tmp_path/'guest.obj')]
        if native_fallback:command.append('/DTSFP_TEST_NATIVE_FALLBACK')
        command.append(str(source))
    else:
        command=[*compiler,'-std=c11','-Wall','-Wextra']
        if native_fallback:command.append('-DTSFP_TEST_NATIVE_FALLBACK')
        command.extend([str(source),'-o',str(executable)])
    subprocess.run(command,cwd=tmp_path,check=True,capture_output=True,text=True)
    result=subprocess.run([str(executable)],check=True,capture_output=True,text=True)
    assert 'PASS:' in result.stdout
