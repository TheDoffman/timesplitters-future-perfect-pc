"""Exact PCM windows and private moderation routes; original generated vectors."""
import base64
import io
import json
import os
from pathlib import Path
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_audio_preview import segment
from mod_api import WorkshopAPI
from mod_package import ModError, build_package, digest
from test_creator_api import call
from test_creator_service import service, account, package, Scanner

PATH = 'audio/00000001.wav'


def wave(pcm, rate=8000, channels=1):
    fmt = struct.pack('<HHIIHH', 1, channels, rate, rate*channels*2, channels*2, 16)
    body = b'WAVEfmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(pcm)) + pcm
    return b'RIFF' + struct.pack('<I', len(body)) + body


@pytest.mark.parametrize('rate,channels', [(8000,1), (22050,2), (48000,2)])
def test_audio_windows_are_exact_contiguous_frame_aligned_and_bounded(rate, channels):
    pcm = struct.pack('<hh', -32768, 32767) * ((rate*2+2)*channels//2)
    data = wave(pcm, rate, channels)
    first, a = segment(data, 0)
    last, b = segment(data, first['next_offset'])
    assert first == dict(rate=rate, channels=channels, frames=rate*2+2, offset=0, next_offset=rate*2)
    assert last['offset'] == rate*2 and last['next_offset'] == rate*2+2
    assert a+b == pcm and len(a) == rate*2*channels*2 <= 384000
    assert len(base64.b64encode(a)) <= 512000
    assert len(json.dumps(dict(first, pcm=base64.b64encode(a).decode(),
                               sha256='f'*64, payload_sha256='f'*64, id='a'*32,
                               path=PATH, size=len(data))).encode()) < 512*1024
    for offset in (-1, True, 0.5, rate*2+2, 48000*600):
        with pytest.raises(ModError):
            segment(data, offset)


def test_audio_inspection_rejects_compressed_truncated_duplicate_or_misaligned_input():
    good = wave(b'\0\0'*10)
    compressed = bytearray(good);struct.pack_into('<H', compressed, 20, 3)
    duplicate = bytearray(good + good[12:36]);struct.pack_into('<I', duplicate, 4, len(duplicate)-8)
    for data in (good[:-1], bytes(compressed), bytes(duplicate), wave(b'\0'), b'not a wave'):
        with pytest.raises(ModError):
            segment(data, 0)


def uploaded_audio(service, tmp_path):
    accounts, uploads, _ = service
    creator, creator_token, _ = account(accounts)
    reviewer, reviewer_token, _ = account(accounts, 'reviewer', 'moderator')
    package(tmp_path)
    source = tmp_path/'test-addon-1.0.0-alice'
    (source/'audio').mkdir()
    data = wave(struct.pack('<hh', -32768, 32767) * 8001)
    (source/PATH).write_bytes(data)
    output = tmp_path/'audio.zip';build_package(source, output)
    archive = output.read_bytes()
    ticket = uploads.begin(creator, 'test-addon', '1.0.0', digest(archive), len(archive))
    uploads.receive(creator, ticket['id'], io.BytesIO(archive), len(archive))
    return accounts, uploads, reviewer, creator_token, reviewer_token, archive, data, ticket


def test_audio_api_role_scan_identity_pagination_and_read_only_contract(service, tmp_path):
    accounts, uploads, reviewer, creator_token, token, archive, data, ticket = uploaded_audio(service, tmp_path)
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    route = '/v1/moderation/'+ticket['id']+'/audio'
    request = dict(sha256=digest(archive), path=PATH, offset=0)
    assert call(api, 'POST', route, request)['status'] == 401
    assert call(api, 'POST', route, request, creator_token)['status'] == 403
    assert call(api, 'POST', route, request, token)['status'] == 404
    uploads.process_next(Scanner())
    response = call(api, 'POST', route, request, token)
    assert response['status'] == 200
    expected, pcm = segment(data, 0)
    assert response['body'] == dict(id=ticket['id'], sha256=digest(archive), path=PATH,
                                   payload_sha256=digest(data), size=len(data), **expected,
                                   pcm=base64.b64encode(pcm).decode())
    last = call(api, 'POST', route, dict(request, offset=16000), token)['body']
    assert last['next_offset'] == 16002 and base64.b64decode(last['pcm']) == data[-4:]
    for changes, status in [({'sha256':'f'*64},409), ({'path':'../private.wav'},400),
                            ({'path':'scripts/main.lua'},400), ({'offset':True},400),
                            ({'offset':16002},400), ({'offset':-1},400)]:
        assert call(api, 'POST', route, dict(request, **changes), token)['status'] == status
    assert call(api, 'POST', route, dict(request, surprise=True), token)['status'] == 400
    assert accounts.repository.catalog() == []
    with accounts.repository.db() as db:
        db.execute("UPDATE workshop_accounts SET role='creator' WHERE id=?", (reviewer.id,))
    assert call(api, 'POST', route, request, token)['status'] == 403


def test_audio_api_rate_denial_does_not_charge_global_limit(service, tmp_path):
    accounts, uploads, reviewer, _, token, archive, _, ticket = uploaded_audio(service, tmp_path)
    uploads.process_next(Scanner())
    with accounts.repository.db() as db:
        for _ in range(24):
            assert accounts.rate(db, 'audio-preview-account', str(reviewer.id), 24, 60)
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    for _ in range(50):
        assert call(api, 'POST', '/v1/moderation/'+ticket['id']+'/audio',
                    dict(sha256=digest(archive), path=PATH, offset=0), token)['status'] == 429
    with accounts.repository.db() as db:
        for _ in range(48):
            assert accounts.rate(db, 'audio-preview-global', 'all', 48, 60)


@pytest.mark.parametrize('damage', ['bytes','hardlink','symlink','fifo','writable'])
def test_audio_api_refuses_changed_or_unsafe_archive(service, tmp_path, damage):
    if os.name != 'posix' and damage in ('symlink','fifo','writable'):
        pytest.skip('Actual POSIX nofollow/ownership/mode contract')
    accounts, uploads, _, _, token, archive, _, ticket = uploaded_audio(service, tmp_path)
    uploads.process_next(Scanner())
    blob = accounts.repository.blob(digest(archive));other = tmp_path/'unrelated-original.zip'
    if damage == 'bytes':
        blob.write_bytes(b'changed')
    elif damage == 'hardlink':
        os.link(blob, other)
    elif damage == 'symlink':
        other.write_bytes(blob.read_bytes());blob.unlink();blob.symlink_to(other)
    elif damage == 'fifo':
        blob.unlink();os.mkfifo(blob, 0o600)
    else:
        blob.chmod(0o666)
    api = WorkshopAPI(accounts, 'https://workshop.invalid', uploads=uploads)
    response = call(api, 'POST', '/v1/moderation/'+ticket['id']+'/audio',
                    dict(sha256=digest(archive), path=PATH, offset=0), token)
    assert response['status'] == (409 if damage == 'bytes' else 503)
    assert 'pcm' not in response['body']
