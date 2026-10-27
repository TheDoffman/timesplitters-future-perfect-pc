
# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import io

import pytest


spec = importlib.util.spec_from_file_location('archive_signatures', _layout_root/'tools/textures/texture-archive-signatures.py')
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)


def dds():
    header = bytearray(128)
    header[:4] = b'DDS '
    struct.pack_into('<4I', header, 4, 124, 0, 8, 8)
    struct.pack_into('<I', header, 76, 32)
    header[84:88] = b'DXT1'
    return bytes(header)+bytes(32)


def fixture(tmp_path, payloads, compressed=None, catalog_sources=None):
    game = tmp_path/'game'
    pak = game/'pak/test.pak'
    pak.parent.mkdir(parents=True)
    root = tmp_path/'art'
    root.mkdir()
    body = b''.join(payloads)
    pos = 12
    table = b''
    for i, payload in enumerate(payloads):
        table += struct.pack('<4I', i, pos, len(payload), len(payload) if i in (compressed or []) else 0)
        pos += len(payload)
    pak.write_bytes(b'P5CK'+struct.pack('<II', pos, len(table))+body+table)
    (root/'embedded-scan.json').write_text(json.dumps({'archives': [dict(archive='pak/test.pak', bytes=pak.stat().st_size, sha256=hashlib.sha256(pak.read_bytes()).hexdigest())]}))
    (root/'catalog.json').write_text(json.dumps({'textures': [{'sources': catalog_sources or []}]}))
    return game, root, pak


def test_dds_payload_and_signatures_cannot_cross_entry_boundary():
    data = dds()+b'XPR0'
    assert scan.scan_entry(data, 0, 159)['dds']['statuses'] == {'truncated_base': 1}
    assert scan.scan_entry(data, 0, 160)['dds']['statuses'] == {'bounded_base_candidate': 1}
    assert 'xpr0' not in scan.scan_entry(data, 160, 163)
    assert scan.scan_entry(data, 160, 164)['xpr0']['count'] == 1
    with pytest.raises(ValueError):
        scan.scan_entry(data, 0, len(data)+1)


def test_skips_exact_catalog_identity_but_scans_embedded_container(tmp_path):
    sources = [dict(archive='pak/test.pak', crc='00000000', offset=12, length=4),
               dict(archive='pak/test.pak', crc='00000001', offset=16, length=4, embedded=True)]
    game, root, _ = fixture(tmp_path, [b'XPR0', b'XPR1'], catalog_sources=sources)
    before = (root/'catalog.json').read_bytes()
    result = scan.scan(game, root)
    assert result['totals']['cataloged_top_level_entries_skipped'] == 1
    assert result['signature_counts'] == {'xpr1': 1}
    assert result['complete'] is False and result['catalog_changes'] == 0
    assert (root/'catalog.json').read_bytes() == before


def test_compression_is_explicitly_unscanned_and_dds_is_entry_bounded(tmp_path):
    game, root, _ = fixture(tmp_path, [dds()[:159], b'\0XPR0', b'XPR1'], compressed=[2])
    result = scan.scan(game, root)
    assert result['dds_statuses'] == {'truncated_base': 1}
    assert result['signature_counts'] == {'dds': 1, 'xpr0': 1}
    assert result['totals']['compressed_entries_skipped'] == 1


def test_report_limits_preserve_all_counts(tmp_path):
    game, root, _ = fixture(tmp_path, [b'XPR0'*40, b'XPR1'])
    result = scan.scan(game, root, sample_limit=2, entry_limit=1)
    assert result['signature_counts'] == {'xpr0': 40, 'xpr1': 1}
    assert result['entry_samples_truncated']
    hit = result['hits'][0]['signatures']['xpr0']
    assert len(hit['samples']) == 2 and hit['samples_truncated']


def test_archive_change_and_set_change_are_rejected(tmp_path):
    game, root, pak = fixture(tmp_path, [b'XPR0'])
    original = pak.read_bytes()
    pak.write_bytes(original.replace(b'XPR0', b'XPR1'))
    with pytest.raises(ValueError, match='differs'):
        scan.scan(game, root)
    pak.write_bytes(original)
    (pak.parent/'extra.pak').write_bytes(original)
    with pytest.raises(ValueError, match='Archive set'):
        scan.scan(game, root)


def test_catalog_identity_mismatch_is_rejected(tmp_path):
    game, root, _ = fixture(tmp_path, [b'XPR0'], catalog_sources=[dict(archive='pak/test.pak', crc='00000000', offset=12, length=3)])
    with pytest.raises(ValueError, match='Missing cataloged'):
        scan.scan(game, root)


def test_jpeg_decode_is_bounded_and_does_not_accept_magic_only():
    encoded = io.BytesIO()
    scan.Image.new('RGB', (8, 8), 'red').save(encoded, format='JPEG')
    data = encoded.getvalue()
    assert scan.jpeg_candidate(data, 0, len(data))['status'] == 'jpeg_decoded_candidate'
    assert scan.jpeg_candidate(data, 0, len(data)-20)['status'] == 'jpeg_decoder_rejected'
    assert scan.jpeg_candidate(b'\xff\xd8\xff'+bytes(100), 0, 103)['status'] == 'jpeg_decoder_rejected'
    result = scan.scan_entry(data+data, 0, 2*len(data), sample_limit=0)
    assert result['jpeg']['statuses'] == {'jpeg_decoded_candidate': 2}


def test_bounded_reader_seek_and_eof():
    with scan.EntryReader(b'outsidePAYLOADoutside', 7, 14) as reader:
        assert reader.read(100) == b'PAYLOAD'
        assert reader.read() == b''
        assert reader.seek(-2, 2) == 5 and reader.read() == b'AD'
        reader.seek(100)
        assert reader.read() == b''
        with pytest.raises(ValueError):
            reader.seek(-1)


def test_decoder_limit_remains_unverified(monkeypatch):
    def limited(*args, **kwargs):
        raise scan.DecoderLimit('test limit')
    monkeypatch.setattr(scan.Image, 'open', limited)
    assert scan.jpeg_candidate(b'\xff\xd8\xff', 0, 3)['status'] == 'jpeg_resource_limit_unverified'
