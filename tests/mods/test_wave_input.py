"""Exercise the shared native PCM input boundary; no game assets/devices needed."""
import ctypes
import os
from pathlib import Path
import struct
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


def chunk(name, body):
    return name+struct.pack('<I', len(body))+body+(b'\0' if len(body) & 1 else b'')


def container(*chunks):
    body = b'WAVE'+b''.join(chunks)
    return b'RIFF'+struct.pack('<I', len(body))+body


def fmt(channels=1, rate=22050, code=1, bits=16, align=None, byte_rate=None, extra=b''):
    align = channels*2 if align is None else align
    return chunk(b'fmt ', struct.pack('<HHIIHH', code, channels, rate,
        rate*align if byte_rate is None else byte_rate, align, bits)+extra)


class View(ctypes.Structure):
    _fields_ = [('pcm', ctypes.c_void_p), ('bytes', ctypes.c_uint32),
               ('frames', ctypes.c_uint32), ('rate', ctypes.c_uint32), ('channels', ctypes.c_uint32)]


@pytest.fixture(scope='module')
def native(tmp_path_factory):
    directory = tmp_path_factory.mktemp('native-wave')
    compiler, env = native_compiler(directory)
    windows = Path(compiler).stem.lower() == 'cl'
    library = directory/('mod-wave.dll' if windows else 'libmod-wave.so')
    source = ROOT/'src/mods/mod_wave.c'
    if windows:
        command = [compiler, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/LD', str(source),
            '/Fo:'+str(directory/'wave.obj'), '/link', '/OUT:'+str(library),
            '/EXPORT:tsfp_mod_wave_parse', '/EXPORT:tsfp_mod_wave_error']
    else:
        command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2', '-shared', '-fPIC', str(source), '-o', str(library)]
    built = subprocess.run(command, cwd=directory, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    loaded = ctypes.CDLL(str(library))
    loaded.tsfp_mod_wave_parse.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(View)]
    loaded.tsfp_mod_wave_parse.restype = ctypes.c_int
    loaded.tsfp_mod_wave_error.argtypes = [ctypes.c_int]
    loaded.tsfp_mod_wave_error.restype = ctypes.c_char_p
    return loaded, directory, compiler, env


def parse(native, wire):
    source = ctypes.create_string_buffer(wire)
    output = View();ctypes.memset(ctypes.byref(output), 0xff, ctypes.sizeof(output))
    error = native[0].tsfp_mod_wave_parse(source, len(wire), ctypes.byref(output))
    if error:
        assert bytes(output) == b'\0'*ctypes.sizeof(output)
        return error, output, b''
    assert ctypes.addressof(source) <= output.pcm < ctypes.addressof(source)+len(wire)
    assert output.pcm+output.bytes <= ctypes.addressof(source)+len(wire)
    return error, output, ctypes.string_at(output.pcm, output.bytes)


@pytest.mark.parametrize('channels', [1, 2])
@pytest.mark.parametrize('rate', [8000, 22050, 44100, 48000])
def test_native_preserves_exact_pcm_and_reports_frames(native, channels, rate):
    pcm = struct.pack('<8h', -32768, -100, -1, 0, 1, 100, 32766, 32767)
    wire = container(chunk(b'JUNK', b'odd'), fmt(channels, rate), chunk(b'data', pcm), chunk(b'LIST', b'metadata'))
    error, output, actual = parse(native, wire)
    assert error == 0 and actual == pcm
    assert (output.rate, output.channels, output.frames, output.bytes) == (rate, channels, 8//channels, len(pcm))


@pytest.mark.parametrize('format_chunk', [fmt(extra=b'\0\0'), fmt()])
def test_pcm_format_with_empty_optional_extension(native, format_chunk):
    assert parse(native, container(format_chunk, chunk(b'data', b'\0\0')))[0] == 0


@pytest.mark.parametrize('wire', [b'', b'RIFF0000WAVE', container(fmt()),
    container(chunk(b'data', b'\0\0'), fmt()), container(fmt(), fmt(), chunk(b'data', b'\0\0')),
    container(fmt(), chunk(b'data', b'\0\0'), chunk(b'data', b'\0\0')),
    container(fmt(), chunk(b'data', b'')), container(fmt(2), chunk(b'data', b'\0\0')),
    container(fmt(), chunk(b'data', b'odd')), container(chunk(b'fmt ', b'bad'), chunk(b'data', b'\0\0')),
    container(fmt(extra=b'\1\0'), chunk(b'data', b'\0\0')),
    container(fmt(extra=b'\0\0\0\0'), chunk(b'data', b'\0\0')),
    container(fmt(), chunk(b'data', b'\0\0'))+b'trailing'])
def test_malformed_ambiguous_and_partial_frames_zero_output(native, wire):
    assert parse(native, wire)[0] != 0


@pytest.mark.parametrize('settings', [dict(code=3), dict(code=0x69), dict(code=0xfffe),
    dict(channels=0), dict(channels=3), dict(rate=7999), dict(rate=48001), dict(bits=8),
    dict(bits=24), dict(align=0), dict(align=4), dict(byte_rate=1)])
def test_unsupported_codecs_rates_channels_and_inconsistent_format_refused(native, settings):
    assert parse(native, container(fmt(**settings), chunk(b'data', b'\0\0')))[0] == 4


def test_every_truncation_padding_and_declared_chunk_overrun(native):
    wire = container(chunk(b'JUNK', b'x'), fmt(), chunk(b'data', b'\0\0'))
    for at in range(len(wire)):
        assert parse(native, wire[:at])[0] != 0
    assert parse(native, container(fmt(), chunk(b'data', b'\0\0'), b'JUNK'+struct.pack('<I', 0xffffffff)))[0] == 3
    assert parse(native, container(fmt(), chunk(b'data', b'\0\0'), b'JUNK'+struct.pack('<I', 1)+b'x'))[0] == 3


def test_explicit_chunk_byte_duration_and_argument_bounds(native):
    assert parse(native, container(*[chunk(b'JUNK', b'')]*62, fmt(), chunk(b'data', b'\0\0')))[0] == 0
    assert parse(native, container(*[chunk(b'JUNK', b'')]*63, fmt(), chunk(b'data', b'\0\0')))[0] == 6
    output = View();source = ctypes.create_string_buffer(b'x')
    assert native[0].tsfp_mod_wave_parse(source, 64*1024**2+1, ctypes.byref(output)) == 6
    assert native[0].tsfp_mod_wave_parse(None, 0, ctypes.byref(output)) == 1
    assert native[0].tsfp_mod_wave_parse(source, 1, None) == 1
    for frames, expected in [(8000*600, 0), (8000*600+1, 6)]:
        assert parse(native, container(fmt(rate=8000), chunk(b'data', b'\0\0'*frames)))[0] == expected
    assert native[0].tsfp_mod_wave_error(999) == b'Invalid WAV error code'


def test_native_mutation_harness_with_optional_sanitizers(native):
    _, directory, compiler, env = native
    source = ROOT/'tests/mods/native_wave_fuzz.c';parser = ROOT/'src/mods/mod_wave.c'
    executable = directory/('wave-fuzz.exe' if os.name == 'nt' else 'wave-fuzz')
    if Path(compiler).stem.lower() == 'cl':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/WX', '/I'+str(ROOT/'src/mods'),
            str(source), str(parser), '/Fo:'+str(directory)+os.sep, '/Fe:'+str(executable)]
    else:
        command = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT/'src/mods'), str(source), str(parser), '-o', str(executable)]
        if os.environ.get('TSFP_MOD_SANITIZE') == '1':
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    built = subprocess.run(command, cwd=directory, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout+result.stderr
    assert '200000 mutations' in result.stdout
