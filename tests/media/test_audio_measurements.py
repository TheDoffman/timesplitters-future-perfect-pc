"""Production decoder bridge and independently calculated measurement controls."""
import ctypes
from pathlib import Path
import os
import subprocess

import numpy as np
import pytest

from audio_measurements import Decoder, metrics, measure_payload
from audio_bank import platform_path, parse_events
import audio_bank
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope='module')
def decoder(tmp_path_factory):
    directory = tmp_path_factory.mktemp('audio-probe')
    compiler, env = native_compiler(directory)
    library = directory / ('probe.dll' if os.name == 'nt' else 'probe.so')
    source = ROOT / 'tools/analysis/audio_decode_probe.c'
    runtime = ROOT / 'external/ts3FPrecomp/src'
    codec = runtime / 'hle/xbox_adpcm.c'
    if os.name == 'nt':
        command = [compiler, '/nologo', '/LD', '/O2', '/I' + str(runtime), str(source), str(codec),
                   '/Fe:' + str(library)]
    else:
        command = [compiler, '-shared', '-fPIC', '-O2', '-I', str(runtime), str(source), str(codec),
                   '-o', str(library)]
    result = subprocess.run(command, cwd=directory, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    instance = Decoder(library)
    yield instance
    if os.name == 'nt':
        import _ctypes
        _ctypes.FreeLibrary(instance.library._handle)


def test_original_predictor_and_channel_order(decoder):
    # Independent zero-code, zero-index control: delta=7>>3=0, so each
    # channel must retain its signed header predictor for all 64 frames.
    block = b'\xe8\x03\0\0' + b'\x18\xfc\0\0' + bytes(64)
    pcm = decoder.pcm(block, 2)
    assert pcm.shape == (64, 2)
    assert np.all(pcm[:, 0] == 1000)
    assert np.all(pcm[:, 1] == -1000)
    report = metrics(pcm)
    assert report['seam_step'] == [0., 0.]
    assert report['dc_offset'] == [1000 / 32768, -1000 / 32768]
    assert report['rms_dbfs'] == pytest.approx([20 * np.log10(1000 / 32768)] * 2)


def test_bridge_refuses_capacity_before_writing(decoder):
    source = ctypes.create_string_buffer(bytes(36))
    output = (ctypes.c_int16 * 64)(*([1234] * 64))
    assert not decoder.decode(source, 36, 1, output, 63)
    assert list(output) == [1234] * 64
    assert not decoder.decode(source, 35, 1, output, 64)


def test_signed_deltas_skip_final_nibble_and_interleave(decoder):
    # With step index zero, IMA codes 1 and 9 give +1/-1. The index remains
    # zero, so all 63 deltas form exact ramps after the header predictor.
    block = bytes(8) + (b'\x11' * 4 + b'\x99' * 4) * 8
    pcm = decoder.pcm(block, 2)
    assert np.array_equal(pcm[:, 0], np.arange(64))
    assert np.array_equal(pcm[:, 1], -np.arange(64))


def test_measurement_records_partial_tail_and_silence(decoder):
    import io
    row = dict(offset=0, stored_bytes=37, compressed=False, channels=1,
               base_frequencies_hz=[8000])
    result = measure_payload(io.BytesIO(bytes(37)), row, decoder, {})
    assert result['trailing_bytes'] == 1
    assert result['frames'] == result['silent_frames'] == 64
    assert result['rms_dbfs'] == [None]
    assert result['durations_at_base_frequency'] == [dict(hz=8000, seconds=.008)]


def test_metrics_do_not_equate_full_scale_with_a_defect():
    report = metrics(np.array([[-32768, 32767], [0, 0]], dtype=np.int16))
    assert report['full_scale_samples'] == 2
    assert report['peak'] == 1
    assert report['silent_frames'] == 1
    assert 'do not establish' in report['meaning']


@pytest.mark.parametrize('language,prefix', [('e', 'sfx'), ('f', 'sfx/french'),
                                          ('g', 'sfx/german'), ('i', 'sfx/italian'),
                                          ('s', 'sfx/spanish')])
def test_language_and_platform_paths(language, prefix):
    assert platform_path('!voice.vag', language) == prefix + '/voice.xbs'
    assert platform_path('wind.MIB', language) == 'sfx/wind.xbm'


def test_event_record_bounds_and_alias_identity(monkeypatch):
    import struct
    monkeypatch.setattr(audio_bank, 'EVENTS', 1)
    data = b'\3SFX' + struct.pack('<HHIH', 0, 20 | (70 << 8), 0x210, 123)
    row = parse_events(data, 0, ['wind.MIB'])[0]
    assert row['frequency_index'] == 20 and row['volume_percent'] == 70
    assert row['channels'] == 2 and row['looping'] and row['extra_word'] == 123
    with pytest.raises(ValueError):
        parse_events(data[:-1], 0, ['wind.MIB'])
    with pytest.raises(ValueError):
        parse_events(data + b'\0', 0, ['wind.MIB'])
    with pytest.raises(ValueError):
        parse_events(data, 0, [])


def test_loudness_envelope_and_ffmpeg_summary_footer(monkeypatch):
    import struct
    import audio_loudness
    raw = bytes(72)
    envelope = audio_loudness.wave_envelope(raw, 2, 32000)
    assert envelope[:4] == b'RIFF'
    assert struct.unpack_from('<I', envelope, 4)[0] == len(envelope) - 8
    assert struct.unpack_from('<HHI', envelope, 20) == (0x69, 2, 32000)
    assert envelope[-72:] == raw
    summary = b'header\n{"input_i":"-inf","input_tp":"-3.00","input_lra":"0.00","input_thresh":"-70.00"}\nfooter stats\n'
    def fake_run(command, **options):
        assert options['input'] == envelope
        assert command[-3:] == ['-f', 'null', '-']
        return subprocess.CompletedProcess(command, 0, b'', summary)
    monkeypatch.setattr(audio_loudness.subprocess, 'run', fake_run)
    result = audio_loudness.measure(Path('ffmpeg'), raw, 2, 32000)
    assert result['integrated_lufs'] is None
    assert result['true_peak_dbtp'] == -3
    assert result['status'] == 'below_gate_or_too_short'
