"""Compile the actual DDS parser and loader/mode code without game assets."""
import os
import csv
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
    executables = {}
    for name, sources in {
        'format': [ROOT / 'tests/texture_pack_format.c'],
        'trace': [ROOT / 'tests/texture_trace.c'],
        'toggle': [ROOT / 'tests/texture_toggle.c', ROOT / 'src/texture_pack.c',
                   ROOT / 'src/texture_mode.c'],
    }.items():
        exe = folder / (name + ('.exe' if os.name == 'nt' else ''))
        includes = [ROOT / 'src', RUNTIME, RUNTIME / 'd3d']
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


@pytest.mark.parametrize('mode', ['classic', '4k', 'missing', 'implicit'])
def test_cached_texture_switch(texture_executables, tmp_path, mode):
    env = dict(os.environ)
    env.pop('TSFP_TEXTURE_UPLOAD_TRACE', None)
    result = subprocess.run([str(texture_executables['toggle']), str(tmp_path), mode],
                            cwd=tmp_path, env=env, capture_output=True, text=True, timeout=15)
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
def test_texture_upload_stages(texture_executables, tmp_path, mode):
    path = tmp_path / 'uploads.csv'
    env = dict(os.environ, TSFP_TEXTURE_UPLOAD_TRACE=str(path))
    result = subprocess.run([str(texture_executables['toggle']), str(tmp_path), mode],
                            env=env, cwd=tmp_path, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    rows = list(csv.DictReader(path.open()))
    # Cached switches still hash their source, but must not read/recreate the
    # replacement. Keep both that work and failed attempts in the diagnostic.
    cached = [r for r in rows if r['outcome'] == 'cached']
    assert len(cached) == 20 and all(r['key'] == rows[0]['key'] for r in cached)
    assert [r['outcome'] for r in rows if r['outcome'] != 'cached'] == [
        'loaded', 'read_rejected', 'read_rejected', 'view_failed', 'loaded', 'read_rejected']
    assert rows[0]['enhanced'] == str(int(mode == '4k'))
    for r in rows:
        marks = [int(r[k]) for k in ('begin_ns', 'hash_begin_ns', 'hash_end_ns', 'read_begin_ns', 'open_end_ns',
            'read_end_ns', 'validated_ns', 'image_begin_ns', 'image_end_ns', 'view_begin_ns', 'view_end_ns', 'end_ns')]
        present = [n for n in marks if n]
        assert present == sorted(present) and present[0] > 0
        assert int(r['hash_begin_ns']) > 0 and int(r['hash_end_ns']) > 0
        if r['outcome'] == 'cached':
            assert all(int(r[k]) == 0 for k in ('read_begin_ns', 'open_end_ns',
                'read_end_ns', 'validated_ns', 'image_begin_ns', 'image_end_ns',
                'view_begin_ns', 'view_end_ns'))
            assert int(r['replacement_bytes']) == 24
        if r['outcome'] in ('loaded', 'view_failed'):
            assert all(marks) and int(r['replacement_bytes']) == 24
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
         *['/I' + str(p) for p in includes], str(ROOT / 'src' / source)],
        cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
