"""Renew existing reviews only; malware, races and recovery stay fail closed."""
import json
from pathlib import Path
import sys
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_accounts import Accounts, ServiceError
from mod_package import ModError, digest
from mod_repository import Repository
from mod_rescan import rescan_next, RESCAN_INTERVAL, RETRY_INTERVAL
from mod_uploads import Uploads
from test_creator_service import Scanner, account, package, queued


@pytest.fixture
def reviewed(tmp_path):
    repo = Repository(tmp_path / 'repository')
    accounts = Accounts(repo)
    uploads = Uploads(accounts)
    actor, _, _ = account(accounts)
    data = package(tmp_path)
    queued(uploads, actor, data)
    uploads.process_next(Scanner())
    sha = digest(data)
    repo.approve(sha, 'Explicit fixture review')
    now = int(time.time())
    with repo.db() as db:
        report = json.loads(repo.get(sha)['scan'])
        report['time'] = now - RESCAN_INTERVAL - 10
        db.execute('UPDATE releases SET scan=? WHERE sha256=?', (json.dumps(report), sha))
    return accounts, repo, sha, now


def test_clean_periodic_scan_renews_existing_review_then_waits(reviewed):
    accounts, repo, sha, now = reviewed
    scanner = Scanner()
    result = rescan_next(accounts, scanner, now=now)
    assert result['state'] == 'approved' and scanner.calls == 1
    assert repo.catalog()[0]['sha256'] == sha
    assert rescan_next(accounts, scanner, now=now + 1) is None
    with repo.db() as db:
        assert db.execute("SELECT COUNT(*) FROM events WHERE action='approved'").fetchone()[0] == 1
        assert db.execute("SELECT COUNT(*) FROM events WHERE action='periodic-clean'").fetchone()[0] == 1


def test_new_pending_or_revoked_release_never_gets_automatic_approval(reviewed):
    accounts, repo, sha, now = reviewed
    with repo.db() as db:
        db.execute("UPDATE releases SET state='pending-review'")
    scanner = Scanner()
    assert rescan_next(accounts, scanner, now=now) is None
    repo.revoke(sha, 'Never renew revoked content')
    assert rescan_next(accounts, scanner, now=now) is None and scanner.calls == 0


def test_detection_revokes_and_cannot_later_auto_reapprove(reviewed):
    accounts, repo, sha, now = reviewed
    assert rescan_next(accounts, Scanner('blocked'), now=now)['state'] == 'revoked'
    assert repo.catalog() == []
    assert rescan_next(accounts, Scanner(), now=now + RESCAN_INTERVAL) is None
    with pytest.raises(ModError, match='revoked'):
        repo.rescan(sha, Scanner())


def test_transient_error_withholds_bytes_and_retries_prior_review_only(reviewed):
    accounts, repo, sha, now = reviewed
    result = rescan_next(accounts, Scanner('error'), now=now)
    assert result['state'] == 'scan-error' and result['retry_at'] == now + RETRY_INTERVAL
    assert repo.catalog() == []
    assert rescan_next(accounts, Scanner(), now=now + 1) is None
    assert rescan_next(accounts, Scanner(), now=now + RETRY_INTERVAL)['state'] == 'approved'


@pytest.mark.parametrize('mutation', ['revoke', 'manual-rescan'])
def test_concurrent_moderation_wins_and_database_is_not_locked_during_scan(reviewed, mutation):
    accounts, repo, sha, now = reviewed
    class Racing(Scanner):
        def scan(self, folder):
            assert repo.catalog() == []  # In-flight scans withhold availability.
            if mutation == 'revoke':
                repo.revoke(sha, 'Concurrent operator revocation')
            else:
                repo.rescan(sha, Scanner())
            return super().scan(folder)
    result = rescan_next(accounts, Racing(), now=now)
    assert result['superseded']
    assert repo.get(sha)['state'] == ('revoked' if mutation == 'revoke' else 'pending-review')


def test_changed_stored_bytes_require_manual_repair_and_review(reviewed):
    accounts, repo, sha, now = reviewed
    original = repo.blob(sha).read_bytes()
    repo.blob(sha).write_bytes(b'changed archive')
    result = rescan_next(accounts, Scanner(), now=now)
    assert result['state'] == 'scan-error' and result['retry_at'] is None
    repo.blob(sha).write_bytes(original)
    assert rescan_next(accounts, Scanner(), now=now + RETRY_INTERVAL) is None
    repo.rescan(sha, Scanner())
    assert repo.get(sha)['state'] == 'pending-review'


def test_abrupt_scan_exit_is_reclaimed_under_the_worker_lock(reviewed):
    accounts, repo, sha, now = reviewed
    class Crashing:
        def scan(self, folder):
            raise RuntimeError('Simulated controller crash')
    with pytest.raises(RuntimeError):
        rescan_next(accounts, Crashing(), now=now)
    assert repo.get(sha)['state'] == 'rescanning' and repo.catalog() == []
    assert rescan_next(accounts, Scanner(), now=now + 1)['state'] == 'approved'


def test_serial_worker_lock_covers_queue_and_periodic_scans(reviewed):
    accounts, _, _, now = reviewed
    with Uploads(accounts)._worker_lock():
        with pytest.raises(ServiceError, match='already running'):
            rescan_next(accounts, Scanner(), now=now)


@pytest.mark.parametrize('field,value', [('isolation','old-policy'), ('database_max_age_days',3), ('time',1), ('time',1.5)])
def test_a_claimed_clean_scan_still_requires_current_freshness_policy(reviewed, field, value):
    accounts, repo, _, now = reviewed
    class Invalid(Scanner):
        def scan(self, folder):
            report = super().scan(folder)
            report[field] = value
            return report
    assert rescan_next(accounts, Invalid(), now=now)['state'] == 'scan-error'
    assert repo.catalog() == []
