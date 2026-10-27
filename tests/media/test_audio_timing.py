"""Execute actual audio adapters against a deterministic fake output device."""


import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / 'external/ts3FPrecomp'


def compile_audio(kind, tmp_path_factory):
    directory = tmp_path_factory.mktemp('audio-' + kind)
    compiler, environment = native_compiler(directory)
    exe = directory / (kind + ('.exe' if os.name == 'nt' else ''))
    source = ROOT / f'tests/media/audio_{kind}_timing.c'
    if Path(compiler).stem.lower() == 'cl':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/I' + str(RUNTIME / 'src'),
                   str(source), '/Fe:' + str(exe), '/Fo:' + str(directory / 'audio.obj')]
    else:
        command = [compiler, '-std=c11', '-O2', '-I', str(RUNTIME / 'src'), str(source),
                   str(RUNTIME / 'src/platform/win32_compat.c'), '-lm', '-lpthread', '-o', str(exe)]
    built = subprocess.run(command, cwd=directory, env=environment, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    return exe


@pytest.fixture(scope='module')
def stream_test(tmp_path_factory):
    return compile_audio('stream', tmp_path_factory)


@pytest.fixture(scope='module')
def buffer_test(tmp_path_factory):
    return compile_audio('buffer', tmp_path_factory)


@pytest.mark.parametrize('scenario', ['underrun', 'process', 'flush', 'fallback',
                                     'snapshot', 'pause', 'completion', 'lost-device', 'bad-data',
                                     'diagnostics', 'adpcm', 'release-reuse', 'flush-ex', 'flush-ex-async'])
def test_stream_timing(stream_test, scenario):
    result = subprocess.run([str(stream_test), scenario], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert f'PASS: {scenario}' in result.stdout


@pytest.mark.parametrize('looping', [False, True])
def test_buffer_backpressure(buffer_test, looping):
    result = subprocess.run([str(buffer_test)] + (['loop'] if looping else []),
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout


def test_stream_headroom(buffer_test):
    result = subprocess.run([str(buffer_test), 'headroom'], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout
