"""Small synthetic DDS fixtures exercise download/packaging without game files."""
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import build_textures as textures


def fixture(tmp_path):
    source = tmp_path/'source'
    source.mkdir()
    words = [0]*32
    for index, value in {0: 0x20534444, 1: 124, 3: 1, 4: 1, 7: 1,
                         19: 32, 20: 4, 21: 0x31545844}.items():
        words[index] = value
    path = source/'0123456789abcdef.dds'
    path.write_bytes(struct.pack('<32I', *words)+b'\0'*8)
    path.with_suffix('.json').write_text(json.dumps({
        'key': path.stem, 'sha256': textures.RELEASE.digest(path),
        'source': '/private/art.png', 'prompt': 'private artwork prompt'}))
    records, size = textures.RELEASE.audit(source)
    part = tmp_path/'part.zip'
    textures.RELEASE.archive(records, part)
    content = part.read_bytes()
    manifest = {'schema': 1, 'version': '0.1.0-test', 'texture_count': 1,
                'installed_bytes': size, 'parts': [{'name': 'textures-001.zip',
                'size': len(content), 'sha256': hashlib.sha256(content).hexdigest()}]}
    pin = tmp_path/'manifest.json'
    pin.write_text(json.dumps(manifest))
    return source, records, content, manifest, pin


def test_download_once_then_offline_reuse_and_no_zip_cache(tmp_path):
    _, _, content, _, pin = fixture(tmp_path)
    calls = []
    def download(url, size):
        calls.append((url, size))
        return content
    root = tmp_path/'build'
    path, records, identity = textures.ensure_pack(root, manifest_path=pin, downloader=download)
    assert identity['source'] == 'release'
    assert identity['version'] == '0.1.0-test'
    assert len(calls) == 1
    assert '/releases/download/textures-v0.1.0-test/textures-001.zip' in calls[0][0]
    assert not list(root.rglob('*.zip'))
    assert not list(root.rglob('*.lock'))
    _, again, reused = textures.ensure_pack(root, manifest_path=pin, downloader=lambda *_: pytest.fail('network on cached build'))
    assert reused == identity
    assert len(again) == len(records) == 1
    assert path == root/'private/texture-remaster/pack'


def test_valid_artist_pack_is_reused_without_replacing_or_downloading(tmp_path):
    source, _, _, _, pin = fixture(tmp_path)
    before = {p.name: p.read_bytes() for p in source.iterdir()}
    _, rows, identity = textures.ensure_pack(tmp_path, source, pin, lambda *_: pytest.fail('download'))
    assert identity['source'] == 'local'
    assert len(rows) == 1
    assert before == {p.name: p.read_bytes() for p in source.iterdir()}


def test_invalid_existing_pack_is_preserved(tmp_path):
    source, _, _, _, pin = fixture(tmp_path)
    path = source/'0123456789abcdef.dds'
    path.write_bytes(b'broken artist file')
    with pytest.raises(ValueError):
        textures.ensure_pack(tmp_path, source, pin, lambda *_: pytest.fail('download'))
    assert path.read_bytes() == b'broken artist file'


@pytest.mark.parametrize('failure', ['hash', 'size', 'count', 'bytes', 'second-part'])
def test_failed_install_rolls_back_only_new_files(tmp_path, failure):
    _, _, content, manifest, pin = fixture(tmp_path)
    if failure == 'hash':
        content = content[:-1]+b'x'
    elif failure == 'size':
        content += b'x'
    elif failure == 'count':
        manifest['texture_count'] = 2
    elif failure == 'bytes':
        manifest['installed_bytes'] += 1
    elif failure == 'second-part':
        manifest['parts'].append({**manifest['parts'][0], 'name': 'textures-002.zip'})
    pin.write_text(json.dumps(manifest))
    root = tmp_path/'build'
    root.mkdir()
    settings = root/'my-settings.ini'
    settings.write_text('keep')
    with pytest.raises(ValueError):
        textures.ensure_pack(root, manifest_path=pin, downloader=lambda *_: content)
    assert settings.read_text() == 'keep'
    assert not list((root/'private/texture-remaster/pack').iterdir())
    assert not list(root.rglob('*.lock'))


@pytest.mark.parametrize('names', [('../escape.dds',), ('/escape.dds',),
                                 ('0123456789abcdef.dds',)*2, ('folder/file.dds',)])
def test_unsafe_archive_rejected_before_extraction(names):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as archive:
        for name in names:
            archive.writestr(name, b'bad')
    with pytest.raises(ValueError):
        textures.inspect_part(data.getvalue(), set(), 1000)


def test_member_expansion_and_cross_part_duplicates_rejected():
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as archive:
        archive.writestr('0123456789abcdef.dds', b'a'*100)
    with pytest.raises(ValueError, match='budget'):
        textures.inspect_part(data.getvalue(), set(), 99)
    with pytest.raises(ValueError, match='duplicate'):
        textures.inspect_part(data.getvalue(), {'0123456789abcdef.dds'}, 1000)


def test_packaging_streams_sanitized_metadata_and_hashes(tmp_path):
    _, records, _, _, _ = fixture(tmp_path)
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w') as archive:
        textures.write_zip(archive, 'Game/', records)
    with zipfile.ZipFile(output) as archive:
        for name, _, sha in textures.zip_entries(records):
            assert hashlib.sha256(archive.read('Game/'+name)).hexdigest() == sha
        metadata = json.loads(archive.read('Game/Textures/0123456789abcdef.json'))
        assert 'source' not in metadata and 'prompt' not in metadata
    records[0][0].write_bytes(b'changed after audit')
    with zipfile.ZipFile(io.BytesIO(), 'w') as archive, pytest.raises(ValueError, match='changed'):
        textures.write_zip(archive, '', records)


def test_interrupted_or_concurrent_setup_does_not_touch_files(tmp_path):
    source, _, _, _, pin = fixture(tmp_path)
    lock = source.parent/'.build-textures.lock'
    lock.write_text('existing operation')
    with pytest.raises(ValueError, match='already running'):
        textures.ensure_pack(tmp_path, source, pin)
    assert lock.read_text() == 'existing operation'
    assert len(list(source.iterdir())) == 2


def test_no_space_download_never_starts(tmp_path, monkeypatch):
    _, _, _, _, pin = fixture(tmp_path)
    from collections import namedtuple
    usage = namedtuple('Usage', 'total used free')
    monkeypatch.setattr(textures.shutil, 'disk_usage', lambda _: usage(100, 100, 0))
    with pytest.raises(ValueError, match='free disk'):
        textures.ensure_pack(tmp_path/'build', manifest_path=pin, downloader=lambda *_: pytest.fail('download'))


def test_published_download_matches_local_pack_without_extracting(tmp_path):
    source, _, content, manifest, pin = fixture(tmp_path)
    before = {p.relative_to(tmp_path): p.read_bytes() for p in tmp_path.rglob('*') if p.is_file()}
    result = textures.verify_download(source, pin, lambda *_: content)
    assert result['texture_count'] == 1 and result['installed_bytes'] == manifest['installed_bytes']
    assert before == {p.relative_to(tmp_path): p.read_bytes() for p in tmp_path.rglob('*') if p.is_file()}
    with pytest.raises(ValueError, match='checksum'):
        textures.verify_download(source, pin, lambda *_: content[:-1]+b'x')
