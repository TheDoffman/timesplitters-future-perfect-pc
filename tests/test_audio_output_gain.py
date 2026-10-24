"""Verify real host output code without speakers, personal settings or game data."""
import os
from pathlib import Path
import re
import shutil
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]
RUNTIME=ROOT/'external/ts3FPrecomp/src'

@pytest.fixture(scope='module',params=['xaudio','sdl'])
def output_exe(request,tmp_path_factory):
    folder=tmp_path_factory.mktemp('audio-gain-'+request.param)
    compiler,env=native_compiler(folder)
    source=ROOT/'tests'/('audio_output_gain.cpp' if request.param=='xaudio' else 'audio_sdl_gain.c')
    if request.param=='xaudio':
        code=(RUNTIME/'hle/audio_output_xaudio2.cpp').read_text()
        functions=[]
        for signature in ('extern "C" void recomp_audio_output_initialize(void)',
                          'static float gainOf(int32_t volume_hundredth_db)'):
            match=re.search(re.escape(signature)+r'\n\{.*?\n\}',code,re.S)
            assert match,signature
            functions.append(match.group())
        (folder/'xaudio_gain.inc').write_text('\n'.join(functions))
        if os.name!='nt':
            compiler=shutil.which(env.get('CXX','c++'))
            assert compiler,'C++ compiler required for actual XAudio initialization fixture'
    else:
        (folder/'SDL3').mkdir()
        (folder/'SDL3/SDL.h').write_text('/* Device APIs are mocked by the fixture. */\n')
        (folder/'pthread.h').write_text('/* Single-thread fixture mutex stubs. */\n')
    exe=folder/('gain.exe' if os.name=='nt' else 'gain')
    includes=[folder,RUNTIME,RUNTIME/'hle']
    args=([compiler,'/nologo','/D_CRT_SECURE_NO_WARNINGS',
           '/std:c++17' if request.param=='xaudio' else '/std:c11',
           *['/I'+str(p) for p in includes],str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c++17' if request.param=='xaudio' else '-std=c11',
           *['-I'+str(p) for p in includes],str(source),'-o',str(exe),'-lm'])
    result=subprocess.run(args,cwd=folder,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    return exe

@pytest.mark.parametrize('setting,expected',[(None,.5),('1',1),('0.25',.25),('0',0)])
def test_output_gain(output_exe,setting,expected):
    env={key:value for key,value in os.environ.items() if key not in ('RECOMP_AUDIO_GAIN','RECOMP_MUTE')}
    if setting is not None:env['RECOMP_AUDIO_GAIN']=setting
    result=subprocess.run([str(output_exe),str(expected)],env=env,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
