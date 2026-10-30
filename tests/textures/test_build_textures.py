"""Small synthetic DDS fixtures exercise download/packaging without game files."""


import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import build_textures as textures


def fixture(tmp_path, dimension=1):
    source = tmp_path/'source'
    source.mkdir()
    words = [0]*32
    for index, value in {0: 0x20534444, 1: 124, 3: dimension, 4: dimension, 7: dimension.bit_length(),
                         19: 32, 20: 4, 21: 0x31545844}.items():
        words[index] = value
    path = source/'0123456789abcdef.dds'
    payload_size = sum(((max(1, dimension >> mip)+3)//4)**2*8
                       for mip in range(dimension.bit_length()))
    path.write_bytes(struct.pack('<32I', *words)+b'\0'*payload_size)
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


def test_release_keeps_one_texture_copy_and_reproducible_sources(tmp_path):
    source, _, _, _, _ = fixture(tmp_path)
    pack = tmp_path/'assets/textures'
    pack.parent.mkdir()
    source.rename(pack)
    _, records, _ = textures.ensure_pack(tmp_path)
    files = {'src/game.c': b'/* source */', 'assets/icon.png': b'icon',
             'config/textures/texture-pack.json': b'{"delivery":"bundled"}'}
    for name, data in files.items():
        path = tmp_path/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    sources = [*files, '', 'src/game.c', 'deleted.c',
               *(p.relative_to(tmp_path).as_posix() for p in pack.iterdir())]
    code = io.BytesIO()
    with zipfile.ZipFile(code, 'w') as archive:
        textures.write_source_files(archive, tmp_path, sources)
        archive.writestr('runtime-local.patch', 'runtime patch')
    package = io.BytesIO()
    with zipfile.ZipFile(package, 'w') as archive:
        archive.writestr('Game/Source/pc-port-source.zip', code.getvalue())
        textures.write_zip(archive, 'Game/', records)
    with zipfile.ZipFile(package) as archive:
        assert set(archive.namelist()) == {
            'Game/Source/pc-port-source.zip', *("Game/"+name for name, _, _ in textures.zip_entries(records))}
        with zipfile.ZipFile(io.BytesIO(archive.read('Game/Source/pc-port-source.zip'))) as code:
            assert set(code.namelist()) == {*files, 'runtime-local.patch'}
            for name, data in files.items():
                assert code.read(name) == data
        for name, _, sha in textures.zip_entries(records):
            assert hashlib.sha256(archive.read('Game/'+name)).hexdigest() == sha


@pytest.mark.parametrize('existing_pointer', [False, True])
def test_repository_export_tracks_only_audited_public_content(tmp_path, existing_pointer):
    import importlib.util
    import subprocess
    spec = importlib.util.spec_from_file_location('repository_textures', ROOT/'tools/textures/texture-repository.py')
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    source, _, _, _, _ = fixture(tmp_path, dimension=64)
    repo = tmp_path/'repo'
    repo.mkdir()
    (repo/'config/textures').mkdir(parents=True)
    subprocess.run(['git', 'init', str(repo)], check=True, capture_output=True)
    if subprocess.run(['git', 'lfs', 'version'], capture_output=True).returncode:
        pytest.skip('Git LFS unavailable')
    if existing_pointer:
        destination = repo/'assets/textures/0123456789abcdef.dds'
        destination.parent.mkdir(parents=True)
        destination.write_text('version https://git-lfs.github.com/spec/v1\n'
                               'oid sha256:'+'0'*64+'\nsize 2872\n')
    exporter.stage(source, root=repo, pointers_only=True)
    pointer = (repo/'assets/textures/0123456789abcdef.dds').read_text()
    assert pointer.startswith('version https://git-lfs.github.com/spec/v1')
    assert textures.RELEASE.digest(source/'0123456789abcdef.dds') in pointer
    public = json.loads((repo/'assets/textures/0123456789abcdef.json').read_text())
    assert 'source' not in public and 'prompt' not in public
    assert json.loads((repo/'config/textures/texture-pack.json').read_text())['texture_count'] == 1
    with pytest.raises(ValueError, match='git lfs pull'):
        textures.ensure_pack(repo)


def test_retirement_policy_travels_with_pack_and_rejects_reintroduction(tmp_path):
    source, _, _, _, _ = fixture(tmp_path)
    (tmp_path/'config/textures').mkdir(parents=True)
    policy = {'keys': ['fedcba9876543210'], 'retired': {
        'fedcba9876543210': {'dds': 'a'*64, 'json': 'b'*64}}}
    path = tmp_path/'config/textures/texture-originals.json'
    path.write_text(json.dumps(policy))
    _, _, identity = textures.ensure_pack(tmp_path, source)
    assert identity['retired'] == policy['retired']
    policy['retired']['fedcba9876543210']['dds'] = 'invalid'
    path.write_text(json.dumps(policy))
    with pytest.raises(ValueError, match='Invalid retired'):
        textures.ensure_pack(tmp_path, source)
    policy = {'keys': ['0123456789abcdef'], 'retired': {}}
    path.write_text(json.dumps(policy))
    with pytest.raises(ValueError, match='Retained-original'):
        textures.ensure_pack(tmp_path, source)


def test_optional_identity_preserves_retirement_policy_without_assets(tmp_path):
    assert textures.optional_identity(tmp_path) is None
    config = tmp_path/'config/textures'
    config.mkdir(parents=True)
    key = '0123456789abcdef'
    pair = {'dds': 'a'*64, 'json': 'b'*64}
    policy = config/'texture-originals.json'
    policy.write_text(json.dumps({'keys': [key], 'retired': {key: pair}}))
    assert textures.optional_identity(tmp_path) == {'delivery': 'optional', 'retired': {key: pair}}
    with pytest.raises(ValueError, match='Retained-original'):
        textures.retired_textures(tmp_path, [key])
    pair['dds'] = 'bad'
    policy.write_text(json.dumps({'keys': [key], 'retired': {key: pair}}))
    with pytest.raises(ValueError, match='retired texture identity'):
        textures.optional_identity(tmp_path)
