"""Private host snapshots, interrupted output and conservative recovery."""
import io
import json
import os
from pathlib import Path
import sqlite3
import sys
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_backup import snapshot, restore, finish_recovery, BARRIER, MANIFEST
from mod_package import ModError, digest
from mod_repository import Repository
from mod_accounts import Accounts, ServiceError
from mod_uploads import Uploads
from mod_publication import export_snapshot
from test_creator_service import account, package, queued, Scanner

pytestmark = pytest.mark.skipif(os.name != 'posix', reason='Mac/POSIX private host operations')


@pytest.fixture
def host(tmp_path):
    tmp_path.chmod(0o700)
    root = tmp_path / 'repository'
    root.mkdir(mode=0o700)
    repository = Repository(root)
    accounts = Accounts(repository)
    uploads = Uploads(accounts)
    actor, token, _ = account(accounts)
    data = package(tmp_path)
    ticket = queued(uploads, actor, data)
    uploads.process_next(Scanner())
    repository.approve(digest(data), 'Fixture review only')
    # A separate queued upload is retained without ever scanning its source.
    pending = package(tmp_path, identity='queued-addon')
    second = queued(uploads, actor, pending, identity='queued-addon')
    with repository.db() as db:
        db.execute('CREATE TABLE publication_sequence (singleton INTEGER PRIMARY KEY,version INTEGER,origin TEXT)')
        db.execute("INSERT INTO publication_sequence VALUES(1,8,'https://workshop.example.test')")
    return repository, accounts, uploads, token, digest(data), second['id'], pending


def reconciliation(path, floor=12, revoked=()):
    value = dict(schema=1, origin='https://workshop.example.test', version_floor=floor,
                 revoked=list(revoked), note='Compared current signed publication and operator revocation journal')
    path.write_text(json.dumps(value))
    path.chmod(0o600)
    return path


def test_report_history_survives_private_restore_with_auth_revoked(host, tmp_path):
    from mod_reports import Reports
    repository, accounts, _, token, sha, _, _ = host
    report = Reports(accounts).create(accounts.authenticate(token), sha, 'broken', 'Original private recovery fixture')
    destination, recovered = tmp_path / 'report-backup', tmp_path / 'report-restore'
    snapshot(repository.root, destination)
    restore(destination, recovered)
    restored = Repository(recovered)
    with restored.db() as db:
        row = db.execute('SELECT id,sha256,description,state FROM workshop_reports').fetchone()
        assert tuple(row) == (report['id'],sha,'Original private recovery fixture','open')
        assert not db.execute('SELECT 1 FROM workshop_sessions').fetchone()
    assert (recovered / BARRIER).is_file()
    with pytest.raises(ModError):
        restored.catalog()


def test_snapshot_restore_preserves_bytes_ownership_and_history_but_not_auth(host, tmp_path):
    repository, accounts, uploads, token, sha, upload_id, pending = host
    accounts.create_invite('not-yet-redeemed')
    secret = repository.root / 'operator-secrets'
    secret.mkdir(mode=0o700)
    (secret / 'never-copy-key').write_text('private fixture')
    destination = tmp_path / 'backup'
    result = snapshot(repository.root, destination)
    assert result['files'] == 3
    assert not (destination / 'operator-secrets').exists()
    assert (destination / 'blobs' / (sha + '.zip')).read_bytes() == repository.blob(sha).read_bytes()
    assert (destination / 'incoming' / (upload_id + '.ready')).read_bytes() == pending
    restored = tmp_path / 'restored'
    assert restore(destination, restored)['recovery_required']
    recovered = Repository(restored)
    recovered_accounts = Accounts(recovered)
    from mod_api import WorkshopAPI
    api = WorkshopAPI(recovered_accounts, 'https://workshop.example.test')
    with pytest.raises(ServiceError) as unavailable:
        api.dispatch({})
    assert unavailable.value.status == 503
    with pytest.raises(ServiceError):
        recovered_accounts.authenticate(token)
    with recovered.db() as db:
        assert db.execute('SELECT COUNT(*) FROM workshop_sessions').fetchone()[0] == 0
        assert db.execute('SELECT COUNT(*) FROM workshop_invites').fetchone()[0] == 0
        assert db.execute('SELECT active FROM workshop_accounts').fetchone()[0] == 0
        assert db.execute('SELECT mod_id FROM workshop_owners ORDER BY mod_id').fetchall()
        assert db.execute('SELECT COUNT(*) FROM workshop_audit').fetchone()[0] > 0
    assert recovered.get(sha)['state'] == 'scan-error'
    for action in (recovered.catalog, lambda: recovered.approved_blob(sha),
                   lambda: export_snapshot(recovered, tmp_path / 'public')):
        with pytest.raises(ModError, match='recovery'):
            action()
    # Original repository is unaffected, including its active session/approval.
    assert accounts.authenticate(token)
    assert repository.get(sha)['state'] == 'approved'
    assert all(path.stat().st_mode & 0o077 == 0 for path in destination.rglob('*'))


def test_resume_requires_current_floor_preserves_unknown_revocations_and_rescans(host, tmp_path, trust_keys):
    repository, _, _, _, sha, _, _ = host
    snapshot(repository.root, tmp_path / 'backup')
    restored = tmp_path / 'restored'
    restore(tmp_path / 'backup', restored)
    review = tmp_path / 'review.json'
    with pytest.raises(ModError, match='roll back'):
        finish_recovery(restored, reconciliation(review, floor=7))
    assert (restored / BARRIER).exists()
    unknown = 'f' * 64
    result = finish_recovery(restored, reconciliation(review, revoked=[unknown]))
    assert result['accounts_suspended'] and not (restored / BARRIER).exists()
    recovered = Repository(restored)
    assert recovered.catalog() == []
    with recovered.db() as db:
        assert db.execute('SELECT version FROM publication_sequence').fetchone()[0] == 12
        assert db.execute('SELECT sha256 FROM recovery_revocations').fetchone()[0] == unknown
    recovered.rescan(sha, Scanner())
    recovered.approve(sha, 'Fresh post-recovery review')
    assert recovered.catalog()[0]['sha256'] == sha
    from test_catalog_trust import authority, payload
    from mod_trust import PublicationSigner
    pinned, _ = authority(trust_keys, int(time.time()))
    public = tmp_path / 'published'
    export_snapshot(recovered, public, signer=PublicationSigner(pinned, [trust_keys[4]]))
    signed = payload((public / 'v1/trust/catalog').read_bytes())
    assert signed['version'] == 13 and signed['revoked'] == [unknown]
    # All un-restored signing keys and public snapshots stay outside the backup.
    assert not (restored / 'operator-secrets').exists()


def test_reserved_snapshot_lock_captures_committed_wal_and_blocks_mutation(host, tmp_path, monkeypatch):
    import mod_backup
    repository, _, _, _, _, _, _ = host
    with sqlite3.connect(repository.root / 'catalog.sqlite') as writer:
        writer.execute('PRAGMA journal_mode=WAL')
        writer.execute("INSERT INTO events(sha256,time,action,note) VALUES('','0','wal-only','committed')")
        writer.commit()
        original = mod_backup.copy_file
        attempts = []
        def copying(*args, **kwargs):
            with sqlite3.connect(repository.root / 'catalog.sqlite', timeout=0) as concurrent:
                with pytest.raises(sqlite3.OperationalError, match='locked'):
                    concurrent.execute("INSERT INTO events VALUES(NULL,'','0','racing','must not commit')")
            attempts.append(True)
            return original(*args, **kwargs)
        monkeypatch.setattr(mod_backup, 'copy_file', copying)
        snapshot(repository.root, tmp_path / 'backup')
        assert attempts
    with sqlite3.connect(tmp_path / 'backup/catalog.sqlite') as db:
        assert db.execute("SELECT note FROM events WHERE action='wal-only'").fetchone()[0] == 'committed'


def test_known_post_backup_revocation_cannot_be_reapproved(host, tmp_path):
    repository, _, _, _, sha, _, _ = host
    snapshot(repository.root, tmp_path / 'backup')
    restored = tmp_path / 'restored'
    restore(tmp_path / 'backup', restored)
    finish_recovery(restored, reconciliation(tmp_path / 'review.json', revoked=[sha]))
    recovered = Repository(restored)
    assert recovered.get(sha)['state'] == 'revoked'
    with pytest.raises(ModError, match='revoked'):
        recovered.approve(sha, 'Must remain revoked')


def test_disk_budget_refusal_does_not_create_output(host, tmp_path, monkeypatch):
    import mod_backup
    from collections import namedtuple
    usage = namedtuple('Usage', 'total used free')
    monkeypatch.setattr(mod_backup.shutil, 'disk_usage', lambda _: usage(1000, 1000, 0))
    with pytest.raises(ModError, match='headroom'):
        snapshot(host[0].root, tmp_path / 'backup')
    assert not (tmp_path / 'backup').exists()


def test_restore_never_automatically_renews_a_historical_periodic_review(host, tmp_path):
    import time
    from mod_rescan import rescan_next, RESCAN_INTERVAL
    repository, accounts, _, _, sha, _, _ = host
    now = int(time.time())
    with repository.db() as db:
        report = json.loads(repository.get(sha)['scan'])
        report['time'] = now - RESCAN_INTERVAL - 1
        db.execute('UPDATE releases SET scan=? WHERE sha256=?', (json.dumps(report), sha))
    assert rescan_next(accounts, Scanner('error'), now=now)['retry_at'] is not None
    snapshot(repository.root, tmp_path / 'backup')
    restored = tmp_path / 'restored'
    restore(tmp_path / 'backup', restored)
    finish_recovery(restored, reconciliation(tmp_path / 'review.json'))
    recovered = Repository(restored)
    assert rescan_next(Accounts(recovered), Scanner(), now=now + RESCAN_INTERVAL) is None
    assert recovered.get(sha)['state'] == 'scan-error'


def test_backup_refuses_an_inflight_periodic_scan(host, tmp_path):
    from mod_rescan import rescan_next, RESCAN_INTERVAL
    import time
    repository, accounts, _, _, sha, _, _ = host
    now = int(time.time())
    with repository.db() as db:
        report = json.loads(repository.get(sha)['scan'])
        report['time'] = now - RESCAN_INTERVAL - 1
        db.execute('UPDATE releases SET scan=? WHERE sha256=?', (json.dumps(report), sha))
    class DuringScan(Scanner):
        def scan(self, folder):
            with pytest.raises(ModError, match='busy'):
                snapshot(repository.root, tmp_path / 'backup')
            assert not (tmp_path / 'backup').exists()
            return super().scan(folder)
    assert rescan_next(accounts, DuringScan(), now=now)['state'] == 'approved'


@pytest.mark.parametrize('state', ['receiving', 'processing'])
def test_busy_upload_or_worker_refuses_snapshot_before_output(host, tmp_path, state):
    repository, _, _, _, _, upload_id, _ = host
    with repository.db() as db:
        db.execute('UPDATE workshop_uploads SET state=? WHERE id=?', (state, upload_id))
    with pytest.raises(ModError, match='busy'):
        snapshot(repository.root, tmp_path / 'backup')
    assert not (tmp_path / 'backup').exists()


def test_tampered_package_incomplete_restore_and_existing_targets_are_preserved(host, tmp_path):
    repository, _, _, _, sha, _, _ = host
    backup = tmp_path / 'backup'
    snapshot(repository.root, backup)
    with pytest.raises(ModError, match='new'):
        snapshot(repository.root, backup)
    with pytest.raises(ModError, match='separate'):
        restore(backup, backup / 'nested-restore')
    assert not (backup / 'nested-restore').exists()
    target = backup / 'blobs' / (sha + '.zip')
    target.write_bytes(b'corrupt stored package')
    restored = tmp_path / 'restored'
    with pytest.raises(ModError, match='integrity'):
        restore(backup, restored)
    assert (restored / BARRIER).exists()
    with pytest.raises(ModError, match='new'):
        restore(backup, restored)
    assert target.read_bytes() == b'corrupt stored package'


@pytest.mark.parametrize('kind', ['link', 'hardlink', 'extra', 'traversal', 'trigger'])
def test_unexpected_backup_members_and_executable_schema_fail_closed(host, tmp_path, kind):
    repository, _, _, _, sha, _, _ = host
    backup = tmp_path / 'backup'
    snapshot(repository.root, backup)
    package_file = backup / 'blobs' / (sha + '.zip')
    if kind == 'link':
        package_file.unlink()
        package_file.symlink_to(repository.blob(sha))
    elif kind == 'hardlink':
        os.link(package_file, tmp_path / 'extra-link')
    elif kind == 'extra':
        (backup / 'personal-file').write_text('preserve')
    elif kind == 'traversal':
        manifest = json.loads((backup / MANIFEST).read_text())
        manifest['files']['../escape'] = manifest['files'].pop('catalog.sqlite')
        (backup / MANIFEST).write_text(json.dumps(manifest))
    else:
        # A self-consistent hash manifest must not authorize executable DB schema.
        with sqlite3.connect(backup / 'catalog.sqlite') as db:
            db.execute('CREATE TRIGGER hostile AFTER DELETE ON workshop_sessions BEGIN DELETE FROM releases; END')
        manifest = json.loads((backup / MANIFEST).read_text())
        data = (backup / 'catalog.sqlite').read_bytes()
        manifest['files']['catalog.sqlite'] = dict(size=len(data), sha256=digest(data))
        (backup / MANIFEST).write_text(json.dumps(manifest))
    with pytest.raises(ModError):
        restore(backup, tmp_path / 'restored')
    assert not (tmp_path / 'escape').exists()
