"""Verify real host output code without speakers, personal settings or game data."""

from project_paths import source_file as repo_file

import os
from pathlib import Path
import re
import shutil
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]
RUNTIME=ROOT/'external/ts3FPrecomp/src'

@pytest.fixture(scope='module',params=['xaudio','sdl'])
def output_exe(request,tmp_path_factory):
    folder=tmp_path_factory.mktemp('audio-gain-'+request.param)
    compiler,env=native_compiler(folder)
    source=repo_file(ROOT, 'tests', 'audio_output_gain.cpp' if request.param=='xaudio' else 'audio_sdl_gain.c')
    if request.param=='xaudio':
        code=(RUNTIME/'hle/audio_output_xaudio2.cpp').read_text()
        functions=[]
        for signature in ('extern "C" void recomp_audio_output_initialize(void)',
                          'static float gainOf(int32_t volume_hundredth_db)',
                          'void routeVoice(uint32_t slot)',
                          'extern "C" void recomp_audio_output_set_spatial(uint32_t slot,float pan,int enabled)'):
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
    includes=[folder,RUNTIME,RUNTIME/'hle',ROOT/'src']
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


def test_weapon_sound_range(tmp_path):
    code=(RUNTIME.parent/'titles/futureperfect/src/recomp_manual.c').read_text()
    match=re.search(r'void sub_00021FB0\(void\)\n\{.*?\n\}',code,re.S)
    assert match
    (tmp_path/'weapon_range.inc').write_text(match.group())
    compiler,env=native_compiler(tmp_path)
    source=ROOT/'tests/media/weapon_sound_range.c'
    exe=tmp_path/('range.exe' if os.name=='nt' else 'range')
    includes=[tmp_path,ROOT/'src']
    args=([compiler,'/nologo','/std:c11',*['/I'+str(p) for p in includes],
           str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11',*['-I'+str(p) for p in includes],str(source),'-lm','-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr


def test_environmental_acoustics(tmp_path):
    compiler,env=native_compiler(tmp_path)
    source=ROOT/'tests/media/acoustic_scene.c'
    exe=tmp_path/('acoustics.exe' if os.name=='nt' else 'acoustics')
    args=([compiler,'/nologo','/std:c11','/I'+str(ROOT/'src'),str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-I'+str(ROOT/'src'),str(source),'-lm','-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr


def test_beta_audio_preference(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('settings.exe' if os.name=='nt' else 'settings')
    source=ROOT/'tests/media/audio_settings.c'
    args=([compiler,'/nologo','/std:c11','/I'+str(ROOT/'src'),str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-I'+str(ROOT/'src'),str(source),'-o',str(exe)])
    built=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    env={k:v for k,v in env.items() if not k.startswith(('RECOMP_','TSFP_'))}
    env['TSFP_DISPLAY_CONFIG']=str(tmp_path/'display.ini')
    def run(initial,change=None,ok=1):
        args=[str(exe),str(initial),'check']
        if change is not None:args += [str(change),str(ok)]
        result=subprocess.run(args,env=env,capture_output=True,text=True,timeout=10)
        assert result.returncode==0,result.stdout+result.stderr
    run(0,1)  # beta is opt-in; a successful save applies in this process
    run(1)  # restart reads the saved choice
    env['RECOMP_ENHANCED_AUDIO']='1'
    run(1,0)
    run(0)  # saved OFF overrides the diagnostic launch override
    env['TSFP_DISPLAY_CONFIG']=str(tmp_path/'missing'/'display.ini')
    run(1,0,0)  # failed save leaves the current live setting unchanged
