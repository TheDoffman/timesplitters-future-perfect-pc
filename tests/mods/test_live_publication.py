"""Signed live refresh, withdrawal races and owned immutable-output retention."""
import json
import os
from pathlib import Path
import sys
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_live_publication as live
from mod_package import ModError
from mod_publication import validate_snapshot
from mod_trust import PublicationSigner, envelope, make_root
from test_mod_workshop import Scanner, approved

pytestmark = pytest.mark.skipif(os.name != 'posix', reason='Private POSIX publication controller')


class Server:
    def __init__(self):
        self.active = False
        self.events = []

    def start(self, path, seconds):
        assert not self.active and 0 < seconds <= 3600
        self.active = True
        self.events.append(('start', path))

    def running(self):
        return self.active

    def stop(self):
        self.active = False
        self.events.append(('stop', None))


@pytest.fixture
def host(tmp_path, trust_keys):
    tmp_path.chmod(0o700)
    repo, _, sha = approved(tmp_path)
    repo.root.chmod(0o700)
    pinned = make_root(trust_keys[:2], [trust_keys[4]], 1,
                       'https://workshop.example.test', int(time.time()) - 60)
    signer = PublicationSigner(pinned, [trust_keys[4]])
    store = live.SnapshotStore(tmp_path / 'publications', repo, pinned)
    server = Server()
    return live.LivePublisher(repo, signer, store, server), sha


def metadata(publisher):
    path = publisher.current / 'v1/trust/catalog'
    return json.loads(envelope(path.read_bytes())[0])


def test_live_revoke_withdraws_old_server_and_publishes_signed_tombstone(host):
    publisher, sha = host
    assert publisher.tick()
    initial = publisher.current
    assert validate_snapshot(initial, publisher.signer.pinned_root)['releases'][0]['sha256'] == sha
    first = metadata(publisher)['version']
    assert not publisher.tick()
    publisher.repository.revoke(sha, 'Fixture revocation')
    assert publisher.tick()
    assert publisher.server.events[-2][0] == 'stop' and publisher.server.events[-1][0] == 'start'
    assert validate_snapshot(publisher.current, publisher.signer.pinned_root)['releases'] == []
    assert metadata(publisher)['revoked'] == [sha] and metadata(publisher)['version'] > first
    assert not (publisher.current / ('v1/packages/' + sha)).exists()


def test_scan_error_and_pending_review_remain_withheld_until_manual_approval(host):
    publisher, sha = host
    publisher.tick()
    publisher.repository.rescan(sha, Scanner('error'))
    publisher.tick()
    assert validate_snapshot(publisher.current, publisher.signer.pinned_root)['releases'] == []
    publisher.repository.rescan(sha, Scanner())
    assert not publisher.tick()
    publisher.repository.approve(sha, 'Fresh fixture reviewed')
    assert publisher.tick()
    assert validate_snapshot(publisher.current, publisher.signer.pinned_root)['releases'][0]['sha256'] == sha


def test_failed_refresh_never_keeps_old_downloads_running(host, monkeypatch):
    publisher, sha = host
    publisher.tick()
    old = publisher.current
    original = (old / 'v1/catalog').read_bytes()
    publisher.repository.revoke(sha, 'Fixture revocation')
    def fail(*_):
        raise ModError('Injected signing/storage failure')
    monkeypatch.setattr(publisher.store, 'create', fail)
    with pytest.raises(ModError):
        publisher.tick()
    assert not publisher.server.running() and publisher.current is None
    assert (old / 'v1/catalog').read_bytes() == original


def test_moderation_change_after_export_never_starts_that_stale_export(host, monkeypatch):
    publisher, sha = host
    create = publisher.store.create
    def race(repo, signer):
        result = create(repo, signer)
        repo.revoke(sha, 'Race fixture')
        return result
    monkeypatch.setattr(publisher.store, 'create', race)
    assert not publisher.tick()
    assert not publisher.server.running() and not any(e[0] == 'start' for e in publisher.server.events)
    monkeypatch.setattr(publisher.store, 'create', create)
    assert publisher.tick() and metadata(publisher)['revoked'] == [sha]


def test_retention_keeps_current_and_previous_and_recovers_complete_receipts(host):
    publisher, _ = host
    for _ in range(3):
        publisher.store.create(publisher.repository, publisher.signer)
    assert len(publisher.store.records()) == 3
    # Simulate a restart after receipt commit, before retention completed.
    store = live.SnapshotStore(publisher.store.root, publisher.repository, publisher.signer.pinned_root)
    publisher.store = store
    assert publisher.tick()
    assert len(store.records()) == 2 and publisher.current.name in store.records()
    publisher.published -= live.REFRESH + 1
    assert publisher.tick() and len(store.records()) == 2


def test_retention_refuses_changed_outputs_and_preserves_unknown_files(host):
    publisher, _ = host
    publisher.tick()
    oldest = publisher.current
    publisher.published -= live.REFRESH + 1
    publisher.tick()
    canary = oldest / 'personal.txt'
    canary.write_text('preserve')
    publisher.published -= live.REFRESH + 1
    with pytest.raises(ModError, match='Preserving changed'):
        publisher.tick()
    assert canary.read_text() == 'preserve' and not publisher.server.running()


def test_interrupted_unreceipted_export_is_preserved_and_blocks_restart(host):
    publisher, _ = host
    unknown = publisher.store.snapshots / ('a' * 32)
    unknown.mkdir()
    (unknown / 'canary').write_text('preserve')
    with pytest.raises(ModError, match='Preserving interrupted'):
        live.SnapshotStore(publisher.store.root, publisher.repository, publisher.signer.pinned_root)
    assert (unknown / 'canary').read_text() == 'preserve'


def test_recovery_barrier_and_failed_server_stop_publication(host):
    publisher, _ = host
    publisher.tick()
    (publisher.repository.root / '.recovery-required').write_text('{}')
    with pytest.raises(ModError, match='recovery'):
        publisher.tick()
    assert not publisher.server.running()


def test_unexpected_server_exit_withdraws_instead_of_reusing_old_metadata(host):
    publisher, _ = host
    publisher.tick()
    publisher.server.active = False
    with pytest.raises(ModError, match='stopped unexpectedly'):
        publisher.tick()
    assert publisher.current is None and not publisher.server.running()


def test_near_expiry_release_is_withheld_without_repeated_copy_churn(host):
    publisher, sha = host
    with publisher.repository.db() as db:
        row = db.execute('SELECT scan FROM releases WHERE sha256=?', (sha,)).fetchone()
        scan = json.loads(row['scan'])
        scan['time'] = int(time.time()) - live.SCAN_TTL + 10
        db.execute('UPDATE releases SET scan=? WHERE sha256=?', (json.dumps(scan), sha))
    assert publisher.tick()
    assert validate_snapshot(publisher.current, publisher.signer.pinned_root)['releases'] == []
    assert publisher.expires - time.time() > live.MARGIN and not publisher.tick()
