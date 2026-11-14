"""Real password derivation, persistent auth and adversarial private upload lifecycle."""
from concurrent.futures import ThreadPoolExecutor
from dataclasses import replace
import io
import json
from pathlib import Path
import sys
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_accounts import Accounts, Actor, ServiceError, SESSION_TTL, INVITE_TTL
from mod_package import build_package, digest, ModError
from mod_repository import Repository
from mod_uploads import Uploads, UploadLimits, WORK_TTL
from mod_scan_sandbox import ISOLATION_POLICY

PASSWORD = 'A fixture password with spaces!'


class Clock:
    def __init__(self):
        self.value = int(time.time())
    def __call__(self):
        return self.value


class Scanner:
    def __init__(self, status='clean'):
        self.status = status
        self.calls = 0
    def scan(self, folder):
        assert (Path(folder) / 'mod.json').is_file()
        self.calls += 1
        return dict(engine='TEST ONLY', version='fixture', status=self.status, time=int(time.time()),
                    detail='', database_max_age_days=2, isolation=ISOLATION_POLICY)


@pytest.fixture
def service(tmp_path):
    clock = Clock()
    repository = Repository(tmp_path / 'repository')
    accounts = Accounts(repository, clock)
    return accounts, Uploads(accounts), clock


def account(accounts, name='alice', role='creator'):
    invite = accounts.create_invite(name, role)
    session = accounts.redeem(invite, PASSWORD, 'fixture-' + name)
    return accounts.authenticate(session['token']), session['token'], invite


def package(tmp_path, identity='test-addon', version='1.0.0', author='alice'):
    source = tmp_path / (identity + '-' + version + '-' + author)
    (source / 'scripts').mkdir(parents=True)
    manifest = dict(schema=1, id=identity, version=version, name='Original fixture', author=author,
                    description='Original test source', game='tsfp-us-45410066', api='tsfp-mod-v1-draft',
                    dependencies={}, files={})
    (source / 'mod.json').write_text(json.dumps(manifest))
    (source / 'scripts/main.lua').write_text('return {}\n')
    path = tmp_path / (source.name + '.zip')
    build_package(source, path)
    return path.read_bytes()


def reserve(uploads, actor, data, identity='test-addon', version='1.0.0'):
    return uploads.begin(actor, identity, version, digest(data), len(data))


def queued(uploads, actor, data, identity='test-addon', version='1.0.0'):
    ticket = reserve(uploads, actor, data, identity, version)
    result = uploads.receive(actor, ticket['id'], io.BytesIO(data), len(data))
    assert result['state'] == 'queued'
    return ticket


def test_invitation_session_storage_and_revocation(service):
    accounts, uploads, clock = service
    actor, token, invite = account(accounts)
    assert actor.username == 'alice' and actor.role == 'creator'
    with pytest.raises(ServiceError):
        accounts.redeem(invite, PASSWORD, 'fixture-alice')
    # Reopening the database preserves sessions; no secret is persisted in plaintext.
    assert Accounts(accounts.repository, clock).authenticate(token).id == actor.id
    database = (accounts.repository.root / 'catalog.sqlite').read_bytes()
    for secret in (token, invite, PASSWORD):
        assert secret.encode() not in database
    forged = Actor(actor.id, 'someone-else', 'admin', actor.session_hash)
    with accounts.repository.db() as db:
        with pytest.raises(ServiceError, match='Permission'):
            accounts.actor(db, forged, ('admin',))
    accounts.logout(actor)
    with pytest.raises(ServiceError):
        accounts.authenticate(token)


def test_expiry_short_password_and_wrong_password_fail(service):
    accounts, _, clock = service
    invite = accounts.create_invite('alice')
    with pytest.raises(ServiceError):
        accounts.redeem(invite, 'short', 'fixture-alice')
    clock.value += INVITE_TTL
    with pytest.raises(ServiceError):
        accounts.redeem(invite, PASSWORD, 'fixture-alice')
    actor, token, _ = account(accounts, 'bravo')
    with pytest.raises(ServiceError, match='Sign-in failed'):
        accounts.login('bravo', 'wrong-password-value', 'fixture-bravo')
    clock.value += SESSION_TTL
    with pytest.raises(ServiceError):
        accounts.authenticate(token)


def test_password_change_and_operator_recovery_preserve_identity(service):
    accounts, _, clock = service
    actor, token, _ = account(accounts)
    replacement = 'A different fixture password!'
    accounts.change_password(actor, PASSWORD, replacement, 'fixture-alice')
    with pytest.raises(ServiceError):
        accounts.authenticate(token)
    token2 = accounts.login('alice', replacement, 'fixture-alice')['token']
    assert accounts.authenticate(token2).id == actor.id
    recovery = accounts.recovery_invite('alice', 'Fixture operator verified the creator identity')
    with pytest.raises(ServiceError):
        accounts.authenticate(token2)
    recovered = accounts.redeem(recovery, PASSWORD, 'fixture-alice')['token']
    assert accounts.authenticate(recovered).id == actor.id
    with pytest.raises(ServiceError):
        accounts.redeem(recovery, PASSWORD, 'fixture-alice')


def test_persistent_rate_limits_and_account_suspension(service):
    accounts, _, clock = service
    actor, token, _ = account(accounts)
    for _ in range(5):
        with pytest.raises(ServiceError) as error:
            accounts.login('alice', 'wrong-password-value', 'fixture-login')
        assert error.value.status == 401
    with pytest.raises(ServiceError) as error:
        Accounts(accounts.repository, clock).login('alice', PASSWORD, 'another-peer')
    assert error.value.status == 429
    clock.value += 301
    assert accounts.login('alice', PASSWORD, 'fixture-login')['token']
    accounts.suspend('alice', 'Fixture suspension')
    with pytest.raises(ServiceError):
        accounts.authenticate(token)


def test_creator_to_queue_review_approval_download_and_revoke(service, tmp_path):
    accounts, uploads, _ = service
    alice, _, _ = account(accounts)
    reviewer, _, _ = account(accounts, 'reviewer', 'moderator')
    data = package(tmp_path)
    ticket = queued(uploads, alice, data)
    assert accounts.repository.catalog() == []
    with pytest.raises(ServiceError):
        uploads.moderate(reviewer, ticket['id'], 'approve', 'Too early')
    scanner = Scanner()
    assert uploads.process_next(scanner)['state'] == 'pending-review'
    assert scanner.calls == 1 and accounts.repository.catalog() == []
    with pytest.raises(ServiceError) as error:
        uploads.moderate(alice, ticket['id'], 'approve', 'Creator cannot approve')
    assert error.value.status == 403
    assert uploads.reviews(reviewer)[0]['id'] == ticket['id']
    assert uploads.moderate(reviewer, ticket['id'], 'approve', 'Reviewed original fixture')['state'] == 'approved'
    assert accounts.repository.approved_blob(digest(data)).read_bytes() == data
    assert uploads.status(alice, ticket['id'])['state'] == 'approved'
    uploads.moderate(reviewer, ticket['id'], 'revoke', 'Acceptance takedown')
    assert accounts.repository.catalog() == []
    with pytest.raises(ModError):
        accounts.repository.approved_blob(digest(data))


def test_other_creator_cannot_read_receive_or_update_owned_mod(service, tmp_path):
    accounts, uploads, _ = service
    alice, _, _ = account(accounts)
    bob, _, _ = account(accounts, 'bravo')
    data = package(tmp_path)
    ticket = reserve(uploads, alice, data)
    for action in (lambda: uploads.status(bob, ticket['id']),
                   lambda: uploads.receive(bob, ticket['id'], io.BytesIO(data), len(data))):
        with pytest.raises(ServiceError) as error:
            action()
        assert error.value.status == 404
    uploads.receive(alice, ticket['id'], io.BytesIO(data), len(data))
    with pytest.raises(ServiceError) as error:
        reserve(uploads, bob, data, version='2.0.0')
    assert error.value.status == 403


@pytest.mark.parametrize('kind', ['hash','length','identity','author','truncated'])
def test_invalid_uploads_stay_private_and_retry_safely(service, tmp_path, kind):
    accounts, uploads, _ = service
    actor, _, _ = account(accounts)
    data = package(tmp_path, author='impostor' if kind == 'author' else 'alice')
    ticket = reserve(uploads, actor, data, identity='different-mod' if kind == 'identity' else 'test-addon')
    body = data[:-1] + bytes([data[-1] ^ 1]) if kind == 'hash' else data
    if kind == 'truncated':
        body = body[:-1]
    with pytest.raises((ServiceError, ModError)):
        uploads.receive(actor, ticket['id'], io.BytesIO(body), len(data) - 1 if kind == 'length' else len(data))
    assert accounts.repository.catalog() == []
    assert not list(uploads.directory.iterdir())
    if kind != 'length':
        assert uploads.status(actor, ticket['id'])['state'] == 'failed'
    else:
        assert uploads.status(actor, ticket['id'])['state'] == 'open'


def test_new_mod_claim_race_checks_owner_at_finalization(service, tmp_path):
    accounts, uploads, _ = service
    alice, _, _ = account(accounts)
    bravo, _, _ = account(accounts, 'bravo')
    first = package(tmp_path)
    second = package(tmp_path, version='2.0.0', author='bravo')
    a = reserve(uploads, alice, first)
    b = reserve(uploads, bravo, second, version='2.0.0')
    uploads.receive(alice, a['id'], io.BytesIO(first), len(first))
    with pytest.raises(ServiceError) as error:
        uploads.receive(bravo, b['id'], io.BytesIO(second), len(second))
    assert error.value.status == 403
    assert uploads.status(bravo, b['id'])['state'] == 'failed'


def test_quota_reservations_are_atomic_and_rejected_content_remains_charged(service, tmp_path):
    accounts, _, _ = service
    actor, _, _ = account(accounts)
    data = package(tmp_path)
    limits = replace(UploadLimits(), global_bytes=len(data), account_bytes=len(data))
    uploads = Uploads(accounts, limits)
    def begin(identity):
        try:
            return reserve(uploads, actor, data, identity=identity)
        except ServiceError as exc:
            return exc.status
    with ThreadPoolExecutor(max_workers=2) as executor:
        results = list(executor.map(begin, ['first-mod','second-mod']))
    assert sum(isinstance(value, dict) for value in results) == 1 and 413 in results
    # Separate canonical reservation: rejected malware remains private and charged.
    with accounts.repository.db() as db:
        db.execute("UPDATE workshop_uploads SET state='expired'")
    ticket = queued(uploads, actor, data)
    assert uploads.process_next(Scanner('blocked'))['state'] == 'rejected'
    with pytest.raises(ServiceError) as error:
        reserve(uploads, actor, data, version='2.0.0')
    assert error.value.status == 413


def test_worker_lock_retry_and_post_commit_crash_recovery(service, tmp_path, monkeypatch):
    accounts, uploads, clock = service
    actor, _, _ = account(accounts)
    ticket = queued(uploads, actor, package(tmp_path))
    with uploads._worker_lock():
        with pytest.raises(ServiceError) as error:
            Uploads(accounts).process_next(Scanner())
        assert error.value.status == 409
    scanner = Scanner('error')
    assert uploads.process_next(scanner)['retry']
    assert uploads.process_next(scanner) is None
    clock.value += 301
    scanner.status = 'clean'
    assert uploads.process_next(scanner)['state'] == 'pending-review'
    assert scanner.calls == 2
    assert uploads.status(actor, ticket['id'])['state'] == 'pending-review'


def test_worker_recovery_never_reapproves_or_rescans_an_existing_clean_release(service, tmp_path, monkeypatch):
    accounts, uploads, clock = service
    actor, _, _ = account(accounts)
    ticket = queued(uploads, actor, package(tmp_path))
    original = accounts.audit
    def crash(db, actor, action, target, note=''):
        if action == 'upload-scanned':
            raise RuntimeError('Simulated controller crash after release commit')
        return original(db, actor, action, target, note)
    monkeypatch.setattr(accounts, 'audit', crash)
    scanner = Scanner()
    with pytest.raises(RuntimeError):
        uploads.process_next(scanner)
    monkeypatch.setattr(accounts, 'audit', original)
    assert uploads.process_next(scanner) is None
    clock.value += WORK_TTL + 1
    assert uploads.process_next(scanner)['state'] == 'pending-review'
    assert scanner.calls == 1 and accounts.repository.catalog() == []
    assert uploads.status(actor, ticket['id'])['state'] == 'pending-review'


def test_review_audit_failure_rolls_back_approval_atomically(service, tmp_path, monkeypatch):
    accounts, uploads, _ = service
    actor, _, _ = account(accounts)
    reviewer, _, _ = account(accounts, 'reviewer', 'moderator')
    ticket = queued(uploads, actor, package(tmp_path))
    uploads.process_next(Scanner())
    original = accounts.audit
    def full(db, actor, action, target, note=''):
        if action == 'approve':
            raise ServiceError(503, 'Simulated audit capacity')
        return original(db, actor, action, target, note)
    monkeypatch.setattr(accounts, 'audit', full)
    with pytest.raises(ServiceError):
        uploads.moderate(reviewer, ticket['id'], 'approve', 'Must remain private')
    assert accounts.repository.catalog() == []
    assert uploads.status(actor, ticket['id'])['state'] == 'pending-review'


def test_session_revoked_during_stream_cannot_queue_or_claim_mod(service, tmp_path):
    accounts, uploads, _ = service
    actor, _, _ = account(accounts)
    data = package(tmp_path)
    ticket = reserve(uploads, actor, data)
    class RevokedStream(io.BytesIO):
        def read(self, amount):
            accounts.logout(actor)
            return super().read(amount)
    with pytest.raises(ServiceError):
        uploads.receive(actor, ticket['id'], RevokedStream(data), len(data))
    assert not list(uploads.directory.iterdir())
    with accounts.repository.db() as db:
        assert db.execute('SELECT COUNT(*) FROM workshop_owners').fetchone()[0] == 0
        assert db.execute('SELECT state FROM workshop_uploads WHERE id=?', (ticket['id'],)).fetchone()[0] == 'failed'


def test_tampered_queued_bytes_never_enter_releases_and_remain_charged(service, tmp_path):
    accounts, uploads, _ = service
    actor, _, _ = account(accounts)
    ticket = queued(uploads, actor, package(tmp_path))
    path = uploads._path(ticket['id'], '.ready')
    path.write_bytes(path.read_bytes() + b'changed')
    with pytest.raises(ModError, match='digest changed'):
        uploads.process_next(Scanner())
    assert uploads.status(actor, ticket['id'])['state'] == 'held'
    with accounts.repository.db() as db:
        assert db.execute('SELECT COUNT(*) FROM releases').fetchone()[0] == 0
    small = Uploads(accounts, replace(UploadLimits(), global_bytes=ticket['size']))
    with pytest.raises(ServiceError) as error:
        reserve(small, actor, b'other', identity='another-mod')
    assert error.value.status == 413


def test_failed_stream_frees_reservation_but_not_daily_limit(service, tmp_path):
    accounts, _, _ = service
    actor, _, _ = account(accounts)
    uploads = Uploads(accounts, replace(UploadLimits(), daily_account=1))
    data = package(tmp_path)
    ticket = reserve(uploads, actor, data)
    with pytest.raises(ServiceError):
        uploads.receive(actor, ticket['id'], io.BytesIO(b''), len(data))
    with pytest.raises(ServiceError) as error:
        reserve(uploads, actor, data)
    assert error.value.status == 429


def test_expired_ticket_frees_pending_slot_and_storage(service, tmp_path):
    accounts, _, clock = service
    actor, _, _ = account(accounts)
    data = package(tmp_path)
    uploads = Uploads(accounts, replace(UploadLimits(), global_bytes=len(data), pending_account=1))
    ticket = reserve(uploads, actor, data)
    clock.value += 901
    assert uploads.status(actor, ticket['id'])['state'] == 'expired'
    again = reserve(uploads, actor, data)
    assert again['id'] != ticket['id']


def test_low_disk_headroom_refuses_before_spooling(service, tmp_path, monkeypatch):
    import mod_uploads
    accounts, uploads, _ = service
    actor, _, _ = account(accounts)
    monkeypatch.setattr(mod_uploads.shutil, 'disk_usage', lambda _: type('Disk',(),{'free':1})())
    with pytest.raises(ServiceError) as error:
        reserve(uploads, actor, package(tmp_path))
    assert error.value.status == 507 and not list(uploads.directory.iterdir())
