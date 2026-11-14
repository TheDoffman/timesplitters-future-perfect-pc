"""Real-crypto catalog tampering, root-rotation and persistent export counter tests."""
import base64
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import sys
import time

import pytest
from cryptography.hazmat.primitives import serialization

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_publication
import mod_signing
from mod_package import ModError, digest
from mod_repository import Repository
from mod_trust import (MAX_REVOKED, PublicationSigner, encode, envelope, key_id, make_root,
                       public_key, root_document, sign, trusted_root, verify_catalog)
from test_mod_workshop import approved

ORIGIN = 'https://workshop.example.test'


def authority(keys, now):
    wire = make_root(keys[:2], [public_key(keys[4])], 1, ORIGIN, now - 60)
    return wire, trusted_root(wire, now=now)


def catalog(keys, now, version=1, revoked=(), root_wire=None):
    pinned = root_wire or authority(keys, now)[0]
    raw = encode({'schema': 1, 'generated': now, 'expires': now + 3600, 'releases': []})
    files = PublicationSigner(pinned, [keys[4]]).publish(raw, version, revoked, now)
    return files['v1/trust/catalog'], raw, trusted_root(pinned, now=now)


def payload(wire):
    return json.loads(envelope(wire)[0])


def test_exact_bytes_and_signature_required(trust_keys):
    now = int(time.time())
    wire, raw, root = catalog(trust_keys, now)
    parsed, state = verify_catalog(wire, raw, root, now=now)
    assert parsed['releases'] == [] and state['version'] == 1
    with pytest.raises(ModError, match='bytes changed'):
        verify_catalog(wire, raw + b' ', root, now=now)
    data = payload(wire)
    data['expires'] += 1
    forged = json.loads(wire)
    forged['payload'] = base64.b64encode(encode(data)).decode('ascii')
    with pytest.raises(ModError, match='threshold'):
        verify_catalog(encode(forged), raw, root, now=now)
    with pytest.raises(ModError):
        verify_catalog(raw, raw, root, now=now)  # Unsigned catalog is never accepted.


@pytest.mark.parametrize('field,value', [
    ('origin', 'https://attacker.example.test'), ('root_version', 2), ('schema', True),
    ('version', True), ('version', 0), ('version', 2**53),
    ('generated', 999999999999), ('expires', 0), ('expires', 999999999999),
    ('catalog_length', 0), ('catalog_sha256', 'f' * 64), ('unexpected', 'field'),
], ids=['origin', 'root', 'schema-bool', 'version-bool', 'zero', 'overflow', 'future',
        'expired', 'long-life', 'length', 'digest', 'unknown-field'])
def test_signed_but_invalid_metadata_is_rejected(trust_keys, field, value):
    now = int(time.time())
    wire, raw, root = catalog(trust_keys, now)
    data = payload(wire)
    data[field] = value
    with pytest.raises(ModError):
        verify_catalog(sign(data, [trust_keys[4]]), raw, root, now=now)


def test_expiry_clock_rollback_and_version_equivocation(trust_keys):
    now = int(time.time())
    pinned, root = authority(trust_keys, now)
    first, raw, _ = catalog(trust_keys, now, root_wire=pinned)
    _, initial = verify_catalog(first, raw, root, now=now)
    verify_catalog(first, raw, root, initial, now + 10)  # Same immutable snapshot is valid.
    with pytest.raises(ModError, match='expired'):
        verify_catalog(first, raw, root, now=now + 3600)
    with pytest.raises(ModError, match='Clock'):
        verify_catalog(first, raw, root, dict(initial, observed=now + 10), now=now)
    second, raw2, _ = catalog(trust_keys, now + 1, version=2, root_wire=pinned)
    _, latest = verify_catalog(second, raw2, root, initial, now + 1)
    with pytest.raises(ModError, match='rollback'):
        verify_catalog(first, raw, root, latest, now + 1)
    changed, changed_raw, _ = catalog(trust_keys, now + 2, version=2, root_wire=pinned)
    with pytest.raises(ModError, match='reused'):
        verify_catalog(changed, changed_raw, root, latest, now + 2)


def test_revocations_are_permanent_and_cannot_advertise_revoked_content(trust_keys):
    now = int(time.time())
    pinned, root = authority(trust_keys, now)
    revoked = digest(b'revoked package')
    first, raw, _ = catalog(trust_keys, now, revoked=[revoked], root_wire=pinned)
    _, checkpoint = verify_catalog(first, raw, root, now=now)
    next_wire, next_raw, _ = catalog(trust_keys, now + 1, version=2, root_wire=pinned)
    with pytest.raises(ModError, match='forgot'):
        verify_catalog(next_wire, next_raw, root, checkpoint, now + 1)
    data = payload(first)
    changed = encode({'schema': 1, 'generated': now, 'expires': now + 3600,
                      'releases': [{'sha256': revoked}]})
    data.update(catalog_sha256=digest(changed), catalog_length=len(changed))
    with pytest.raises(ModError, match='revoked package'):
        verify_catalog(sign(data, [trust_keys[4]]), changed, root, now=now)


def test_root_rotation_needs_old_and_new_thresholds_and_contiguous_versions(trust_keys):
    now = int(time.time())
    pinned, root = authority(trust_keys, now)
    successor = make_root(trust_keys[2:4], [public_key(trust_keys[5])], 2, ORIGIN, now,
                          old_root_keys=trust_keys[:2])
    new = trusted_root(pinned, [successor], now)
    assert new['version'] == 2 and new['roles']['catalog'] != root['roles']['catalog']
    raw = encode({'schema': 1, 'generated': now, 'expires': now + 3600, 'releases': []})
    files = PublicationSigner(pinned, [trust_keys[5]], [successor]).publish(raw, 2, [], now)
    verify_catalog(files['v1/trust/catalog'], raw, new, now=now)
    with pytest.raises(ModError, match='threshold'):
        verify_catalog(files['v1/trust/catalog'], raw, root, now=now)
    for signatures in (trust_keys[:2], trust_keys[2:4], [trust_keys[0], trust_keys[2], trust_keys[3]]):
        with pytest.raises(ModError, match='threshold'):
            trusted_root(pinned, [sign(root_document(successor), signatures)], now)
    skipped = dict(root_document(successor), version=3)
    with pytest.raises(ModError, match='sequence'):
        trusted_root(pinned, [sign(skipped, trust_keys[:4])], now)
    wrong_origin = dict(root_document(successor), origin='https://other.example.test')
    with pytest.raises(ModError, match='origin'):
        trusted_root(pinned, [sign(wrong_origin, trust_keys[:4])], now)


def test_expired_old_root_can_only_authorize_live_cross_signed_successor(trust_keys):
    now = int(time.time())
    old = make_root(trust_keys[:2], [trust_keys[4]], 1, ORIGIN, now - 367 * 86400)
    with pytest.raises(ModError, match='expired'):
        trusted_root(old, now=now)
    successor = make_root(trust_keys[2:4], [trust_keys[5]], 2, ORIGIN, now,
                          old_root_keys=trust_keys[:2])
    assert trusted_root(old, [successor], now)['version'] == 2


def test_online_key_cannot_rotate_root_or_impersonate_root_key(trust_keys):
    now = int(time.time())
    pinned, _ = authority(trust_keys, now)
    with pytest.raises(ModError, match='threshold'):
        trusted_root(sign(root_document(pinned), [trust_keys[4]]), now=now)
    data = root_document(pinned)
    data['roles']['root']['keyids'][0] = data['roles']['catalog']['keyids'][0]
    with pytest.raises(ModError, match='distinct'):
        trusted_root(sign(data, trust_keys[:2]), now=now)
    data = root_document(pinned)
    data['roles']['root']['threshold'] = 1
    with pytest.raises(ModError, match='integer'):
        trusted_root(sign(data, trust_keys[:2]), now=now)


@pytest.mark.parametrize('mutation', ['duplicate-signer', 'duplicate-json', 'bad-base64', 'oversized', 'missing-signature'])
def test_envelope_rejects_ambiguous_or_amplified_wire(trust_keys, mutation):
    now = int(time.time())
    wire, raw, root = catalog(trust_keys, now)
    data = json.loads(wire)
    if mutation == 'duplicate-signer':
        data['signatures'] *= 2
        wire = encode(data)
    elif mutation == 'duplicate-json':
        wire = wire[:-1] + b',"payload":"AAAA"}'
    elif mutation == 'bad-base64':
        data['payload'] += '\n'
        wire = encode(data)
    elif mutation == 'oversized':
        wire = b' ' * (2 * 1024 * 1024 + 1)
    else:
        data['signatures'] = []
        wire = encode(data)
    with pytest.raises(ModError):
        verify_catalog(wire, raw, root, now=now)


def test_revocation_budget_and_root_key_parameters(trust_keys):
    now = int(time.time())
    wire, raw, root = catalog(trust_keys, now)
    data = payload(wire)
    data['revoked'] = [digest(str(i).encode()) for i in range(MAX_REVOKED + 1)]
    with pytest.raises(ModError, match='revocation'):
        verify_catalog(sign(data, [trust_keys[4]]), raw, root, now=now)
    data = root_document(authority(trust_keys, now)[0])
    identity = next(iter(data['keys']))
    data['keys'][identity]['exponent'] = 'Aw=='
    with pytest.raises(ModError):
        root_document(sign(data, trust_keys[:2]))


def test_signed_export_restart_sequence_revocation_and_preflight(tmp_path, trust_keys):
    now = int(time.time())
    pinned, root = authority(trust_keys, now)
    signer = PublicationSigner(pinned, [trust_keys[4]])
    repo, package, sha = approved(tmp_path)
    first = tmp_path / 'first'
    mod_publication.export_snapshot(repo, first, signer=signer)
    assert mod_publication.validate_snapshot(first, pinned)['releases'][0]['sha256'] == sha
    wire = (first / 'v1/trust/catalog').read_bytes()
    _, checkpoint = verify_catalog(wire, (first / 'v1/catalog').read_bytes(), root)
    assert checkpoint['version'] == 1
    repo.revoke(sha, 'fixture revocation')
    second = tmp_path / 'second'
    mod_publication.export_snapshot(Repository(repo.root), second, signer=signer)
    assert mod_publication.validate_snapshot(second, pinned, checkpoint)['releases'] == []
    next_wire = (second / 'v1/trust/catalog').read_bytes()
    assert payload(next_wire)['version'] == 2 and payload(next_wire)['revoked'] == [sha]
    assert not (second / 'v1/packages' / sha).exists()
    with pytest.raises(ModError, match='unexpected'):
        mod_publication.validate_snapshot(second)  # Cannot mistake signed tree for unsigned preview.
    (first / 'v1/catalog').write_bytes((first / 'v1/catalog').read_bytes() + b' ')
    with pytest.raises(ModError, match='bytes changed'):
        mod_publication.validate_snapshot(first, pinned)


def test_export_failure_after_reservation_never_reuses_version(tmp_path, trust_keys, monkeypatch):
    now = int(time.time())
    pinned, _ = authority(trust_keys, now)
    signer = PublicationSigner(pinned, [trust_keys[4]])
    repo = Repository(tmp_path / 'repo')
    original = mod_publication.validate_snapshot
    def failed(*args, **kwargs):
        raise ModError('simulated final preflight failure')
    monkeypatch.setattr(mod_publication, 'validate_snapshot', failed)
    with pytest.raises(ModError, match='simulated'):
        mod_publication.export_snapshot(repo, tmp_path / 'failed', signer=signer)
    assert not (tmp_path / 'failed').exists()
    monkeypatch.setattr(mod_publication, 'validate_snapshot', original)
    mod_publication.export_snapshot(repo, tmp_path / 'next', signer=signer)
    assert payload((tmp_path / 'next/v1/trust/catalog').read_bytes())['version'] == 2


def test_concurrent_publishers_reserve_distinct_monotonic_versions(tmp_path, trust_keys):
    pinned, _ = authority(trust_keys, int(time.time()))
    signer = PublicationSigner(pinned, [trust_keys[4]])
    repo = Repository(tmp_path / 'repo')
    outputs = [tmp_path / 'one', tmp_path / 'two']
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(lambda path: mod_publication.export_snapshot(repo, path, signer=signer), outputs))
    assert sorted(payload((path / 'v1/trust/catalog').read_bytes())['version'] for path in outputs) == [1, 2]
    for path in outputs:
        mod_publication.validate_snapshot(path, pinned)


def test_public_host_requires_external_pin_and_refuses_unsigned_snapshot(tmp_path, trust_keys):
    now = int(time.time())
    pinned, _ = authority(trust_keys, now)
    repo = Repository(tmp_path / 'repo')
    mod_publication.export_snapshot(repo, tmp_path / 'unsigned')
    with pytest.raises(ModError, match='missing'):
        mod_publication.validate_snapshot(tmp_path / 'unsigned', pinned)
    signer = PublicationSigner(pinned, [trust_keys[4]])
    mod_publication.export_snapshot(repo, tmp_path / 'signed', signer=signer)
    unrelated = make_root(trust_keys[2:4], [trust_keys[5]], 1, ORIGIN, now - 60)
    with pytest.raises(ModError, match='threshold'):
        mod_publication.validate_snapshot(tmp_path / 'signed', unrelated)


@pytest.mark.skipif(os.name != 'posix', reason='Production private-key permissions require Unix')
def test_private_key_loading_permissions_and_catalog_role(tmp_path, trust_keys, monkeypatch):
    # Unit adapter allows root-owned CI sandboxes to exercise the non-root check
    # without granting the real CLI an override or issuing any production key.
    actual_uid = os.geteuid()
    monkeypatch.setattr(mod_signing.os, 'geteuid', lambda: actual_uid or 1)
    folder = tmp_path.resolve() / 'private'
    folder.mkdir(mode=0o700)
    if actual_uid == 0:
        os.chown(folder, 1, -1)
    key = folder / 'catalog.pem'
    key.write_bytes(trust_keys[4].private_bytes(serialization.Encoding.PEM,
                    serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    key.chmod(0o600)
    if actual_uid == 0:
        os.chown(key, 1, -1)
    assert key_id(public_key(mod_signing.load_private(key))) == key_id(public_key(trust_keys[4]))
    key.chmod(0o644)
    with pytest.raises(ModError, match='0600'):
        mod_signing.load_private(key)
    key.chmod(0o600)
    alias = folder / 'alias.pem'
    alias.symlink_to(key)
    with pytest.raises(OSError):
        mod_signing.load_private(alias)


@pytest.mark.skipif(os.name != 'posix' or getattr(os, 'geteuid', lambda: 0)() == 0,
                    reason='Real non-root Unix key ceremony')
def test_real_private_key_ceremony_and_online_export(tmp_path, monkeypatch, capsys):
    tmp_path = tmp_path.resolve()
    passphrase = 'temporary fixture passphrase only'
    monkeypatch.setattr(mod_signing.getpass, 'getpass', lambda _: passphrase)
    for name in ('root-a', 'root-b', 'online'):
        assert mod_signing.main(['keygen', str(tmp_path / name), *([] if name == 'online' else ['--offline'])]) == 0
    root_path = tmp_path / 'root.json'
    assert mod_signing.main(['root', '--root-key', str(tmp_path / 'root-a/private.pem'),
                            '--root-key', str(tmp_path / 'root-b/private.pem'),
                            '--catalog-public', str(tmp_path / 'online/public.json'),
                            '--origin', ORIGIN, '--output', str(root_path)]) == 0
    printed = capsys.readouterr().out
    assert passphrase not in printed and 'PRIVATE KEY' not in printed and 'modulus' not in printed
    signer = mod_signing.load_signer(root_path, [tmp_path / 'online/private.pem'])
    repo = Repository(tmp_path / 'repo')
    mod_publication.export_snapshot(repo, tmp_path / 'public', signer=signer)
    assert mod_publication.validate_snapshot(tmp_path / 'public', root_path.read_bytes())['releases'] == []
    with pytest.raises(ModError, match='noninteractive'):
        mod_signing.load_signer(root_path, [tmp_path / 'root-a/private.pem'])
    assert mod_signing.main(['keygen', str(tmp_path / 'online')]) == 2  # Existing secrets preserved.


def test_cli_requires_explicit_signed_export_or_development_preview(tmp_path):
    import importlib.util
    script = Path(__file__).resolve().parents[2] / 'tools/mods/mod-workshop.py'
    spec = importlib.util.spec_from_file_location('workshop_signing_cli', script)
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    args = ['export', '--repository', str(tmp_path / 'repo'), '--output', str(tmp_path / 'out')]
    assert cli.main(args) == 2 and not (tmp_path / 'out').exists()
    assert cli.main([*args, '--unsigned-preview']) == 0
