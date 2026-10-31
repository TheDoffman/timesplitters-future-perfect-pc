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
    includes = [folder, ROOT/'external/ts3FPrecomp/src', ROOT/'src']
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
                                     'com-retry', 'device-submit', 'live-volume', 'register', 'master-volume', 'enhanced-on', 'enhanced-off', 'diagnostics', 'exit-failure', 'exit-muted'])
def test_device_recovery(recovery_exe, scenario):
    env = {k:v for k,v in os.environ.items() if k not in ('RECOMP_AUDIO_GAIN', 'RECOMP_MUTE')}
    if scenario in ('muted', 'invalid-gain', 'exit-muted'):
        env['RECOMP_AUDIO_GAIN'] = 'bad' if scenario == 'invalid-gain' else '0'
    result = subprocess.run([str(recovery_exe), scenario], env=env, capture_output=True,
                            text=True, timeout=15)
    assert result.returncode == 0, result.stdout+result.stderr
    if scenario in ('diagnostics', 'exit-failure', 'exit-muted'):
        assert result.stderr.count('[audio-output] final health:') == 1
    if scenario == 'exit-failure':
        assert 'device_failed=1 open_attempts=1 opens=0' in result.stderr
    if scenario == 'exit-muted':
        assert 'device_failed=0 open_attempts=0 opens=0' in result.stderr


@pytest.fixture(scope='module')
def sdl_recovery_exe(tmp_path_factory):
    folder = tmp_path_factory.mktemp('sdl-recovery')
    compiler, env = native_compiler(folder)
    (folder/'SDL3').mkdir()
    (folder/'SDL3/SDL.h').write_text('/* Mocked SDL API. */\n')
    (folder/'pthread.h').write_text('/* Single-threaded mutex fixture. */\n')
    source = ROOT/'tests/media/audio_sdl_recovery.c'
    includes = [folder, ROOT/'external/ts3FPrecomp/src/hle', ROOT/'src']
    exe = folder/('sdl.exe' if os.name == 'nt' else 'sdl')
    if os.name == 'nt':
        cmd = [compiler, '/nologo', '/std:c11', '/D_CRT_SECURE_NO_WARNINGS',
               *['/I'+str(p) for p in includes], str(source), '/Fe:'+str(exe)]
    else:
        cmd = [compiler, '-std=c11', *['-I'+str(p) for p in includes], str(source), '-lm', '-o', str(exe)]
    built = subprocess.run(cmd, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    return exe

@pytest.mark.parametrize('scenario', ['startup', 'subsystem', 'shutdown', 'queue', 'bind', 'put', 'volume', 'enhanced-on', 'enhanced-off', 'diagnostics', 'exit-failure', 'exit-muted'])
def test_sdl_recovery(sdl_recovery_exe, scenario):
    env = {k:v for k,v in os.environ.items() if k not in ('RECOMP_AUDIO_GAIN', 'RECOMP_MUTE')}
    if scenario == 'exit-muted': env['RECOMP_AUDIO_GAIN'] = '0'
    result = subprocess.run([str(sdl_recovery_exe), scenario], env=env, capture_output=True,
                            text=True, timeout=15)
    assert result.returncode == 0, result.stdout+result.stderr
    if scenario in ('diagnostics', 'exit-failure', 'exit-muted'):
        assert result.stderr.count('[audio-output] final health:') == 1
    if scenario == 'exit-failure':
        assert 'device_failed=1 open_attempts=1 opens=0' in result.stderr
    if scenario == 'exit-muted':
        assert 'device_failed=0 open_attempts=0 opens=0' in result.stderr


@pytest.mark.skipif(os.name != 'nt' or os.environ.get('TSFP_TEST_AUDIO_DEVICE') != '1',
                    reason='Opt-in local Windows device check; no physical unplug is simulated')
def test_real_xaudio_reopen(tmp_path):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/'live.exe'
    cmd = [compiler, '/nologo', '/std:c++17', '/EHsc', '/D_CRT_SECURE_NO_WARNINGS',
           '/I'+str(ROOT/'external/ts3FPrecomp/src'), '/I'+str(ROOT/'src'), str(ROOT/'tests/media/audio_device_live.cpp'),
           '/Fe:'+str(exe), '/link', 'xaudio2.lib', 'ole32.lib']
    built = subprocess.run(cmd, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    env = {k:v for k,v in os.environ.items() if k not in ('RECOMP_AUDIO_GAIN', 'RECOMP_MUTE')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=15)
    if result.returncode == 77:
        pytest.skip('No default audio device available')
    assert result.returncode == 0, result.stdout+result.stderr
    print(result.stdout+result.stderr)


@pytest.fixture(scope='module', params=['xaudio', 'sdl'])
def lifecycle_exe(request, tmp_path_factory):
    """Link shared C buffer HLE to each actual adapter, with only host APIs faked."""
    folder = tmp_path_factory.mktemp('audio-lifecycle-' + request.param)
    compiler, env = native_compiler(folder)
    fake = folder / 'host'
    fake.mkdir()
    (fake / 'SDL3').mkdir()
    for header in ('windows.h', 'objbase.h', 'xaudio2.h', 'SDL3/SDL.h'):
        (fake / header).write_text('/* Host APIs supplied by fixture. */\n')
    if request.param == 'sdl':
        (fake / 'pthread.h').write_text('/* SDL fixture supplies mutex APIs. */\n')
    runtime = ROOT / 'external/ts3FPrecomp/src'
    includes = [runtime, runtime/'hle', ROOT/'src', ROOT/'src/runtime']
    sources = [ROOT/'tests/media/audio_lifecycle_stress.c']
    if os.name != 'nt':
        sources += [runtime/'platform/win32_compat.c', ROOT/'src/native/shared_host.c']
        sources += [ROOT/'src/runtime'/name for name in
                    ('shared_views.c', 'owned_arena.c', 'mmio_views.c', 'guest_arena_service.c')]
    objects = []
    for source in sources:
        obj = folder / (source.stem + ('.obj' if os.name == 'nt' else '.o'))
        if os.name == 'nt':
            cmd = [compiler, '/nologo', '/std:c11', '/O2', '/c',
                   *['/I'+str(p) for p in includes], str(source), '/Fo:'+str(obj)]
        else:
            cmd = [compiler, '-std=c11', '-O2', '-c', *['-I'+str(p) for p in includes],
                   str(source), '-o', str(obj)]
        built = subprocess.run(cmd, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
        assert built.returncode == 0, built.stdout + built.stderr
        objects.append(str(obj))
    cpp = request.param == 'xaudio'
    driver = ROOT/f'tests/media/audio_lifecycle_{request.param}.{ "cpp" if cpp else "c" }'
    exe = folder / ('lifecycle.exe' if os.name == 'nt' else 'lifecycle')
    if os.name == 'nt':
        cmd = [compiler, '/nologo', '/std:c++17' if cpp else '/std:c11', '/EHsc',
               *['/I'+str(p) for p in [fake, *includes]], str(driver), *objects, '/Fe:'+str(exe)]
    else:
        linker = shutil.which(env.get('CXX', 'c++')) if cpp else compiler
        cmd = [linker, '-std=c++17' if cpp else '-std=c11',
               *['-I'+str(p) for p in [fake, *includes]], str(driver), *objects,
               '-lm', '-pthread', '-o', str(exe)]
    built = subprocess.run(cmd, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout + built.stderr
    return exe


@pytest.mark.parametrize('enhanced', [0, 1])
def test_release_during_recovery_and_scene_teardown(lifecycle_exe, enhanced):
    env = {k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    result = subprocess.run([str(lifecycle_exe), str(enhanced)], env=env,
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS: 32 scene teardowns' in result.stdout
    assert result.stderr.count('[DSOUND] final health:') == 1
    assert 'occupied=0/256 pool_full=0 release_calls=256 released_slots=256' in result.stderr
    assert result.stderr.count('[audio-output] final health:') == 1
