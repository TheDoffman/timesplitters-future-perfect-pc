"""Exercise the complete XAudio adapter with deterministic device failures."""
import os
from pathlib import Path
import shutil
import subprocess
import pytest
from native_compiler import native_compiler
ROOT = Path(__file__).resolve().parents[2]

@pytest.fixture(scope='module')
def recovery_exe(tmp_path_factory):
    folder = tmp_path_factory.mktemp('audio-recovery')
    compiler, env = native_compiler(folder)
    for header in ('windows.h', 'objbase.h', 'xaudio2.h'):
        (folder/header).write_text('/* Host APIs supplied by fixture. */\n')
    source = ROOT/'tests/media/audio_device_recovery.cpp'
    includes = [folder, ROOT/'external/ts3FPrecomp/src']
    exe = folder/('recovery.exe' if os.name == 'nt' else 'recovery')
    if os.name == 'nt':
        cmd = [compiler, '/nologo', '/std:c++17', '/EHsc', '/D_CRT_SECURE_NO_WARNINGS',
               *['/I'+str(p) for p in includes], str(source), '/Fe:'+str(exe)]
    else:
        compiler = shutil.which(env.get('CXX', 'c++'))
        assert compiler
        cmd = [compiler, '-std=c++17', '-pthread', *['-I'+str(p) for p in includes],
               str(source), '-o', str(exe)]
    built = subprocess.run(cmd, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    return exe

@pytest.mark.parametrize('scenario', ['startup', 'critical', 'create', 'start', 'volume',
                                     'submit', 'queue', 'shutdown', 'muted', 'invalid-gain', 'concurrent',
                                     'com-retry', 'device-submit', 'live-volume', 'register', 'master-volume'])
def test_device_recovery(recovery_exe, scenario):
    env = {k:v for k,v in os.environ.items() if k not in ('RECOMP_AUDIO_GAIN', 'RECOMP_MUTE')}
    if scenario in ('muted', 'invalid-gain'):
        env['RECOMP_AUDIO_GAIN'] = '0' if scenario == 'muted' else 'bad'
    result = subprocess.run([str(recovery_exe), scenario], env=env, capture_output=True,
                            text=True, timeout=15)
    assert result.returncode == 0, result.stdout+result.stderr


@pytest.fixture(scope='module')
def sdl_recovery_exe(tmp_path_factory):
    folder = tmp_path_factory.mktemp('sdl-recovery')
    compiler, env = native_compiler(folder)
    (folder/'SDL3').mkdir()
    (folder/'SDL3/SDL.h').write_text('/* Mocked SDL API. */\n')
    (folder/'pthread.h').write_text('/* Single-threaded mutex fixture. */\n')
    source = ROOT/'tests/media/audio_sdl_recovery.c'
    includes = [folder, ROOT/'external/ts3FPrecomp/src/hle']
    exe = folder/('sdl.exe' if os.name == 'nt' else 'sdl')
    if os.name == 'nt':
        cmd = [compiler, '/nologo', '/std:c11', '/D_CRT_SECURE_NO_WARNINGS',
               *['/I'+str(p) for p in includes], str(source), '/Fe:'+str(exe)]
    else:
        cmd = [compiler, '-std=c11', *['-I'+str(p) for p in includes], str(source), '-lm', '-o', str(exe)]
    built = subprocess.run(cmd, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    return exe

@pytest.mark.parametrize('scenario', ['startup', 'subsystem', 'shutdown', 'queue', 'bind', 'put', 'volume'])
def test_sdl_recovery(sdl_recovery_exe, scenario):
    env = {k:v for k,v in os.environ.items() if k not in ('RECOMP_AUDIO_GAIN', 'RECOMP_MUTE')}
    result = subprocess.run([str(sdl_recovery_exe), scenario], env=env, capture_output=True,
                            text=True, timeout=15)
    assert result.returncode == 0, result.stdout+result.stderr


@pytest.mark.skipif(os.name != 'nt' or os.environ.get('TSFP_TEST_AUDIO_DEVICE') != '1',
                    reason='Opt-in local Windows device check; no physical unplug is simulated')
def test_real_xaudio_reopen(tmp_path):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/'live.exe'
    cmd = [compiler, '/nologo', '/std:c++17', '/EHsc', '/D_CRT_SECURE_NO_WARNINGS',
           '/I'+str(ROOT/'external/ts3FPrecomp/src'), str(ROOT/'tests/media/audio_device_live.cpp'),
           '/Fe:'+str(exe), '/link', 'xaudio2.lib', 'ole32.lib']
    built = subprocess.run(cmd, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    env = {k:v for k,v in os.environ.items() if k not in ('RECOMP_AUDIO_GAIN', 'RECOMP_MUTE')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=15)
    if result.returncode == 77:
        pytest.skip('No default audio device available')
    assert result.returncode == 0, result.stdout+result.stderr
    print(result.stdout+result.stderr)
