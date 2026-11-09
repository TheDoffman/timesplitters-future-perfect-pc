"""Synthetic scope/bounds controls for standalone candidate triage, not identity."""
import hashlib
import io
from pathlib import Path
import struct

import pytest
import audio_candidates as candidates


def test_layout_checks_late_headers_and_stereo_header_order():
    mono = b'\0\0\x20\0' + b'\x11' * 32
    assert candidates.compatible_layout(io.BytesIO(mono * 5000), 0, 36 * 5000, 1)['blocks'] == 5000
    corrupt = bytearray(mono * 5000)
    corrupt[-34] = 89
    assert candidates.compatible_layout(io.BytesIO(corrupt), 0, len(corrupt), 1) is None
    stereo = b'\0\0\x20\0\1\0\x08\0' + b'\x11' * 64
    assert candidates.compatible_layout(io.BytesIO(stereo * 2), 0, 144, 2)['channels'] == 2
    assert candidates.compatible_layout(io.BytesIO(stereo * 2), 0, 144, 1) is None
    corrupt = bytearray(stereo);corrupt[7] = 1
    assert candidates.compatible_layout(io.BytesIO(corrupt), 0, 72, 2) is None


def test_zero_and_short_data_do_not_prove_identity():
    result = candidates.compatible_layout(io.BytesIO(bytes(72)), 0, 72, 2)
    assert result['all_zero']
    assert candidates.compatible_layout(io.BytesIO(b''), 0, 0, 1) is None
    assert candidates.compatible_layout(io.BytesIO(bytes(35)), 0, 35, 1) is None
    with pytest.raises(ValueError, match='Truncated'):
        candidates.compatible_layout(io.BytesIO(bytes(36)), 0, 72, 1)


def test_signatures_are_bounded_and_not_raw_name_guesses():
    assert candidates.signatures(b'RIFF'+struct.pack('<I', 4)+b'WAVE', 12) == ['riff_wave_header']
    assert not candidates.signatures(b'RIFF'+struct.pack('<I', 500)+b'WAVE', 12)
    assert not candidates.signatures(b'RIFF', 4)
    assert candidates.signatures(bytes(12)+b'xobX'+struct.pack('<I', 4)+bytes(16), 64) == ['xmv_container_magic']
    # Magic detection does not claim a version is decodable.
    assert candidates.signatures(bytes(12)+b'xobX'+struct.pack('<I', 9)+bytes(16), 64) == ['xmv_container_magic']
    assert not candidates.signatures(bytes(12)+b'xobX'+struct.pack('<I', 4), 20)


def fixture(tmp_path):
    raw = b'\0\0\x20\0' + b'\x11' * 32
    pak = b'P5CK'+struct.pack('<II', 48, 16)+raw+struct.pack('<4I', 0x1234, 12, 36, 0)
    path=tmp_path/'level.pak';path.write_bytes(pak)
    bank=dict(path='bank.pak',sha256=hashlib.sha256(b'bank').hexdigest(),payloads=[dict(crc='00009999',name='known.xbs',channels=1,
        measurement=dict(source_sha256=hashlib.sha256(raw).hexdigest()))])
    (tmp_path/'bank.pak').write_bytes(b'bank')
    inventory=dict(archives=[bank,dict(path='level.pak',sha256=hashlib.sha256(pak).hexdigest())],
        known_audio_copies=dict(status='verified_known_audio_copies',unresolved=[]))
    return inventory, path


def test_matching_unknown_content_remains_candidate_and_sources_unchanged(tmp_path):
    inventory,path=fixture(tmp_path);before=path.read_bytes()
    report=candidates.survey(tmp_path,inventory)
    assert report['complete'] is False and report['counts']['identical_to_known_content']==1
    row=report['candidates'][0]
    assert row['identical_content'][0]['name']=='known.xbs'
    assert row['status'].startswith('candidate_only') and path.read_bytes()==before


def test_changed_archive_and_unverified_copy_exclusions_rejected(tmp_path):
    inventory,path=fixture(tmp_path);path.write_bytes(path.read_bytes()+b'changed')
    with pytest.raises(ValueError,match='changed'):
        candidates.survey(tmp_path,inventory)
    inventory['known_audio_copies']['unresolved']=[{'crc':'00009999'}]
    with pytest.raises(ValueError,match='copies verified'):
        candidates.survey(tmp_path,inventory)


def test_archive_cannot_escape_data_root(tmp_path):
    inventory,_=fixture(tmp_path);inventory['archives'][0]['path']='../outside.pak'
    with pytest.raises(ValueError,match='escapes'):
        candidates.survey(tmp_path,inventory)


def test_known_crc_never_hides_different_content(tmp_path):
    inventory,_=fixture(tmp_path)
    inventory['archives'][0]['payloads'][0]['crc']='00001234'
    inventory['archives'][0]['payloads'][0]['measurement']['source_sha256']='0'*64
    with pytest.raises(ValueError,match='Known-CRC copy'):
        candidates.survey(tmp_path,inventory)


def test_compressed_unknown_stays_explicitly_unsurveyed(tmp_path):
    inventory,path=fixture(tmp_path)
    raw=b'RIFF';pak=b'P5CK'+struct.pack('<II',16,16)+raw+struct.pack('<4I',0x1234,12,1000,4)
    path.write_bytes(pak);inventory['archives'][1]['sha256']=hashlib.sha256(pak).hexdigest()
    report=candidates.survey(tmp_path,inventory)
    assert report['counts']['compressed_not_surveyed']==1 and not report['candidates']
    assert not report['complete']
