"""Authored loop boundaries, missing definitions and source-hash safeguards."""
import hashlib
import struct
import sys

import numpy as np
import pytest

from audio_loops import encoded_region, loop_metadata, main, map_regions, measure_regions


class Image:
    def __init__(self, times=((.064, .192),), names=('loop',)):
        self.times, self.names = times, names

    def at(self, address, size):
        if address == 0x4B9110:
            assert size == 4
            return struct.pack('<I', len(self.times))
        assert size == 12
        index = (address - 0x4B90B0) // 12
        return struct.pack('<Iff', index, *self.times[index])

    def string(self, pointer):
        return self.names[pointer]


def test_original_sample_block_conversion_keeps_intro_and_authored_end():
    row, = loop_metadata(Image())
    assert encoded_region(row, 1000) == (36, 108)
    # .903f * 22042 is approximately19903.9: retain310 complete ADPCM blocks.
    assert encoded_region(dict(start_seconds=.903, end_seconds=2.425), 22042) == (11160, 30060)
    # SSE rounds the multiply to single precision before integer truncation.
    # Double precision gives15359.999656...; SSE single gives15360.
    assert encoded_region(dict(start_seconds=.96, end_seconds=1.2), 16000) == (8640, 10800)


@pytest.mark.parametrize('times,names', [
    ((), ()), (((float('nan'), 1),), ('a',)), (((0, float('inf')),), ('a',)),
    (((-1, 1),), ('a',)), (((1, 1),), ('a',)), (((0, 1),), ('',)),
    (((0, 1),), ('a\0b',)), (((0, 1), (0, 2)), ('a', 'a')),
])
def test_bad_original_records_are_rejected(times, names):
    with pytest.raises(ValueError):
        loop_metadata(Image(times, names))


@pytest.mark.parametrize('rate', [0, 187, 191984, 1000.5])
def test_frequency_must_fit_original_caller_bounds(rate):
    with pytest.raises(ValueError):
        encoded_region(dict(start_seconds=0, end_seconds=1), rate)


@pytest.mark.parametrize('end', [1e30, 1e38, 1e300])
def test_invalid_integer_conversion_or_single_precision_overflow(end):
    with pytest.raises(ValueError):
        encoded_region(dict(start_seconds=0, end_seconds=end), 48000)


def fixture(tmp_path):
    data = bytes(144)
    pcm = np.arange(256, dtype=np.int16).reshape(-1, 1)
    (tmp_path / 'test.pak').write_bytes(data)
    payload = dict(name='sfx/loop.xbs', channels=1, stored_bytes=len(data), offset=0,
                   compressed=False, base_frequencies_hz=[1000], measurement=dict(
                       source_sha256=hashlib.sha256(data).hexdigest(),
                       pcm_sha256=hashlib.sha256(pcm.tobytes()).hexdigest()))
    inventory = dict(archives=[dict(path='test.pak', payloads=[payload])])
    class Decoder:
        def pcm(self, source, channels):
            assert source == data and channels == 1
            return pcm
    return inventory, Decoder(), payload


def test_actual_region_seam_does_not_use_whole_file_boundary(tmp_path):
    inventory, decoder, _ = fixture(tmp_path)
    rows = map_regions(loop_metadata(Image()), inventory)
    measure_regions(rows, tmp_path, inventory, decoder)
    rate, = rows[0]['matches'][0]['rates']
    assert (rate['start_frame'], rate['end_frame'], rate['intro_frames']) == (64, 192, 64)
    assert rate['loop_seconds'] == .128
    assert rate['seam_step'] == [127 / 32768]  # Whole-file seam would be255/32768.


def test_missing_records_and_out_of_source_regions_stay_in_denominator(tmp_path):
    inventory, _, _ = fixture(tmp_path)
    records = loop_metadata(Image(((.064, .4), (0, 1)), ('loop', 'unknown')))
    rows = map_regions(records, inventory)
    assert len(rows) == 2 and rows[1]['status'] == 'not_found_in_inventory'
    assert rows[0]['matches'][0]['rates'][0]['status'] == 'outside_verified_source_layout'


@pytest.mark.parametrize('change', ['source', 'pcm', 'path', 'compression'])
def test_mismatch_or_unverified_payload_refuses_measurement(tmp_path, change):
    inventory, decoder, payload = fixture(tmp_path)
    if change == 'source':
        (tmp_path / 'test.pak').write_bytes(bytes([1]) * 144)
    elif change == 'pcm':
        payload['measurement']['pcm_sha256'] = '0' * 64
    elif change == 'path':
        inventory['archives'][0]['path'] = '../outside.pak'
    else:
        payload['compressed'] = True
    rows = map_regions(loop_metadata(Image()), inventory)
    with pytest.raises(ValueError):
        measure_regions(rows, tmp_path, inventory, decoder)


@pytest.mark.parametrize('existing', [False, True])
def test_cli_preserves_game_data_and_existing_evidence(tmp_path, monkeypatch, existing):
    game = tmp_path / 'game'
    game.mkdir()
    output = tmp_path / 'selected.json' if existing else game / 'source.json'
    output.write_text('retain this evidence', encoding='utf-8')
    monkeypatch.setattr(sys, 'argv', ['audio_loops', '--game', str(game),
                                     '--inventory', str(tmp_path / 'inventory.json'),
                                     '--output', str(output)])
    with pytest.raises(SystemExit) as error:
        main()
    assert error.value.code == 2 and output.read_text() == 'retain this evidence'
