"""Compile and execute each side of the shared guest networking boundary."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('source_name,native_fallback',[
    ('network_guest_dispatch.c',False),('network_guest_dispatch.c',True),
    ('network_bots.c',False),('network_bot_motion.c',False)])
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
        command.extend([str(source),'-lm','-o',str(executable)])
    subprocess.run(command,cwd=tmp_path,env=environment,check=True,capture_output=True,text=True)
    result=subprocess.run([str(executable)],env=environment,check=True,capture_output=True,text=True)
    assert 'PASS:' in result.stdout


@pytest.mark.parametrize('mutation,diagnostic',[
    ('no_interpolation','changes>=55'),('mixed_crossfade','!memcmp(out.blend_channels')])
def test_bot_motion_mutations_fail(tmp_path,mutation,diagnostic):
    """Reject packet holds and the invalid crossfade combination seen in a draft."""
    import shutil
    (tmp_path/'src').mkdir()
    (tmp_path/'tests').mkdir()
    shutil.copyfile(ROOT/'src/pc_network_bots_state.h',tmp_path/'src/pc_network_bots_state.h')
    header=(ROOT/'src/pc_network_bot_motion.h').read_text()
    if mutation=='no_interpolation':
        needle='motion->duration=continuous?.05:0.;'
        replacement='motion->duration=0.;'
    else:
        needle='#undef BOT_MOTION_VECTOR'
        replacement='BOT_MOTION_VECTOR(blend_channels,3);\n'+needle
    assert needle in header
    (tmp_path/'src/pc_network_bot_motion.h').write_text(header.replace(needle,replacement))
    shutil.copyfile(ROOT/'tests/network_bot_motion.c',tmp_path/'tests/network_bot_motion.c')
    compiler,environment=native_compiler(tmp_path)
    executable=tmp_path/('mutation.exe' if os.name=='nt' else 'mutation')
    source=tmp_path/'tests/network_bot_motion.c'
    if Path(compiler).name.lower() in ('cl','cl.exe'):
        command=[compiler,'/nologo','/Fe:'+str(executable),str(source)]
    else:
        command=[compiler,'-std=c11',str(source),'-lm','-o',str(executable)]
    subprocess.run(command,cwd=tmp_path,env=environment,check=True,capture_output=True,text=True)
    result=subprocess.run([str(executable)],env=environment,capture_output=True,text=True)
    assert result.returncode != 0 and diagnostic in result.stderr
