"""Compile the actual DDS parser and loader/mode code without game assets."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'external/ts3FPrecomp/src'


@pytest.fixture(scope='module')
def texture_executables(tmp_path_factory):
    folder = tmp_path_factory.mktemp('texture-runtime')
    compiler, env = native_compiler(folder)
    device = (RUNTIME / 'd3d/d3d8_device.c').read_text(encoding='utf-8')
    sync = 'void tsfp_texture_sync_bindings(void)' + device.split(
        'void tsfp_texture_sync_bindings(void)', 1)[1].split('\n}', 1)[0] + '\n}\n'
    (folder / 'texture_binding_sync.inc').write_text(sync, encoding='utf-8')
    executables = {}
    for name, sources in {
        'format': [ROOT / 'tests/texture_pack_format.c'],
        'stream': [ROOT / 'tests/texture_pack_stream.c'],
        'stream_gl': [ROOT / 'tests/texture_stream_gl.c'],
        'toggle': [ROOT / 'tests/texture_toggle.c', ROOT / 'src/texture_pack.c',
                   ROOT / 'src/texture_mode.c'],
    }.items():
        exe = folder / (name + ('.exe' if os.name == 'nt' else ''))
        includes = [folder, ROOT / 'src', RUNTIME, RUNTIME / 'd3d']
        if os.name == 'nt':
            args = [compiler, '/nologo', '/std:c11', '/DTSFP_TEXTURE_PACK',
                    *['/I' + str(p) for p in includes],
                    *map(str, sources), '/Fe:' + str(exe)]
        else:
            args = [compiler, '-std=gnu11', '-DTSFP_TEXTURE_PACK', '-pthread',
                    *['-I' + str(p) for p in includes],
                    *map(str, sources), '-o', str(exe)]
        result = subprocess.run(args, cwd=folder, env=env, capture_output=True,
                                text=True, timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        executables[name] = exe
    return executables


def test_dds_parser(texture_executables):
    result = subprocess.run([str(texture_executables['format'])],
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize('mode', ['on', 'off'])
def test_background_texture_reads(texture_executables, tmp_path, mode):
    environment=dict(os.environ,TSFP_TEXTURE_STREAMING='0' if mode=='off' else '1')
    result=subprocess.run([str(texture_executables['stream']),str(tmp_path/'input.dds'),mode],
                          cwd=tmp_path,env=environment,capture_output=True,text=True,timeout=20)
    assert result.returncode==0,result.stdout+result.stderr
    assert not list(tmp_path.glob('*.dds'))


@pytest.mark.parametrize('streaming', ['0', '1'])
def test_legacy_gl_background_textures(texture_executables, tmp_path, streaming):
    environment = dict(os.environ, TSFP_TEXTURE_STREAMING=streaming)
    result = subprocess.run([str(texture_executables['stream_gl']), str(tmp_path)],
                            cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert not list(tmp_path.glob('*.dds'))


@pytest.mark.parametrize('mode', ['classic', '4k', 'missing', 'implicit'])
@pytest.mark.parametrize('streaming', ['0', '1'])
def test_cached_texture_switch(texture_executables, tmp_path, mode, streaming):
    environment = dict(os.environ, TSFP_TEXTURE_STREAMING=streaming)
    result = subprocess.run([str(texture_executables['toggle']), str(tmp_path), mode],
                            cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert not list(tmp_path.glob('*.dds'))


@pytest.mark.skipif(os.name != 'nt', reason='Win32 input/settings adapters')
@pytest.mark.parametrize('source', ['pc_controls.c', 'pc_settings.c'])
def test_windows_texture_controls_compile(tmp_path, source):
    compiler, env = native_compiler(tmp_path)
    includes = [ROOT / 'src', RUNTIME, RUNTIME / 'input', RUNTIME / 'config',
                RUNTIME.parent / 'third_party/SDL/include']
    result = subprocess.run(
        [compiler, '/nologo', '/std:c11', '/Zs',
         *['/I' + str(p) for p in includes], str(ROOT / 'src' / source)],
        cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
