"""Portable threshold-key recovery, wrong-password/tamper gates and private IO."""
import hashlib
import os
from pathlib import Path
import secrets
import sys
import time

import pytest
from cryptography.hazmat.primitives import serialization

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_portable_recovery as portable
from mod_package import ModError
from mod_trust import b64, encode, make_root, read_json

PASSWORD = 'Disposable recovery fixture only!'


@pytest.fixture(scope='module')
def payloads(trust_keys):
    roots, online = trust_keys[:2], trust_keys[4]
    wire = make_root(roots, [online], 1, 'https://workshop.example.test:8443', int(time.time()))
    pin = hashlib.sha256(wire).hexdigest()
    passwords = {name: secrets.token_bytes(32) for name in ('root-a', 'root-b')}
    values = {name+'/private.pem': key.private_bytes(serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8, serialization.BestAvailableEncryption(passwords[name]))
        for name, key in zip(('root-a', 'root-b'), roots)}
    values.update({'host-input/trusted-root.json': wire,
        'host-input/origin.json': encode(dict(schema=1, origin='https://workshop.example.test:8443', trusted_root_sha256=pin)),
        'host-input/catalog-1.pem': online.private_bytes(serialization.Encoding.PEM,
            serialization.PrivateFormat.PKCS8, serialization.NoEncryption()),
        'passwords.json': encode(dict(schema=1, producer='tsfp-windows-root-ceremony-v1',
            origin='https://workshop.example.test:8443', trusted_root_sha256=pin,
            passwords={k: b64(v) for k, v in passwords.items()}))})
    return values, pin


def test_portable_authenticated_roundtrip_preserves_exact_keys_and_proves_threshold(payloads):
    values, pin = payloads
    wire = portable.seal(values, PASSWORD, pin)
    assert all(v not in wire for v in values.values())
    restored, proof = portable.unseal(wire, PASSWORD, pin)
    assert restored == values
    assert proof['phase'] == 'portable-recovery-verified' and proof['trusted_root_sha256'] == pin
    assert proof['account_bound'] is proof['network_started'] is proof['services_started'] is False
    assert proof['root_valid_now'] is True
    second = portable.seal(values, PASSWORD, pin)
    assert second != wire and portable.unseal(second, PASSWORD, pin)[0] == values


def test_wrong_password_tampering_wrong_pin_and_unbounded_kdf_are_refused(payloads, monkeypatch):
    values, pin = payloads
    wire = portable.seal(values, PASSWORD, pin)
    with pytest.raises(ModError, match='incorrect'): portable.unseal(wire, PASSWORD+'wrong', pin)
    doc = read_json(wire)
    for field in ('salt', 'nonce', 'ciphertext'):
        damaged = dict(doc)
        data = bytearray(portable.unb64(doc[field], portable.MAX_WIRE)); data[0] ^= 1
        damaged[field] = b64(data)
        with pytest.raises(ModError): portable.unseal(encode(damaged), PASSWORD, pin)
    with pytest.raises(ModError, match='pin differs'): portable.unseal(wire, PASSWORD, 'f'*64)
    def forbidden(*_): pytest.fail('Untrusted KDF parameters must be rejected before KDF allocation')
    monkeypatch.setattr(portable, 'derive', forbidden)
    for field, value in (('kdf', 'scrypt-n999999999-r8-p1'), ('schema', True), ('extra', 'unknown'), ('nonce', 'AA==')):
        damaged = dict(doc); damaged[field] = value
        with pytest.raises(ModError): portable.unseal(encode(damaged), PASSWORD, pin)
    with pytest.raises(ModError): portable.unseal(b'x'*(portable.MAX_WIRE+1), PASSWORD, pin)


@pytest.mark.parametrize('change', ['missing', 'extra', 'root-swap', 'catalog', 'passwords', 'origin'])
def test_recovery_inventory_and_authority_must_match_before_export(payloads, change):
    values, pin = payloads; values = dict(values)
    if change == 'missing': del values['root-b/private.pem']
    elif change == 'extra': values['../escape'] = b'forbidden'
    elif change == 'root-swap': values['root-a/private.pem'], values['root-b/private.pem'] = values['root-b/private.pem'], values['root-a/private.pem']
    elif change == 'catalog': values['host-input/catalog-1.pem'] = b'not a key'
    elif change == 'passwords': values['passwords.json'] = encode(dict(passwords={}))
    elif change == 'origin': values['host-input/origin.json'] = encode(dict(schema=1, origin='https://other.example.test', trusted_root_sha256=pin))
    with pytest.raises((ModError, ValueError)): portable.seal(values, PASSWORD, pin)


@pytest.mark.parametrize('password', ['', 'short', 'x'*257, b'not text'])
def test_weak_or_unbounded_passphrases_refused(password):
    with pytest.raises(ModError): portable.password_bytes(password)


def test_authenticated_but_unknown_inner_inventory_is_refused(payloads):
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    values, pin = payloads
    inner = dict(schema=1, type=portable.TYPE, files={k: b64(v) for k, v in values.items()})
    inner['files']['../escape'] = b64(b'no extraction allowed')
    salt, nonce = secrets.token_bytes(32), secrets.token_bytes(12)
    encrypted = AESGCM(portable.derive(PASSWORD, salt)).encrypt(nonce, encode(inner), portable.AAD)
    wire = encode(dict(schema=1, type=portable.TYPE, kdf=portable.KDF, cipher='aes256gcm',
                       salt=b64(salt), nonce=b64(nonce), ciphertext=b64(encrypted)))
    with pytest.raises(ModError, match='inventory'): portable.unseal(wire, PASSWORD, pin)


def test_expired_root_can_be_recovered_but_is_reported_unready(payloads, monkeypatch):
    values, pin = payloads
    root = portable.root_document(values['host-input/trusted-root.json'])
    monkeypatch.setattr(portable.time, 'time', lambda: root['expires']+1)
    proof = portable.validate(values, pin)
    assert proof['root_valid_now'] is False


def test_private_new_only_export_io_and_links(tmp_path, payloads):
    values, pin = payloads; wire = portable.seal(values, PASSWORD, pin)
    if os.name == 'nt':
        import mod_windows_recovery as recovery
        parent = recovery.private(tmp_path/'private', create=True)
    else:
        parent = tmp_path; parent.chmod(0o700)
    path = parent/'recovery.bin'
    portable.write_export(path, wire)
    assert portable.read_export(path) == wire
    with pytest.raises(FileExistsError): portable.write_export(path, b'preserve')
    linked = parent/'alias.bin'; os.link(path, linked)
    with pytest.raises(ModError): portable.read_export(path)
    assert path.read_bytes() == wire


@pytest.mark.skipif(os.name != 'nt', reason='Actual Windows DPAPI export/import')
def test_windows_import_rebinds_dpapi_and_preserves_source_existing_and_failed_stores(tmp_path, payloads):
    values, pin = payloads; wire = portable.seal(values, PASSWORD, pin)
    destination = tmp_path/'recovered'
    with pytest.raises(ModError): portable.import_windows(wire, PASSWORD+'wrong', pin, destination)
    assert not destination.exists()
    with pytest.raises(ModError): portable.import_windows(wire, PASSWORD, 'f'*64, destination)
    assert not destination.exists()
    proof = portable.import_windows(wire, PASSWORD, pin, destination)
    assert proof['account_bound'] is True and proof['phase'] == 'recovery-verified'
    exported = portable.export_windows(destination, PASSWORD, pin)
    assert portable.unseal(exported, PASSWORD, pin)[0] == values
    before = {p: p.read_bytes() for p in destination.rglob('*') if p.is_file()}
    with pytest.raises(ModError): portable.import_windows(wire, PASSWORD, pin, destination)
    assert before == {p: p.read_bytes() for p in before}
