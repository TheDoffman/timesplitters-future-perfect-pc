"""Actual game-side custom voice engine against deterministic device responses."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]

@pytest.fixture(scope='module')
def player(tmp_path_factory):
    directory=tmp_path_factory.mktemp('mod-audio-player');compiler,environment=native_compiler(directory)
    exe=directory/('player.exe' if os.name=='nt' else 'player')
    includes=[ROOT/'src',ROOT/'external/ts3FPrecomp/src']
    source=ROOT/'tests/media/mod_audio_player.c'
    command=([compiler,'/nologo','/std:c11','/W4','/WX',*['/I'+str(p) for p in includes],str(source),'/Fe:'+str(exe),'/Fo:'+str(directory/'player.obj')] if os.name=='nt' else
             [compiler,'-std=c11','-Wall','-Wextra','-Werror',*['-I'+str(p) for p in includes],str(source),'-o',str(exe)])
    result=subprocess.run(command,cwd=directory,env=environment,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    return exe

@pytest.mark.parametrize('scenario',['basic','bounds','refusal','loop','pause','one-shot','stale','invalidate','budget'])
def test_actual_custom_voice_player(player,scenario):
    result=subprocess.run([str(player),scenario],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
