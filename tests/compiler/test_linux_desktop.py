"""Compile the real Linux GTK adapter with the pinned SDL/public shared headers."""
import os
from pathlib import Path
import subprocess
import sys
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
@pytest.mark.skipif(sys.platform!='linux',reason='Linux GTK desktop adapter')
def test_linux_desktop_adapter_compiles(tmp_path):
    compiler,env=native_compiler(tmp_path)
    flags=subprocess.check_output(['pkg-config','--cflags','gtk+-3.0'],text=True).split()
    command=[compiler,'-std=c11','-fsyntax-only','-Werror=implicit-function-declaration',*flags,
        '-I'+str(ROOT/'external/ts3FPrecomp/third_party/SDL/include'),
        str(ROOT/'src/native/settings_linux.c')]
    result=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.skipif(sys.platform!='linux',reason='POSIX native movie build guards')
@pytest.mark.parametrize('rhi',[False,True],ids=['legacy-stub','shared-vulkan'])
def test_posix_movie_player_build_guards(tmp_path,rhi):
    compiler,env=native_compiler(tmp_path);runtime=ROOT/'external/ts3FPrecomp/src'
    command=[compiler,'-std=c11','-c','-Werror=implicit-function-declaration','-I'+str(runtime),
        '-I'+str(runtime/'config')]
    if rhi:command += ['-DTSFP_PC_CONTROLS=1','-DTSFP_RHI_RENDERER=1','-I'+str(runtime/'video'),'-I'+str(runtime/'d3d')]
    obj=tmp_path/'movie.o';command += [str(runtime/'hle/hle_xmv_play.c'),'-o',str(obj)]
    result=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    undefined=subprocess.check_output(['nm','-u',str(obj)],text=True)
    assert ('xmv_decoder_create' in undefined)==rhi
