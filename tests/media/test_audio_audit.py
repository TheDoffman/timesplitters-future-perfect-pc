"""Bounds and scope checks for the read-only audio census, using synthetic data."""
import importlib.util
import io
from pathlib import Path
import struct

import pytest

spec = importlib.util.spec_from_file_location(
    'audio_audit', Path(__file__).resolve().parents[2] / 'tools/analysis/audio_audit.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def test_index_keeps_compressed_size_and_unknown_encoding():
    payload = b'abcdef'
    data = b'P5CK' + struct.pack('<II', 18, 16) + payload
    data += struct.pack('<4I', 123, 12, 1000, 6)
    assert audit.entries(io.BytesIO(data), len(data)) == [(123, 12, 1000, 6)]


@pytest.mark.parametrize('offset,length,payload_offset,payload_size', [
    (12, 17, 28, 0), (12, 16, 25, 10), (12, 16, 12, 4), (12, 16, 4, 1),
    (12, audit.MAX_INDEX + 16, 28, 0),
])
def test_rejects_invalid_extents(offset, length, payload_offset, payload_size):
    data = b'P5CK' + struct.pack('<II', offset, length)
    data += struct.pack('<4I', 123, payload_offset, payload_size, 0)
    with pytest.raises(ValueError):
        audit.entries(io.BytesIO(data), len(data))


def test_filename_list_does_not_claim_codec_or_payload_mapping():
    names = [b'gun.vag', b'wind.MIB']
    data = struct.pack('<H', 2) + b''.join(bytes([len(n)]) + n for n in names) + b'other records'
    result = audit.filename_list(data)
    assert result['names'] == ['gun.vag', 'wind.MIB']
    assert result['remaining_bytes'] == 13
    assert not result['payload_mapping_verified']


@pytest.mark.parametrize('data', [b'', b'\1\0', b'\1\0\xffhi', b'\1\0\4a\0.b', b'\1\0\5a.txt'])
def test_rejects_bad_filename_lists(data):
    with pytest.raises(ValueError):
        audit.filename_list(data)


def test_other_archives_stay_unclassified(tmp_path):
    path = tmp_path / 'level.pak'
    path.write_bytes(b'P5CK' + struct.pack('<II', 16, 16) + b'RIFF' + struct.pack('<4I', 1, 12, 4, 0))
    result = audit.audit_archive(path, tmp_path, False)
    assert result['entries'] == 1
    assert result['audio_status'].startswith('not_classified')
    assert 'payloads' not in result


def test_archive_copies_require_matching_payload_hash(tmp_path):
    import hashlib
    path = tmp_path / 'level.pak'
    path.write_bytes(b'P5CK' + struct.pack('<II', 16, 16) + b'RIFF' + struct.pack('<4I', 1, 12, 4, 0))
    original = dict(payloads=[dict(crc='00000001', measurement=dict(
        source_sha256=hashlib.sha256(b'RIFF').hexdigest()))])
    other = dict(path='level.pak', bytes=path.stat().st_size)
    report = audit.known_audio_copies(tmp_path, [original, other])
    assert report['counts']['identical_to_bank'] == 1
    assert not report['unresolved']
    original['payloads'][0]['measurement']['source_sha256'] = hashlib.sha256(b'other').hexdigest()
    report = audit.known_audio_copies(tmp_path, [original, other])
    assert report['counts'].get('identical_to_bank', 0) == 0
    assert len(report['unresolved']) == 1
