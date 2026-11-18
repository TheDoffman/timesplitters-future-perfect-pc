"""Immutable private bundles, authenticated byte boundaries and incomplete output."""
import io
import os
from pathlib import Path
import struct
import sys
from types import SimpleNamespace

import pytest
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_backup_exports as exports
import mod_backup_cipher as cipher
import mod_backup_recipient as recipients
from mod_package import ModError
from mod_trust import encode

ORIGIN = 'https://export-fixture.example.test:8443'
PIN = 'a' * 64


@pytest.fixture(scope='module')
def case(tmp_path_factory):
    root = tmp_path_factory.mktemp('backup-exports').resolve()
    if os.name == 'nt':
        from mod_windows_recovery import private
        root = private(root / 'private', create=True)
    else:
        root.chmod(0o700)
    key = X25519PrivateKey.generate()
    public = recipients.public(key, ORIGIN, PIN)
    folder = exports.new_directory(root / 'source')
    data = (bytes(range(251)) * 5000)[:cipher.CHUNK + 19]
    plain = root / 'plaintext'
    plain.write_bytes(data)
    if os.name == 'posix':
        plain.chmod(0o600)
    result = cipher.encrypt_file(plain, folder / exports.PAYLOAD, public, ORIGIN, PIN,
                                 plain_bytes=len(data), plain_sha256=cipher.digest(data))
    receipt = dict(schema=1, type=exports.TYPE, export_id='1' * 32, created=1700000000,
                   origin=ORIGIN, trusted_root_sha256=PIN, key_id=public['key_id'],
                   cipher_bytes=result['bytes'], cipher_sha256=result['cipher_sha256'])
    wire = encode(receipt)
    exports.write_receipt(folder, wire)
    payload = (folder / exports.PAYLOAD).read_bytes()
    frame = exports.MAGIC + struct.pack('>I', len(wire)) + wire + payload
    return SimpleNamespace(root=root, folder=folder, public=public, key=key,
                           receipt=receipt, frame=frame, payload=payload, plain=data)


def test_complete_fragmented_transfer_roundtrip_and_exact_two_file_inventory(case):
    class Fragmented(io.BytesIO):
        def read(self, length):
            assert 0 < length <= cipher.CHUNK
            return super().read(min(length, 4093))
    output = case.root / 'complete'
    called = []
    receipt = exports.receive_stream(Fragmented(case.frame), output, case.public, ORIGIN, PIN,
                                      verify_transport=lambda: called.append(True))
    assert called == [True] and receipt == case.receipt
    assert {path.name for path in output.iterdir()} == {exports.PAYLOAD, exports.RECEIPT}
    assert exports.inspect_export(output, case.public, ORIGIN, PIN)[2] == case.receipt
    recovered = case.root / 'decrypted'
    cipher.decrypt_file(output / exports.PAYLOAD, recovered, case.key, ORIGIN, PIN,
                        cipher_sha256=receipt['cipher_sha256'])
    assert recovered.read_bytes() == case.plain
    sent = io.BytesIO()
    exports.send_stream(output, sent, case.public, ORIGIN, PIN)
    assert sent.getvalue() == case.frame


@pytest.mark.parametrize('fault', ['magic', 'receipt-limit', 'noncanonical', 'duplicate', 'origin',
                                 'pin', 'key', 'schema-bool', 'size-bool', 'extra-field',
                                 'truncated', 'trailing', 'corrupt', 'size', 'cipher-header', 'transport'])
def test_bad_transfer_never_commits_receipt_and_preserves_source(case, fault):
    value = dict(case.receipt)
    payload = case.payload
    if fault == 'origin':
        value['origin'] = 'https://other.example.test'
    elif fault == 'pin':
        value['trusted_root_sha256'] = 'b' * 64
    elif fault == 'key':
        value['key_id'] = 'b' * 64
    elif fault == 'schema-bool':
        value['schema'] = True
    elif fault == 'size-bool':
        value['cipher_bytes'] = True
    elif fault == 'extra-field':
        value['plaintext_hash'] = 'b' * 64
    elif fault == 'size':
        value['cipher_bytes'] += 1
    elif fault == 'corrupt':
        payload = payload[:-1] + bytes([payload[-1] ^ 1])
    elif fault == 'cipher-header':
        payload = b'UNKNOWN!' + payload[8:]
        value['cipher_sha256'] = cipher.digest(payload)
    wire = encode(value)
    if fault == 'noncanonical':
        wire = b' ' + wire
    elif fault == 'duplicate':
        wire = b'{"schema":1,' + wire[1:]
    frame = exports.MAGIC + struct.pack('>I', len(wire)) + wire + payload
    if fault == 'magic':
        frame = b'UNKNOWN!' + frame[8:]
    elif fault == 'receipt-limit':
        frame = exports.MAGIC + struct.pack('>I', exports.MAX_RECEIPT + 1) + frame[12:]
    elif fault == 'truncated':
        frame = frame[:-1]
    elif fault == 'trailing':
        frame += b'extra'
    def verify():
        if fault == 'transport':
            raise ModError('Sender failed after producing bytes')
    output = case.root / ('bad-' + fault)
    with pytest.raises(ModError):
        exports.receive_stream(io.BytesIO(frame), output, case.public, ORIGIN, PIN, verify_transport=verify)
    assert not (output / exports.RECEIPT).exists()
    if output.exists():
        with pytest.raises(ModError, match='incomplete'):
            exports.inspect_export(output, case.public, ORIGIN, PIN)
    assert (case.folder / exports.PAYLOAD).read_bytes() == case.payload


def test_existing_destination_and_low_space_are_preserved_before_output(case, monkeypatch):
    existing = exports.new_directory(case.root / 'existing')
    (existing / 'keep').write_bytes(b'preserved')
    with pytest.raises((OSError, ModError)):
        exports.receive_stream(io.BytesIO(case.frame), existing, case.public, ORIGIN, PIN,
                               verify_transport=lambda: None)
    assert (existing / 'keep').read_bytes() == b'preserved'
    monkeypatch.setattr(exports.shutil, 'disk_usage', lambda path: SimpleNamespace(free=0))
    output = case.root / 'low-space'
    with pytest.raises(ModError, match='headroom'):
        exports.receive_stream(io.BytesIO(case.frame), output, case.public, ORIGIN, PIN,
                               verify_transport=lambda: None)
    assert not output.exists()


def test_no_completion_callback_and_short_sender_write_refused(case):
    with pytest.raises(ModError, match='completion'):
        exports.receive_stream(io.BytesIO(case.frame), case.root / 'no-callback', case.public,
                               ORIGIN, PIN, verify_transport=None)
    class Short(io.BytesIO):
        def write(self, data):
            return super().write(data[:-1])
    with pytest.raises(ModError, match='Short'):
        exports.send_stream(case.folder, Short(), case.public, ORIGIN, PIN)


@pytest.mark.parametrize('extra', ['unexpected', 'missing'])
def test_inspection_preserves_foreign_or_incomplete_inventory(case, extra):
    folder = exports.new_directory(case.root / ('inventory-' + extra))
    (folder / exports.PAYLOAD).write_bytes(case.payload)
    if os.name == 'posix':
        (folder / exports.PAYLOAD).chmod(0o600)
    if extra == 'unexpected':
        exports.write_receipt(folder, encode(case.receipt))
        (folder / 'keep').write_bytes(b'preserved')
    before = {path.name for path in folder.iterdir()}
    with pytest.raises(ModError, match='incomplete or foreign'):
        exports.inspect_export(folder, case.public, ORIGIN, PIN)
    assert {path.name for path in folder.iterdir()} == before


@pytest.mark.skipif(os.name != 'posix', reason='Actual POSIX snapshot producer')
def test_real_snapshot_export_reception_and_conservative_restore(case):
    import mod_backup as backup
    from mod_repository import Repository
    root = exports.new_directory(case.root / 'host')
    repository = Repository(root / 'repository')
    repository.root.chmod(0o700)
    backup.snapshot(repository.root, root / 'snapshot')
    receipt = exports.export_snapshot(root / 'snapshot', root / 'export', case.public, ORIGIN, PIN)
    assert 'plain_sha256' not in receipt and 'snapshot' not in receipt
    assert exports.inspect_export(root / 'export', case.public, ORIGIN, PIN)[2] == receipt
    report = cipher.decrypt_file(root / 'export' / exports.PAYLOAD, root / 'decrypted',
                                 case.key, ORIGIN, PIN, cipher_sha256=receipt['cipher_sha256'])
    exports.archive.unpack_snapshot(root / 'decrypted', root / 'extracted', plain_sha256=report['plain_sha256'])
    backup.restore(root / 'extracted', root / 'restored')
    assert (root / 'restored' / backup.BARRIER).is_file()
    with pytest.raises(ModError, match='separate'):
        exports.export_snapshot(root / 'snapshot', root / 'snapshot' / 'inside', case.public, ORIGIN, PIN)
    assert not (root / 'snapshot' / 'inside').exists()
