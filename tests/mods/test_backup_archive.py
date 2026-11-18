"""Real private snapshot export/recovery, bounded framing and interruption refusal."""
from contextlib import contextmanager
import hashlib
import os
from pathlib import Path
import struct
import sys

import pytest
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_backup as backup
import mod_backup_archive as archive
import mod_backup_cipher as cipher
import mod_backup_recipient as recipients
from mod_package import ModError
from mod_repository import Repository
from mod_trust import encode

pytestmark = pytest.mark.skipif(os.name != 'posix', reason='Mac/POSIX snapshot export/recovery')
ORIGIN = 'https://archive-fixture.example.test:8443'
PIN = 'a' * 64


@pytest.fixture
def case(tmp_path):
    root = tmp_path.resolve()
    root.chmod(0o700)
    repository = Repository(root / 'repository')
    repository.root.chmod(0o700)
    seed = b'backup opaque blob fixture'
    data = (seed * ((cipher.CHUNK + 17 + len(seed) - 1) // len(seed)))[:cipher.CHUNK + 17]
    sha = hashlib.sha256(data).hexdigest()
    write(repository.root / 'blobs' / (sha + '.zip'), data)
    with repository.db() as db:
        db.execute('INSERT INTO releases VALUES(?,?,?,?,?,?)',
                   (sha, 'fixture', '1.0.0', '{}', 'approved', '{}'))
    snapshot = root / 'snapshot'
    backup.snapshot(repository.root, snapshot)
    key = X25519PrivateKey.generate()
    return root, snapshot, key, recipients.public(key, ORIGIN, PIN)


def write(path, data):
    path.write_bytes(data)
    path.chmod(0o600)


def packed(case):
    _, source, _, _ = case
    _, manifest, _ = backup.backup_manifest(source)
    return b''.join(archive.chunks(source, manifest))


def test_real_snapshot_stream_encrypt_decrypt_unpack_restore_keeps_barrier(case):
    root, source, key, public = case
    encrypted = root / 'encrypted'
    report = archive.encrypt_snapshot(source, encrypted, public, ORIGIN, PIN)
    assert sorted(path.name for path in root.iterdir()) == ['encrypted', 'repository', 'snapshot']
    plain = root / 'decrypted'
    result = cipher.decrypt_file(encrypted, plain, key, ORIGIN, PIN, cipher_sha256=report['cipher_sha256'])
    extracted = root / 'extracted'
    proof = archive.unpack_snapshot(plain, extracted, plain_sha256=result['plain_sha256'])
    assert proof['manifest_sha256'] == report['snapshot']['manifest_sha256']
    restored = root / 'restored'
    backup.restore(extracted, restored)
    assert (restored / backup.BARRIER).is_file()
    assert backup.verify_snapshot(source) == report['snapshot']
    assert plain.read_bytes() == packed(case)
    assert plain.stat().st_size > cipher.CHUNK
    for member in extracted.rglob('*'):
        assert member.stat().st_mode & 0o077 == 0


@pytest.mark.parametrize('fault', ['magic', 'traversal', 'duplicate', 'noncanonical', 'limit', 'truncated', 'trailing', 'hash'])
def test_malformed_or_wrong_receipt_archive_refused_before_output(case, fault):
    root, source, _, _ = case
    wire = packed(case)
    prefix = len(archive.MAGIC) + 4
    length = struct.unpack('>I', wire[len(archive.MAGIC):prefix])[0]
    manifest = cipher.read_json(wire[prefix:prefix + length])
    if fault == 'magic':
        wire = b'UNKNOWN!' + wire[8:]
    elif fault == 'traversal':
        manifest['files']['../escape'] = manifest['files']['catalog.sqlite']
        data = encode(manifest)
        wire = archive.MAGIC + struct.pack('>I', len(data)) + data + wire[prefix + length:]
    elif fault == 'duplicate':
        data = b'{"schema":1,' + wire[prefix + 1:prefix + length]
        wire = archive.MAGIC + struct.pack('>I', len(data)) + data + wire[prefix + length:]
    elif fault == 'noncanonical':
        data = b' ' + wire[prefix:prefix + length]
        wire = archive.MAGIC + struct.pack('>I', len(data)) + data + wire[prefix + length:]
    elif fault == 'limit':
        wire = archive.MAGIC + struct.pack('>I', archive.MAX_MANIFEST + 1) + wire[prefix:]
    elif fault == 'truncated':
        wire = wire[:-1]
    elif fault == 'trailing':
        wire += b'extra'
    plain = root / 'archive'
    write(plain, wire)
    output = root / 'absent'
    with pytest.raises((ModError, ValueError)):
        archive.unpack_snapshot(plain, output, plain_sha256='b' * 64 if fault == 'hash' else cipher.digest(wire))
    assert not output.exists() and not (root / 'escape').exists()


def test_corrupt_member_never_gets_completion_manifest(case):
    root, _, _, _ = case
    wire = bytearray(packed(case))
    wire[-1] ^= 1
    plain = root / 'archive'
    write(plain, wire)
    output = root / 'partial'
    with pytest.raises(ModError, match='integrity'):
        archive.unpack_snapshot(plain, output, plain_sha256=cipher.digest(wire))
    assert output.exists() and not (output / backup.MANIFEST).exists()


def test_changed_snapshot_between_digest_and_encryption_preserves_partial(case, monkeypatch):
    root, source, _, public = case
    original = cipher.encrypt_stream

    def change(*args, **kwargs):
        path = source / 'catalog.sqlite'
        data = bytearray(path.read_bytes())
        data[-1] ^= 1
        write(path, data)
        return original(*args, **kwargs)

    monkeypatch.setattr(cipher, 'encrypt_stream', change)
    output = root / 'incomplete-encrypted'
    with pytest.raises(ModError, match='differs'):
        archive.encrypt_snapshot(source, output, public, ORIGIN, PIN)
    assert output.exists()
    with pytest.raises(ModError):
        cipher.decrypt_file(output, root / 'no-completed-recovery', case[2], ORIGIN, PIN,
                            cipher_sha256=cipher.digest(output.read_bytes()))


def test_extraction_refuses_existing_and_disk_floor_preserving_files(case, monkeypatch):
    root, _, _, _ = case
    plain = root / 'archive'
    data = packed(case)
    write(plain, data)
    output = root / 'existing'
    output.mkdir(mode=0o700)
    write(output / 'keep', b'unchanged')
    with pytest.raises(ModError):
        archive.unpack_snapshot(plain, output, plain_sha256=cipher.digest(data))
    assert (output / 'keep').read_bytes() == b'unchanged'
    disk = archive.shutil.disk_usage(root)
    monkeypatch.setattr(archive.shutil, 'disk_usage', lambda path: type(disk)(disk.total, disk.used, 0))
    with pytest.raises(ModError, match='headroom'):
        archive.unpack_snapshot(plain, root / 'no-space', plain_sha256=cipher.digest(data))
    assert not (root / 'no-space').exists()


def test_reader_never_accepts_unbounded_reads_and_detects_manifest_change(case):
    _, source, _, _ = case
    _, manifest, _ = backup.backup_manifest(source)
    reader = archive.Reader(source, manifest)
    for size in (-1, 0, True, cipher.CHUNK + 1):
        with pytest.raises(ModError):
            reader.read(size)
    with pytest.raises(ModError):
        reader.stable()
    altered = dict(manifest, created=manifest['created'] + 1)
    write(source / backup.MANIFEST, encode(altered))
    with pytest.raises(ModError, match='inventory changed'):
        reader.read(1)
    reader.close()


def test_valid_archive_swap_after_first_hash_cannot_commit_inventory(case, monkeypatch):
    root, _, _, _ = case
    wire = packed(case)
    prefix = len(archive.MAGIC) + 4
    length = struct.unpack('>I', wire[len(archive.MAGIC):prefix])[0]
    manifest = cipher.read_json(wire[prefix:prefix + length])
    manifest['created'] += 1
    changed = archive.header(manifest)[0] + wire[prefix + length:]
    assert len(changed) == len(wire)
    plain = root / 'archive'
    write(plain, wire)
    real = cipher.incoming

    @contextmanager
    def replace_after_hash(path, maximum):
        with real(path, maximum) as (stream, size, stable):
            class Swap:
                def read(self, count):
                    return stream.read(count)

                def seek(self, offset):
                    write(plain, changed)
                    return stream.seek(offset)
            # Model cached timestamps, keeping the actual-consumed hash mandatory.
            yield Swap(), size, lambda: None

    monkeypatch.setattr(cipher, 'incoming', replace_after_hash)
    output = root / 'partial-swap'
    with pytest.raises(ModError, match='changed after receipt'):
        archive.unpack_snapshot(plain, output, plain_sha256=cipher.digest(wire))
    assert output.exists() and not (output / backup.MANIFEST).exists()


def test_unknown_database_schema_cannot_commit_inventory(case):
    import sqlite3
    root, source, _, _ = case
    with sqlite3.connect(source / 'catalog.sqlite') as db:
        db.execute('CREATE TABLE unknown_private_schema (x)')
    path = source / backup.MANIFEST
    manifest = cipher.read_json(path.read_bytes())
    data = (source / 'catalog.sqlite').read_bytes()
    manifest['files']['catalog.sqlite'] = dict(size=len(data), sha256=cipher.digest(data))
    write(path, encode(manifest))
    wire = packed(case)
    plain = root / 'archive'
    write(plain, wire)
    output = root / 'partial-schema'
    with pytest.raises(ModError, match='Unknown database schema'):
        archive.unpack_snapshot(plain, output, plain_sha256=cipher.digest(wire))
    assert output.exists() and not (output / backup.MANIFEST).exists()
    with pytest.raises(ModError, match='Unknown database schema'):
        archive.encrypt_snapshot(source, root / 'never-encrypted', case[3], ORIGIN, PIN)
    assert not (root / 'never-encrypted').exists()


def test_linked_destination_preserves_outside_folder(case):
    root, _, _, _ = case
    wire = packed(case)
    plain = root / 'archive'
    write(plain, wire)
    outside = root / 'outside'
    outside.mkdir(mode=0o700)
    write(outside / 'keep', b'preserved')
    output = root / 'linked'
    output.symlink_to(outside, target_is_directory=True)
    with pytest.raises(ModError):
        archive.unpack_snapshot(plain, output, plain_sha256=cipher.digest(wire))
    assert sorted(path.name for path in outside.iterdir()) == ['keep']
    assert (outside / 'keep').read_bytes() == b'preserved'
