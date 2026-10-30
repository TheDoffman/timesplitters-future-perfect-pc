"""Compile the actual DDS parser and loader/mode code without game assets."""

from project_paths import source_file as repo_file

import os
import csv
import re
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / 'external/ts3FPrecomp/src'


@pytest.fixture(scope='module')
def texture_executables(tmp_path_factory):
    folder = tmp_path_factory.mktemp('texture-runtime')
    compiler, env = native_compiler(folder)
    device = (RUNTIME / 'd3d/d3d8_device.c').read_text(encoding='utf-8')
    sync = 'void tsfp_texture_sync_bindings(void)' + device.split(
        'void tsfp_texture_sync_bindings(void)', 1)[1].split('\n}', 1)[0] + '\n}\n'
    (folder / 'texture_binding_sync.inc').write_text(sync, encoding='utf-8')
    frames = []
    for function in ('present_resolve', 'dev_Swap', 'dev_Present'):
        match = re.search(r'^static [^\n]*\b' + function + r'\([^\n]*\)\n\{.*?\n\}',
                          device, re.M | re.S)
        assert match, function
        frames.append(match.group())
    (folder / 'texture_frame_dispatch.inc').write_text('\n'.join(frames), encoding='utf-8')
    executables = {}
    for name, sources in {
        'trace': [ROOT / 'tests/textures/texture_trace.c'],
        'format': [ROOT / 'tests/textures/texture_pack_format.c'],
        'stream': [ROOT / 'tests/textures/texture_pack_stream.c'],
        'stream_gl': [ROOT / 'tests/textures/texture_stream_gl.c', ROOT / 'src/textures/texture_mode.c'],
        'toggle': [ROOT / 'tests/textures/texture_toggle.c', ROOT / 'src/textures/texture_pack.c',
                   ROOT / 'src/textures/texture_mode.c'],
    }.items():
        exe = folder / (name + ('.exe' if os.name == 'nt' else ''))
        includes = [folder, ROOT / 'src', RUNTIME, RUNTIME / 'd3d']
        if os.name == 'nt':
            args = [compiler, '/nologo', '/std:c11', '/DTSFP_TEXTURE_PACK', '/DTSFP_TEST_BINDING_SYNC',
                    *['/I' + str(p) for p in includes],
                    *map(str, sources), '/Fe:' + str(exe)]
        else:
            args = [compiler, '-std=gnu11', '-DTSFP_TEXTURE_PACK', '-DTSFP_TEST_BINDING_SYNC', '-pthread',
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
@pytest.mark.parametrize('mode', ['classic', '4k'])
def test_legacy_gl_background_textures(texture_executables, tmp_path, streaming, mode):
    path = tmp_path / 'gl-uploads.csv'
    environment = dict(os.environ, TSFP_TEXTURE_STREAMING=streaming, TSFP_TEXTURE_UPLOAD_TRACE=str(path))
    result = subprocess.run([str(texture_executables['stream_gl']), str(tmp_path), mode],
                            cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert not list(tmp_path.glob('*.dds'))
    rows = list(csv.DictReader(path.open()))
    expected = (['deferred'] * 3 if mode == 'classic' else []) + ['loaded', 'cached', 'image_failed']
    if streaming == '0':
        expected += ['loaded']  # Completed before cancellation in synchronous mode.
    expected += ['read_rejected']
    assert [r['outcome'] for r in rows] == expected
    for row in rows:
        assert row['backend'] == 'gl'
        marks = [int(row[k]) for k in ('begin_ns', 'hash_begin_ns', 'hash_end_ns',
            'read_begin_ns', 'open_end_ns', 'read_end_ns', 'validated_ns',
            'image_begin_ns', 'image_end_ns', 'end_ns')]
        nonzero = [n for n in marks if n]
        assert nonzero == sorted(nonzero) and marks[0]
        assert all(marks[1:3]) == (int(row['source_bytes']) > 0)
        if row['outcome'] in ('loaded', 'image_failed'):
            assert all(marks[3:]) and row['replacement_bytes'] == '24'
        if row['outcome'] == 'deferred':
            assert row['enhanced'] == '0' and not any(marks[3:-1])
        assert row['view_begin_ns'] == row['view_end_ns'] == '0'



@pytest.mark.parametrize('mode', ['classic', '4k', 'missing', 'implicit'])
@pytest.mark.parametrize('streaming', ['0', '1'])
def test_cached_texture_switch(texture_executables, tmp_path, mode, streaming):
    environment = dict(os.environ, TSFP_TEXTURE_STREAMING=streaming)
    result = subprocess.run([str(texture_executables['toggle']), str(tmp_path), mode],
                            cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert not list(tmp_path.glob('*.dds'))


@pytest.mark.parametrize('mode', ['disabled', 'enabled', 'unwritable'])
def test_texture_trace_bounds(texture_executables, tmp_path, mode):
    env = dict(os.environ)
    path = tmp_path / 'trace.csv'
    env.pop('TSFP_TEXTURE_UPLOAD_TRACE', None)
    if mode != 'disabled':
        env['TSFP_TEXTURE_UPLOAD_TRACE'] = str(path if mode == 'enabled' else tmp_path / 'missing' / 'trace.csv')
    result = subprocess.run([str(texture_executables['trace']), mode], env=env,
                            cwd=tmp_path, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    if mode == 'enabled':
        rows = list(csv.DictReader(path.open()))
        assert len(rows) == 8192 and path.stat().st_size < 4 * 1024 * 1024
        assert int(rows[-1]['key'], 16) == 8191
    else:
        assert not path.exists()


@pytest.mark.parametrize('mode', ['classic', '4k'])
@pytest.mark.parametrize('streaming', ['0', '1'])
def test_texture_upload_stages(texture_executables, tmp_path, mode, streaming):
    path = tmp_path / 'uploads.csv'
    env = dict(os.environ, TSFP_TEXTURE_UPLOAD_TRACE=str(path), TSFP_TEXTURE_STREAMING=streaming)
    result = subprocess.run([str(texture_executables['toggle']), str(tmp_path), mode],
                            env=env, cwd=tmp_path, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    rows = list(csv.DictReader(path.open()))
    # Cached switches still hash their source, but must not read/recreate the
    # replacement. Keep both that work and failed attempts in the diagnostic.
    cached = [r for r in rows if r['outcome'] == 'cached']
    assert len(cached) == 20 and all(r['key'] == rows[0]['key'] for r in cached)
    expected = (['deferred'] * 3 if mode == 'classic' else []) + ['loaded', 'read_rejected', 'read_rejected', 'view_failed', 'loaded']
    if streaming == '0':
        expected += ['read_rejected', 'loaded']  # These complete before cancellation.
    expected += ['read_rejected']
    assert [r['outcome'] for r in rows if r['outcome'] != 'cached'] == expected
    assert rows[0]['enhanced'] == str(int(mode == '4k'))
    for r in rows:
        marks = [int(r[k]) for k in ('begin_ns', 'hash_begin_ns', 'hash_end_ns', 'read_begin_ns', 'open_end_ns',
            'read_end_ns', 'validated_ns', 'image_begin_ns', 'image_end_ns', 'view_begin_ns', 'view_end_ns', 'end_ns')]
        present = [n for n in marks if n]
        assert present == sorted(present) and present[0] > 0
        assert (int(r['hash_begin_ns']) > 0 and int(r['hash_end_ns']) > 0) == (int(r['source_bytes']) > 0)
        if r['outcome'] == 'deferred':
            assert r['enhanced'] == '0' and not any(marks[3:-1])
        if r['outcome'] == 'cached':
            assert all(int(r[k]) == 0 for k in ('read_begin_ns', 'open_end_ns',
                'read_end_ns', 'validated_ns', 'image_begin_ns', 'image_end_ns',
                'view_begin_ns', 'view_end_ns'))
            assert int(r['replacement_bytes']) == 24
        if r['outcome'] in ('loaded', 'view_failed'):
            assert all(marks[3:]) and int(r['replacement_bytes']) == 24
        else:
            assert int(r['image_begin_ns']) == 0
    assert not list(tmp_path.glob('*.dds'))


@pytest.mark.skipif(os.name != 'nt', reason='Win32 input/settings adapters')
@pytest.mark.parametrize('source', ['pc_controls.c', 'pc_settings.c'])
def test_windows_texture_controls_compile(tmp_path, source):
    compiler, env = native_compiler(tmp_path)
    includes = [ROOT / 'src', RUNTIME, RUNTIME / 'input', RUNTIME / 'config',
                RUNTIME.parent / 'third_party/SDL/include']
    result = subprocess.run(
        [compiler, '/nologo', '/std:c11', '/Zs',
         *['/I' + str(p) for p in includes], str(repo_file(ROOT, 'src', source))],
        cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
