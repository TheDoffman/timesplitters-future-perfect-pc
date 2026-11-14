"""Authenticated, quota-reserved uploads and a private, serial scan queue."""
from contextlib import contextmanager
from dataclasses import dataclass
import hashlib
import errno
import os
from pathlib import Path
import re
import secrets
import shutil
import stat

from mod_accounts import ServiceError, demand, username
from mod_package import ID, VERSION, SHA, MAX_ARCHIVE, ModError, inspect_package

UPLOAD_ID = re.compile(r'[0-9a-f]{32}\Z')
CHARGED = "('open','receiving','queued','processing','retry','complete','held')"
TICKET_TTL = 15 * 60
RECEIVE_TTL = 5 * 60
WORK_TTL = 10 * 60


@dataclass(frozen=True)
class UploadLimits:
    global_bytes: int = 4 * 1024**3
    account_bytes: int = 1024**3
    minimum_free: int = 2 * 1024**3
    pending_account: int = 2
    pending_global: int = 8
    daily_account: int = 5
    history: int = 10000
    mods_account: int = 20


class Uploads:
    def __init__(self, accounts, limits=UploadLimits()):
        self.accounts = accounts
        self.repository = accounts.repository
        self.limits = limits
        self.directory = self.repository.root / 'incoming'
        demand(not self.directory.is_symlink(), 503, 'Private upload storage is linked')
        self.directory.mkdir(mode=0o700, exist_ok=True)
        with self.repository.db() as db:
            db.executescript('''
                CREATE TABLE IF NOT EXISTS workshop_owners (
                    mod_id TEXT PRIMARY KEY, account_id INTEGER NOT NULL REFERENCES workshop_accounts(id));
                CREATE TABLE IF NOT EXISTS workshop_uploads (
                    id TEXT PRIMARY KEY, account_id INTEGER NOT NULL REFERENCES workshop_accounts(id),
                    mod_id TEXT NOT NULL, version TEXT NOT NULL, sha256 TEXT NOT NULL,
                    size INTEGER NOT NULL, created INTEGER NOT NULL, expires INTEGER NOT NULL,
                    state TEXT NOT NULL, lease TEXT NOT NULL DEFAULT '', lease_until INTEGER NOT NULL DEFAULT 0,
                    attempts INTEGER NOT NULL DEFAULT 0, retry_at INTEGER NOT NULL DEFAULT 0,
                    error TEXT NOT NULL DEFAULT '');
                CREATE UNIQUE INDEX IF NOT EXISTS workshop_upload_version
                    ON workshop_uploads(mod_id,version) WHERE state NOT IN ('failed','expired');
                CREATE INDEX IF NOT EXISTS workshop_upload_account ON workshop_uploads(account_id,created);
            ''')

    def _path(self, upload_id, suffix):
        demand(isinstance(upload_id, str) and UPLOAD_ID.fullmatch(upload_id), 404, 'Upload unavailable')
        return self.directory / (upload_id + suffix)

    def _remove(self, upload_id, suffix):
        path = self._path(upload_id, suffix)
        try:
            mode = path.lstat().st_mode
        except FileNotFoundError:
            return
        demand(stat.S_ISREG(mode), 503, 'Unexpected private upload file; operator action required')
        path.unlink()

    def _expire(self, db):
        now = self.accounts.now()
        rows = db.execute("SELECT id FROM workshop_uploads WHERE (state='open' AND expires<=?) "
                          "OR (state='receiving' AND lease_until<=?)", (now, now)).fetchall()
        for row in rows:
            self._remove(row['id'], '.part')
            self._remove(row['id'], '.ready')
            db.execute("UPDATE workshop_uploads SET state='expired',error='upload-expired' WHERE id=?", (row['id'],))

    def _owner(self, db, actor, mod_id):
        row = db.execute('SELECT account_id FROM workshop_owners WHERE mod_id=?', (mod_id,)).fetchone()
        demand(row is None or row[0] == actor.id, 403, 'This mod ID belongs to another creator')
        if row is None:
            legacy = db.execute('SELECT 1 FROM releases WHERE mod_id=? LIMIT 1',
                                (mod_id,)).fetchone()
            demand(legacy is None, 409, 'Existing mod requires operator ownership assignment')
        return row

    def begin(self, actor, mod_id, version, sha256, size):
        demand(isinstance(mod_id, str) and len(mod_id) <= 80 and ID.fullmatch(mod_id), 400, 'Invalid mod ID')
        demand(isinstance(version, str) and len(version) <= 32 and VERSION.fullmatch(version), 400, 'Invalid version')
        demand(isinstance(sha256, str) and SHA.fullmatch(sha256), 400, 'Invalid package digest')
        demand(type(size) is int and 0 < size <= MAX_ARCHIVE, 400, 'Invalid package size')
        now = self.accounts.now()
        upload_id = secrets.token_hex(16)
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor = self.accounts.actor(db, actor)
            self._expire(db)
            owner = self._owner(db, actor, mod_id)
            demand(db.execute('SELECT COUNT(*) FROM workshop_uploads').fetchone()[0] < self.limits.history,
                   503, 'Upload history capacity reached; operator action required')
            demand(db.execute('SELECT 1 FROM releases WHERE mod_id=? AND version=?',
                              (mod_id, version)).fetchone() is None, 409, 'This mod version already exists')
            duplicate = db.execute("SELECT 1 FROM workshop_uploads WHERE mod_id=? AND version=? "
                                   "AND state NOT IN ('failed','expired')", (mod_id, version)).fetchone()
            demand(duplicate is None, 409, 'This mod version already has an upload')
            if owner is None:
                count = db.execute('SELECT COUNT(*) FROM workshop_owners WHERE account_id=?', (actor.id,)).fetchone()[0]
                demand(count < self.limits.mods_account, 429, 'Creator mod-ID limit reached')
            charged = db.execute('SELECT COALESCE(SUM(size),0) FROM workshop_uploads WHERE state IN ' + CHARGED).fetchone()[0]
            own = db.execute('SELECT COALESCE(SUM(size),0) FROM workshop_uploads WHERE account_id=? AND state IN ' + CHARGED,
                             (actor.id,)).fetchone()[0]
            demand(charged + size <= self.limits.global_bytes and own + size <= self.limits.account_bytes,
                   413, 'Workshop or creator storage quota reached')
            pending = db.execute("SELECT COUNT(*) FROM workshop_uploads WHERE state IN ('open','receiving','queued','processing','retry')").fetchone()[0]
            mine = db.execute("SELECT COUNT(*) FROM workshop_uploads WHERE account_id=? AND state IN ('open','receiving','queued','processing','retry')",
                              (actor.id,)).fetchone()[0]
            demand(pending < self.limits.pending_global and mine < self.limits.pending_account,
                   429, 'Upload queue is full; retry later')
            daily = db.execute('SELECT COUNT(*) FROM workshop_uploads WHERE account_id=? AND created>=?',
                               (actor.id, now - 86400)).fetchone()[0]
            demand(daily < self.limits.daily_account, 429, 'Daily creator upload quota reached')
            reserved = db.execute("SELECT COALESCE(SUM(size),0) FROM workshop_uploads WHERE state IN ('open','receiving')").fetchone()[0]
            demand(shutil.disk_usage(self.directory).free - reserved - size >= self.limits.minimum_free,
                   507, 'Insufficient workshop disk headroom')
            db.execute('INSERT INTO workshop_uploads(id,account_id,mod_id,version,sha256,size,created,expires,state) '
                       "VALUES(?,?,?,?,?,?,?,?,'open')", (upload_id, actor.id, mod_id, version, sha256, size, now, now + TICKET_TTL))
            self.accounts.audit(db, actor.username, 'upload-reserved', upload_id, mod_id + '@' + version)
        return self.status(actor, upload_id)

    def _get(self, db, actor, upload_id):
        self._path(upload_id, '.part')
        actor = self.accounts.actor(db, actor)
        row = db.execute('SELECT * FROM workshop_uploads WHERE id=? AND account_id=?', (upload_id, actor.id)).fetchone()
        demand(row is not None, 404, 'Upload unavailable')
        return actor, row

    def receive(self, actor, upload_id, stream, content_length):
        lease = secrets.token_hex(16)
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor, row = self._get(db, actor, upload_id)
            now = self.accounts.now()
            demand(row['state'] == 'open' and row['created'] <= now < row['expires'], 409, 'Upload is not open')
            demand(type(content_length) is int and content_length == row['size'], 400, 'Upload length must match reservation')
            db.execute("UPDATE workshop_uploads SET state='receiving',lease=?,lease_until=? WHERE id=?",
                       (lease, now + RECEIVE_TTL, upload_id))
        part = self._path(upload_id, '.part')
        ready = self._path(upload_id, '.ready')
        published = False
        try:
            digest = hashlib.sha256()
            count = 0
            with part.open('xb') as output:
                if os.name == 'posix':
                    os.chmod(part, 0o600)
                while count < content_length:
                    block = stream.read(min(65536, content_length - count))
                    demand(isinstance(block, bytes) and 0 < len(block) <= min(65536, content_length - count),
                           400, 'Upload body truncated or invalid')
                    output.write(block)
                    digest.update(block)
                    count += len(block)
                output.flush()
                os.fsync(output.fileno())
            demand(digest.hexdigest() == row['sha256'], 400, 'Upload digest mismatch')
            manifest = inspect_package(part)
            demand(manifest['id'] == row['mod_id'] and manifest['version'] == row['version'],
                   400, 'Package identity does not match reservation')
            demand(manifest['author'] == actor.username, 400, 'Package author must be the authenticated creator name')
            demand(shutil.disk_usage(self.directory).free >= self.limits.minimum_free, 507, 'Insufficient workshop disk headroom')
            with self.repository.db() as db:
                db.execute('BEGIN IMMEDIATE')
                actor, current = self._get(db, actor, upload_id)
                demand(current['state'] == 'receiving' and current['lease'] == lease
                       and self.accounts.now() < current['lease_until'], 409, 'Upload expired or changed')
                owner = self._owner(db, actor, row['mod_id'])
                if owner is None:
                    count = db.execute('SELECT COUNT(*) FROM workshop_owners WHERE account_id=?', (actor.id,)).fetchone()[0]
                    demand(count < self.limits.mods_account, 429, 'Creator mod-ID limit reached')
                    db.execute('INSERT INTO workshop_owners VALUES(?,?)', (row['mod_id'], actor.id))
                demand(not ready.exists() and not ready.is_symlink(), 503, 'Unexpected private upload file')
                part.rename(ready)
                db.execute("UPDATE workshop_uploads SET state='queued',lease='',lease_until=0 WHERE id=?", (upload_id,))
                self.accounts.audit(db, actor.username, 'upload-queued', upload_id, row['sha256'])
            published = True
        except BaseException:
            if not published:
                self._remove(upload_id, '.part')
                self._remove(upload_id, '.ready')
                with self.repository.db() as db:
                    db.execute('BEGIN IMMEDIATE')
                    db.execute("UPDATE workshop_uploads SET state='failed',error='upload-invalid' "
                               "WHERE id=? AND state='receiving' AND lease=?", (upload_id, lease))
            raise
        return self.status(actor, upload_id)

    def status(self, actor, upload_id):
        with self.repository.db() as db:
            actor, row = self._get(db, actor, upload_id)
            result = {key: row[key] for key in ('id','mod_id','version','sha256','size','created','expires','state','retry_at','error')}
            if row['state'] == 'complete':
                release = db.execute('SELECT state FROM releases WHERE sha256=?', (row['sha256'],)).fetchone()
                if release is not None:
                    result['state'] = release[0]
            if row['state'] == 'open' and self.accounts.now() >= row['expires']:
                result['state'] = 'expired'
            return result

    def list(self, actor, after=0):
        demand(type(after) is int and after >= 0, 400, 'Invalid upload cursor')
        with self.repository.db() as db:
            actor = self.accounts.actor(db, actor)
            rows = db.execute('SELECT rowid AS cursor,id FROM workshop_uploads WHERE account_id=? AND rowid>? '
                              'ORDER BY rowid LIMIT 100', (actor.id, after)).fetchall()
        return [dict(self.status(actor, row['id']), cursor=row['cursor']) for row in rows]

    def moderate(self, actor, upload_id, action, note):
        demand(action in ('approve','revoke'), 400, 'Invalid review action')
        demand(isinstance(note, str) and 0 < len(note.strip()) <= 1800, 400, 'Record a review note')
        self._path(upload_id, '.ready')
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor = self.accounts.actor(db, actor, ('moderator','admin'))
            row = db.execute('SELECT * FROM workshop_uploads WHERE id=?', (upload_id,)).fetchone()
            demand(row is not None and row['state'] == 'complete', 409, 'Upload is not ready for moderation')
            if action == 'approve':
                owner = db.execute('SELECT active FROM workshop_accounts WHERE id=?', (row['account_id'],)).fetchone()
                demand(owner is not None and owner[0], 409, 'Creator is suspended; operator review required')
                self.repository.approve_in_transaction(db, row['sha256'], actor.username + ': ' + note.strip())
            else:
                self.repository.revoke_in_transaction(db, row['sha256'], actor.username + ': ' + note.strip())
            self.accounts.audit(db, actor.username, action, row['sha256'], note.strip())
            state = db.execute('SELECT state FROM releases WHERE sha256=?', (row['sha256'],)).fetchone()[0]
        return {'id': upload_id, 'sha256': row['sha256'], 'state': state}

    def reviews(self, actor, after=0):
        demand(type(after) is int and after >= 0, 400, 'Invalid review cursor')
        with self.repository.db() as db:
            self.accounts.actor(db, actor, ('moderator','admin'))
            rows = db.execute("SELECT u.rowid AS cursor,u.id,u.mod_id,u.version,u.sha256,r.state,r.manifest "
                              "FROM workshop_uploads u JOIN releases r ON u.sha256=r.sha256 "
                              "WHERE u.state='complete' AND u.rowid>? ORDER BY u.rowid LIMIT 100", (after,)).fetchall()
            return [dict(row) for row in rows]

    def assign_owner(self, mod_id, name, note):
        """Local operator-only adoption/transfer, including legacy reviewed mods."""
        name = username(name)
        demand(isinstance(mod_id, str) and len(mod_id) <= 80 and ID.fullmatch(mod_id), 400, 'Invalid mod ID')
        demand(isinstance(note, str) and 0 < len(note.strip()) <= 1800, 400, 'Record an ownership reason')
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            target = db.execute('SELECT id FROM workshop_accounts WHERE username=? AND active=1', (name,)).fetchone()
            demand(target is not None, 404, 'Creator unavailable')
            db.execute('INSERT INTO workshop_owners VALUES(?,?) ON CONFLICT(mod_id) DO UPDATE SET account_id=excluded.account_id',
                       (mod_id, target['id']))
            self.accounts.audit(db, 'operator', 'owner-assigned', mod_id, name + ': ' + note.strip())

    @contextmanager
    def _worker_lock(self):
        path = self.repository.root / 'scan-worker.lock'
        demand(not path.is_symlink(), 503, 'Worker lock is linked')
        with path.open('a+b') as stream:
            stream.seek(0)
            if os.fstat(stream.fileno()).st_size == 0:
                stream.write(b'0')
                stream.flush()
            stream.seek(0)
            try:
                if os.name == 'nt':
                    import msvcrt
                    msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError as exc:
                if exc.errno in (errno.EACCES, errno.EAGAIN):
                    raise ServiceError(409, 'Scanner worker is already running') from exc
                raise
            try:
                yield
            finally:
                stream.seek(0)
                if os.name == 'nt':
                    msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
                else:
                    fcntl.flock(stream, fcntl.LOCK_UN)

    def process_next(self, scanner):
        """One serial, idempotent job; clean results still await manual review."""
        with self._worker_lock():
            now = self.accounts.now()
            lease = secrets.token_hex(16)
            with self.repository.db() as db:
                db.execute('BEGIN IMMEDIATE')
                self._expire(db)
                # Expired worker leases exceed the child180s self-deadline.
                db.execute("UPDATE workshop_uploads SET state='retry',retry_at=? WHERE state='processing' AND lease_until<=?",
                           (now, now))
                if db.execute("SELECT 1 FROM workshop_uploads WHERE state='processing' LIMIT 1").fetchone():
                    return None
                row = db.execute("SELECT * FROM workshop_uploads WHERE state='queued' OR (state='retry' AND retry_at<=?) "
                                 'ORDER BY created,id LIMIT 1', (now,)).fetchone()
                if row is None:
                    return None
                demand(shutil.disk_usage(self.directory).free >= self.limits.minimum_free, 507, 'Insufficient scanner disk headroom')
                account = db.execute('SELECT active FROM workshop_accounts WHERE id=?', (row['account_id'],)).fetchone()
                if account is None or not account[0]:
                    db.execute("UPDATE workshop_uploads SET state='held',error='creator-unavailable' WHERE id=?", (row['id'],))
                    return {'id': row['id'], 'state': 'held'}
                db.execute("UPDATE workshop_uploads SET state='processing',lease=?,lease_until=?,attempts=attempts+1 WHERE id=?",
                           (lease, now + WORK_TTL, row['id']))
            try:
                with self.repository.db() as db:
                    existing = db.execute('SELECT sha256,state FROM releases WHERE mod_id=? AND version=?',
                                          (row['mod_id'], row['version'])).fetchone()
                if existing is None:
                    result = self.repository.submit(self._path(row['id'], '.ready'), scanner, expected_sha=row['sha256'])
                    demand(result['sha256'] == row['sha256'], 409, 'Queued package changed')
                else:
                    demand(existing['sha256'] == row['sha256'], 409, 'Queued release identity changed')
                    if existing['state'] == 'scan-error':
                        self.repository.rescan(row['sha256'], scanner)
                release = self.repository.get(row['sha256'])
                retry = release['state'] == 'scan-error' and row['attempts'] + 1 < 3
                with self.repository.db() as db:
                    db.execute('BEGIN IMMEDIATE')
                    changed = db.execute('UPDATE workshop_uploads SET state=?,retry_at=?,lease=\'\',lease_until=0,error=? '
                                         "WHERE id=? AND state='processing' AND lease=?",
                                         ('retry' if retry else 'complete', self.accounts.now() + 300 if retry else 0,
                                          'scan-unavailable' if release['state'] == 'scan-error' else '', row['id'], lease)).rowcount
                    demand(changed == 1, 409, 'Scan job changed')
                    self.accounts.audit(db, 'scanner', 'upload-scanned', row['id'], release['state'])
                self._remove(row['id'], '.ready')
                return {'id': row['id'], 'state': release['state'], 'retry': retry}
            except (ModError, OSError, ServiceError):
                with self.repository.db() as db:
                    db.execute("UPDATE workshop_uploads SET state='held',error='scan-job-failed' "
                               "WHERE id=? AND state='processing' AND lease=?", (row['id'], lease))
                raise
