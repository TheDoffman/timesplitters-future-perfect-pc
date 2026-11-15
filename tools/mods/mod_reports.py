"""Authenticated release reports, private to the reporter and moderators."""
import re
import secrets

from mod_accounts import ServiceError, demand
from mod_package import SHA

PAGE = 20
MAX_REPORTS = 10000
REASONS = ('malware', 'broken', 'policy', 'other')
REPORT_ID = re.compile(r'[0-9a-f]{32}\Z')


def text(value, maximum):
    demand(isinstance(value, str) and 0 < len(value.strip()) <= maximum
           and not any(ord(c) < 32 or 127 <= ord(c) < 160 for c in value), 400, 'Enter a bounded report note')
    try:
        value.encode('utf-8')
        demand(len(value.strip().encode('utf-16-le')) // 2 <= maximum, 400, 'Report note exceeds native text limit')
    except UnicodeError as exc:
        raise ServiceError(400, 'Invalid report text') from exc
    return value.strip()


class Reports:
    def __init__(self, accounts):
        self.accounts = accounts
        self.repository = accounts.repository
        with self.repository.db() as db:
            db.executescript('''
                CREATE TABLE IF NOT EXISTS workshop_reports (
                    sequence INTEGER PRIMARY KEY, id TEXT NOT NULL UNIQUE,
                    account_id INTEGER NOT NULL REFERENCES workshop_accounts(id),
                    sha256 TEXT NOT NULL REFERENCES releases(sha256), reason TEXT NOT NULL,
                    description TEXT NOT NULL, created INTEGER NOT NULL, updated INTEGER NOT NULL,
                    state TEXT NOT NULL CHECK(state IN ('open','closed')), resolution TEXT NOT NULL,
                    UNIQUE(account_id,sha256));
                CREATE INDEX IF NOT EXISTS workshop_reports_account ON workshop_reports(account_id,sequence);
                CREATE INDEX IF NOT EXISTS workshop_reports_state ON workshop_reports(state,sequence);
            ''')

    def _row(self, db, report_id):
        row = db.execute('SELECT q.sequence AS cursor,q.id,q.sha256,r.mod_id,r.version,a.username AS reporter,'
                         'q.reason,q.description,q.created,q.updated,q.state,q.resolution FROM workshop_reports q '
                         'JOIN releases r ON r.sha256=q.sha256 JOIN workshop_accounts a ON a.id=q.account_id '
                         'WHERE q.id=?', (report_id,)).fetchone()
        demand(row is not None, 404, 'Report unavailable')
        return dict(row)

    def create(self, actor, sha, reason, description):
        demand(isinstance(sha, str) and SHA.fullmatch(sha) and isinstance(reason, str) and reason in REASONS,
               400, 'Select a release and report category')
        description = text(description, 1000)
        # Persist rate admission even when a later duplicate/storage/audit check refuses.
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor = self.accounts.actor(db, actor)
            allowed = self.accounts.rate(db, 'report-account', str(actor.id), 5, 86400)
            if allowed:
                allowed = self.accounts.rate(db, 'report-global', 'all', 200, 86400)
        demand(allowed, 429, 'Report limit reached; retry later')
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor = self.accounts.actor(db, actor)
            # Previously approved but now revoked/withheld installed releases remain reportable.
            demand(db.execute("SELECT 1 FROM releases r WHERE r.sha256=? AND EXISTS "
                              "(SELECT 1 FROM events e WHERE e.sha256=r.sha256 AND e.action='approved')", (sha,)).fetchone(),
                   404, 'Published release unavailable')
            demand(not db.execute('SELECT 1 FROM workshop_reports WHERE account_id=? AND sha256=?',
                                  (actor.id, sha)).fetchone(), 409, 'This account already reported this release')
            demand(db.execute('SELECT COUNT(*) FROM workshop_reports').fetchone()[0] < MAX_REPORTS,
                   503, 'Report capacity requires operator attention')
            report_id, now = secrets.token_hex(16), self.accounts.now()
            db.execute('INSERT INTO workshop_reports(id,account_id,sha256,reason,description,created,updated,state,resolution) '
                       "VALUES(?,?,?,?,?,?,?,'open','')", (report_id, actor.id, sha, reason, description, now, now))
            self.accounts.audit(db, 'user:' + actor.username, 'report-created', report_id, sha + ': ' + reason)
            return self._row(db, report_id)

    def list(self, actor, after=0, moderation=False):
        demand(type(after) is int and 0 <= after <= 999999999, 400, 'Invalid report cursor')
        with self.repository.db() as db:
            db.execute('BEGIN')
            actor = self.accounts.actor(db, actor, ('moderator','admin')) if moderation else self.accounts.actor(db, actor)
            condition, values = ("state='open'", ()) if moderation else ('account_id=?', (actor.id,))
            rows = db.execute('SELECT id FROM workshop_reports WHERE ' + condition + ' AND sequence>? '
                              'ORDER BY sequence LIMIT ?', (*values, after, PAGE)).fetchall()
            return [self._row(db, row['id']) for row in rows]

    def moderate(self, actor, report_id, action, note):
        demand(isinstance(report_id, str) and REPORT_ID.fullmatch(report_id) and action in ('close','reopen'),
               400, 'Invalid report decision')
        note = text(note, 1800)
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor = self.accounts.actor(db, actor, ('moderator','admin'))
            before = self._row(db, report_id)
            demand(before['state'] == ('open' if action == 'close' else 'closed'), 409, 'Report state changed')
            db.execute('UPDATE workshop_reports SET state=?,resolution=?,updated=? WHERE id=?',
                       ('closed' if action == 'close' else 'open', note, self.accounts.now(), report_id))
            self.accounts.audit(db, 'user:' + actor.username, 'report-' + action, report_id, note)
            return self._row(db, report_id)
