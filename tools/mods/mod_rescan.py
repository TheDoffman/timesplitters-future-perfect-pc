"""Serial, periodic revalidation of previously reviewed immutable releases."""
import json
import secrets
import tempfile
import time

from mod_package import MAX_ARCHIVE, ModError, digest, inspect_package, require
from mod_repository import SCAN_TTL, DATABASE_MAX_AGE_DAYS
from mod_uploads import Uploads

RESCAN_INTERVAL = 6 * 60 * 60
RETRY_INTERVAL = 5 * 60
MAX_RELEASES = 1000
MAX_EVENTS = 100000


def rescan_next(accounts, scanner, interval=RESCAN_INTERVAL, now=None):
    """The local service supplies the scanner, never an uploaded field.

    Clean renewal retains an existing review only. New uploads, operator-held
    errors and restored approvals never obtain automatic approval here.
    """
    require(type(interval) is int and 300 <= interval < SCAN_TTL, 'Invalid periodic scan interval')
    now = int(time.time()) if now is None else now
    require(type(now) is int and now >= 0, 'Invalid periodic scan clock')
    repository = accounts.repository
    repository.require_recovered()
    uploads = Uploads(accounts)
    with uploads._worker_lock():
        lease = secrets.token_hex(16)
        with repository.db() as db:
            db.execute('CREATE TABLE IF NOT EXISTS periodic_scans ('
                       'sha256 TEXT PRIMARY KEY REFERENCES releases(sha256), '
                       'next_at INTEGER NOT NULL, lease TEXT NOT NULL, reviewed INTEGER NOT NULL)')
            db.execute('BEGIN IMMEDIATE')
            rows = db.execute("SELECT r.sha256,r.state,r.scan,s.next_at,s.reviewed "
                              'FROM releases r LEFT JOIN periodic_scans s ON r.sha256=s.sha256 '
                              "WHERE r.state='approved' OR (r.state IN ('scan-error','rescanning') AND s.reviewed=1) "
                              'ORDER BY COALESCE(s.next_at,0),r.sha256 LIMIT ?', (MAX_RELEASES + 1,)).fetchall()
            require(len(rows) <= MAX_RELEASES, 'Periodic release budget exhausted')
            candidate = None
            for row in rows:
                if row['next_at'] is not None:
                    due = row['next_at'] <= now
                else:
                    try:
                        report = json.loads(row['scan'])
                        stamp = report.get('time')
                        due = type(stamp) is not int or stamp > now or stamp + interval <= now
                    except (ValueError, AttributeError):
                        due = True
                if due:
                    candidate = row
                    break
            if candidate is None:
                return None
            require(db.execute("SELECT COUNT(*) FROM events WHERE action LIKE 'periodic-%'").fetchone()[0] < MAX_EVENTS,
                    'Periodic audit capacity exhausted; archive history before continuing')
            sha = candidate['sha256']
            db.execute("UPDATE releases SET state='rescanning' WHERE sha256=?", (sha,))
            db.execute('INSERT INTO periodic_scans VALUES(?,?,?,1) ON CONFLICT(sha256) '
                       'DO UPDATE SET next_at=excluded.next_at,lease=excluded.lease,reviewed=1',
                       (sha, now, lease))
        # No long writer transaction during extraction/AV. Readers withhold this
        # release while moderators remain able to revoke it immediately.
        integrity = True
        try:
            blob = repository.blob(sha)
            require(blob.stat().st_size <= MAX_ARCHIVE, 'Stored archive exceeds its limit')
            integrity = digest(blob.read_bytes()) == sha
            require(integrity, 'Stored archive digest changed')
            with tempfile.TemporaryDirectory(dir=repository.root) as temporary:
                inspect_package(blob, temporary)
                report = scanner.scan(temporary)
            require(isinstance(report, dict) and report.get('status') in ('clean', 'blocked', 'error'), 'Invalid scanner report')
            require(len(json.dumps(report).encode()) <= 128 * 1024, 'Scanner report exceeds limit')
            if report['status'] == 'clean':
                require(type(report.get('time')) is int and report['time'] >= 0, 'Invalid scanner timestamp')
                require(repository.fresh(report), 'Scanner result is not fresh or lacks required isolation')
        except (OSError, ModError, ValueError, TypeError) as error:
            report = dict(engine='ClamAV', status='error', time=now,
                          database_max_age_days=DATABASE_MAX_AGE_DAYS, detail=str(error)[:2048])
        status = report['status']
        state = {'clean': 'approved', 'blocked': 'revoked', 'error': 'scan-error'}[status]
        reviewed = int(integrity and state != 'revoked')
        delay = interval if state == 'approved' else RETRY_INTERVAL
        with repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            current = db.execute('SELECT state FROM releases WHERE sha256=?', (sha,)).fetchone()
            owner = db.execute('SELECT lease FROM periodic_scans WHERE sha256=?', (sha,)).fetchone()
            if current is None or owner is None or owner['lease'] != lease:
                return {'sha256': sha, 'state': 'superseded'}
            if current['state'] != 'rescanning':
                # A manual rescan/review/revocation wins over an older AV result.
                db.execute('UPDATE periodic_scans SET lease=\'\',next_at=?,reviewed=? WHERE sha256=?',
                           (now + interval, int(current['state'] == 'approved'), sha))
                return {'sha256': sha, 'state': current['state'], 'superseded': True}
            db.execute('UPDATE releases SET state=?,scan=? WHERE sha256=?', (state, json.dumps(report), sha))
            db.execute('UPDATE periodic_scans SET lease=\'\',next_at=?,reviewed=? WHERE sha256=?',
                       (now + delay, reviewed, sha))
            repository.event(db, sha, 'periodic-' + status,
                             'Existing review renewed' if state == 'approved' else 'Availability withheld; ' + state)
        return {'sha256': sha, 'state': state, 'retry_at': now + delay if state == 'scan-error' and reviewed else None}
