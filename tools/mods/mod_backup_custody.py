"""Independent backup recipient custody and recoverable encrypted-file admission.

Preparation creates only a local DPAPI-protected key. Completion requires an
actual portable-file decrypt round trip; it never enables a service or task.
"""
import hashlib
from itertools import islice
import os
from pathlib import Path
import uuid

import mod_backup_recipient as recipient
import mod_windows_recovery as windows
from mod_package import read_json, require, SHA
from mod_portable_recovery import password_bytes
from mod_trust import encode

TYPE = 'tsfp-backup-recipient-custody-v1'
PREPARATION = 'preparation.json'
STORE = 'recipient'
RECOVERY = 'recipient-recovery.enc.json'
COMPLETE = 'custody.json'
MAX_META = 8192


def record(data):
    return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def prepare(directory, service_origin, pin):
    require(os.name == 'nt', 'Prepare backup key custody on Windows')
    recipient.profile(service_origin, pin)
    directory = windows.private(Path(directory).absolute(), create=True)
    public = recipient.create_windows(directory / STORE, service_origin, pin)
    payloads = {name: record(windows.read_file(directory / STORE / name)) for name in sorted(recipient.FILES)}
    value = dict(schema=1, type=TYPE, custody_id=uuid.uuid4().hex, origin=service_origin,
                 trusted_root_sha256=pin, recipient=public, store=payloads)
    wire = encode(value)
    require(len(wire) <= MAX_META, 'Backup key preparation exceeds budget')
    windows.new_file(directory / PREPARATION, wire)
    return dict(schema=1, phase='backup-key-prepared', preparation_sha256=record(wire)['sha256'],
                key_id=public['key_id'], directory=str(directory), portable_verified=False, services_started=False)


def opened(directory, expected_sha256, service_origin, pin):
    require(os.name == 'nt', 'Manage local backup key custody on Windows')
    recipient.profile(service_origin, pin)
    require(isinstance(expected_sha256, str) and SHA.fullmatch(expected_sha256), 'Supply the recorded key preparation SHA256')
    directory = windows.private(Path(directory).absolute())
    names = set(p.name for p in islice(directory.iterdir(), 5))
    require({PREPARATION, STORE} <= names <= {PREPARATION, STORE, RECOVERY, COMPLETE}
            and (COMPLETE not in names or RECOVERY in names), 'Preserving partial or foreign key custody state')
    wire = windows.read_file(directory / PREPARATION, MAX_META)
    require(record(wire)['sha256'] == expected_sha256, 'Backup key preparation differs')
    value = read_json(wire)
    require(isinstance(value, dict) and set(value) == {'schema', 'type', 'custody_id', 'origin', 'trusted_root_sha256', 'recipient', 'store'}
            and type(value['schema']) is int and value['schema'] == 1 and value['type'] == TYPE
            and isinstance(value['custody_id'], str) and len(value['custody_id']) == 32
            and all(c in '0123456789abcdef' for c in value['custody_id']) and encode(value) == wire
            and value['origin'] == service_origin and value['trusted_root_sha256'] == pin, 'Backup key custody authority differs')
    recipient.checked_public(value['recipient'], service_origin, pin)
    require(isinstance(value['store'], dict) and set(value['store']) == recipient.FILES, 'Unknown account key inventory')
    for name in sorted(recipient.FILES):
        require(record(windows.read_file(directory / STORE / name)) == value['store'][name], 'Account key store changed')
    private = recipient.load_windows(directory / STORE, service_origin, pin)
    require(recipient.public(private, service_origin, pin) == value['recipient'], 'Account key identity differs')
    return directory, value, private


def completion(directory, value, preparation_sha256):
    wire = windows.read_file(directory / COMPLETE, MAX_META)
    result = read_json(wire)
    encrypted = windows.read_file(directory / RECOVERY, recipient.MAX_WIRE)
    expected = dict(schema=1, type=TYPE, phase='portable-roundtrip-verified', custody_id=value['custody_id'],
                    preparation_sha256=preparation_sha256, key_id=value['recipient']['key_id'], recovery=record(encrypted))
    require(wire == encode(expected), 'Backup key completion or encrypted recovery file changed')
    return expected, encrypted


def summary(value, result):
    return dict(schema=1, phase='backup-key-custody-verified', custody_id=value['custody_id'],
                key_id=value['recipient']['key_id'], recovery_sha256=result['recovery']['sha256'],
                recovery_bytes=result['recovery']['bytes'], portable_verified=True,
                cross_host_verified=False, services_started=False)


def inspect(directory, preparation_sha256, service_origin, pin):
    directory, value, _ = opened(directory, preparation_sha256, service_origin, pin)
    result, _ = completion(directory, value, preparation_sha256)
    return summary(value, result)


def finish(directory, preparation_sha256, service_origin, pin, password, confirmation):
    password_bytes(password)
    require(isinstance(confirmation, str) and password == confirmation, 'Recovery passphrases differ')
    directory, value, private = opened(directory, preparation_sha256, service_origin, pin)
    expected_secret = recipient.secret(private, service_origin, pin)
    if (directory / COMPLETE).exists():
        result, encrypted = completion(directory, value, preparation_sha256)
    else:
        if not (directory / RECOVERY).exists():
            encrypted = recipient.seal(private, password, service_origin, pin)
            recovered = recipient.unseal(encrypted, password, service_origin, pin)
            require(recipient.secret(recovered, service_origin, pin) == expected_secret, 'Portable key self-check failed')
            windows.new_file(directory / RECOVERY, encrypted)
        # On interruption after ciphertext creation, authenticate that exact
        # existing file. Never silently replace it with a new password/key.
        encrypted = windows.read_file(directory / RECOVERY, recipient.MAX_WIRE)
        result = dict(schema=1, type=TYPE, phase='portable-roundtrip-verified', custody_id=value['custody_id'],
                      preparation_sha256=preparation_sha256, key_id=value['recipient']['key_id'], recovery=record(encrypted))
    recovered = recipient.unseal(encrypted, password, service_origin, pin)
    require(recipient.secret(recovered, service_origin, pin) == expected_secret, 'Portable recovery returns a different backup key')
    # Revalidate account files and ciphertext before committing the final marker.
    _, current, current_key = opened(directory, preparation_sha256, service_origin, pin)
    require(current == value and recipient.secret(current_key, service_origin, pin) == expected_secret
            and record(windows.read_file(directory / RECOVERY, recipient.MAX_WIRE)) == result['recovery'],
            'Backup key custody changed during verification')
    if not (directory / COMPLETE).exists():
        windows.new_file(directory / COMPLETE, encode(result))
    return inspect(directory, preparation_sha256, service_origin, pin)


def check_portable(file, service_origin, pin, recovery_sha256, key_id, password):
    """Independent-account proof: no Windows store, DPAPI or plaintext output."""
    recipient.profile(service_origin, pin)
    require(isinstance(recovery_sha256, str) and SHA.fullmatch(recovery_sha256)
            and isinstance(key_id, str) and SHA.fullmatch(key_id), 'Supply recorded recovery/key fingerprints')
    from mod_backup_cipher import incoming
    with incoming(file, recipient.MAX_WIRE) as (stream, length, stable):
        encrypted = stream.read(recipient.MAX_WIRE + 1)
        require(len(encrypted) == length and record(encrypted)['sha256'] == recovery_sha256,
                'Encrypted backup key fingerprint differs')
        stable()
    private = recipient.unseal(encrypted, password, service_origin, pin)
    require(recipient.public(private, service_origin, pin)['key_id'] == key_id, 'Recovered backup key identity differs')
    return dict(schema=1, phase='portable-backup-key-verified', key_id=key_id, recovery_sha256=recovery_sha256,
                account_independent=True, services_started=False)
