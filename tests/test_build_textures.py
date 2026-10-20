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


def test_repository_pack_is_reused_offline_without_copies(tmp_path):
    source, _, _, _, _ = fixture(tmp_path)
    target = tmp_path/'assets/textures'
    target.parent.mkdir()
    source.rename(target)
    before = {p.name: p.read_bytes() for p in target.iterdir()}
    path, records, identity = textures.ensure_pack(tmp_path)
    assert path == target and len(records) == 1
    assert identity['source'] == 'repository'
    assert before == {p.name: p.read_bytes() for p in target.iterdir()}
    assert not (tmp_path/'private').exists()


def test_artist_override_is_explicit_and_preserved(tmp_path):
    source, _, _, _, _ = fixture(tmp_path)
    _, rows, identity = textures.ensure_pack(tmp_path, source)
    assert identity['source'] == 'local' and len(rows) == 1
    with pytest.raises(ValueError, match='missing'):
        textures.ensure_pack(tmp_path)


def test_lfs_pointer_fails_with_recovery_instruction(tmp_path):
    source, _, _, _, _ = fixture(tmp_path)
    pointer = b'version https://git-lfs.github.com/spec/v1\noid sha256:abc\nsize 136\n'
    path = source/'0123456789abcdef.dds'
    path.write_bytes(pointer)
    with pytest.raises(ValueError, match='git lfs pull'):
        textures.ensure_pack(tmp_path, source)
    assert path.read_bytes() == pointer


def test_corrupt_texture_is_preserved_and_rejected(tmp_path):
    source, _, _, _, _ = fixture(tmp_path)
    path = source/'0123456789abcdef.dds'
    path.write_bytes(b'broken artist file')
    with pytest.raises(ValueError):
        textures.ensure_pack(tmp_path, source)
    assert path.read_bytes() == b'broken artist file'


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


def test_repository_export_tracks_only_audited_public_content(tmp_path):
    import importlib.util
    import subprocess
    spec = importlib.util.spec_from_file_location('repository_textures', ROOT/'tools/texture-repository.py')
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    source, _, _, _, _ = fixture(tmp_path)
    repo = tmp_path/'repo'
    repo.mkdir()
    (repo/'config').mkdir()
    subprocess.run(['git', 'init', str(repo)], check=True, capture_output=True)
    if subprocess.run(['git', 'lfs', 'version'], capture_output=True).returncode:
        pytest.skip('Git LFS unavailable')
    exporter.stage(source, root=repo, pointers_only=True)
    pointer = (repo/'assets/textures/0123456789abcdef.dds').read_text()
    assert pointer.startswith('version https://git-lfs.github.com/spec/v1')
    assert textures.RELEASE.digest(source/'0123456789abcdef.dds') in pointer
    public = json.loads((repo/'assets/textures/0123456789abcdef.json').read_text())
    assert 'source' not in public and 'prompt' not in public
    assert json.loads((repo/'config/texture-pack.json').read_text())['texture_count'] == 1
    with pytest.raises(ValueError, match='git lfs pull'):
        textures.ensure_pack(repo)
