"""Persistent creator identities and revocable sessions; no credentials in public metadata."""
from dataclasses import dataclass
import hashlib
import hmac
import re
import secrets
import threading
import time
import unicodedata

USERNAME = re.compile(r'[a-z][a-z0-9-]{2,31}\Z')
TOKEN = re.compile(r'[A-Za-z0-9_-]{43}\Z')
ROLES = ('creator', 'moderator', 'admin')
SESSION_TTL = 12 * 60 * 60
INVITE_TTL = 24 * 60 * 60
MAX_ACCOUNTS = 10000
MAX_AUDIT = 100000
_password_slot = threading.BoundedSemaphore(1)


class ServiceError(ValueError):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status


def demand(condition, status, message):
    if not condition:
        raise ServiceError(status, message)


def token_hash(token):
    demand(isinstance(token, str) and TOKEN.fullmatch(token), 401, 'Authentication required')
    return hashlib.sha256(token.encode('ascii')).hexdigest()


def username(value):
    demand(isinstance(value, str) and USERNAME.fullmatch(value), 400,
           'Use a lowercase creator name of 3–32 letters, digits or hyphens')
    return value


def password_bytes(value, minimum=1):
    demand(isinstance(value, str), 400, 'Invalid password')
    value = unicodedata.normalize('NFC', value)
    demand(minimum <= len(value) <= 256, 400, 'Password must contain 15–256 characters')
    try:
        encoded = value.encode('utf-8')
    except UnicodeError as exc:
        raise ServiceError(400, 'Invalid password') from exc
    demand(len(encoded) <= 1024, 400, 'Invalid password')
    return encoded


def password_hash(encoded, salt):
    # OWASP's scrypt profile:128MiB, N=2^17/r8/p1. One derivation per process.
    demand(_password_slot.acquire(blocking=False), 429, 'Authentication busy; retry later')
    try:
        return hashlib.scrypt(encoded, salt=salt, n=2**17, r=8, p=1,
                              maxmem=256 * 1024 * 1024, dklen=32)
    finally:
        _password_slot.release()


@dataclass(frozen=True)
class Actor:
    id: int
    username: str
    role: str
    session_hash: str


class Accounts:
    """The operator owns the repository directory; HTTP cannot create privileged invitations."""
    def __init__(self, repository, clock=time.time):
        self.repository = repository
        self.clock = clock
        with repository.db() as db:
            db.executescript('''
                CREATE TABLE IF NOT EXISTS workshop_accounts (
                    id INTEGER PRIMARY KEY, username TEXT NOT NULL UNIQUE,
                    role TEXT NOT NULL CHECK(role IN ('creator','moderator','admin')),
                    salt BLOB NOT NULL, password_hash BLOB NOT NULL,
                    active INTEGER NOT NULL CHECK(active IN (0,1)), created INTEGER NOT NULL);
                CREATE TABLE IF NOT EXISTS workshop_invites (
                    token_hash TEXT PRIMARY KEY, username TEXT NOT NULL UNIQUE,
                    role TEXT NOT NULL, created INTEGER NOT NULL, expires INTEGER NOT NULL,
                    account_id INTEGER REFERENCES workshop_accounts(id));
                CREATE TABLE IF NOT EXISTS workshop_sessions (
                    token_hash TEXT PRIMARY KEY, account_id INTEGER NOT NULL
                        REFERENCES workshop_accounts(id) ON DELETE CASCADE,
                    created INTEGER NOT NULL, expires INTEGER NOT NULL);
                CREATE INDEX IF NOT EXISTS workshop_sessions_account ON workshop_sessions(account_id);
                CREATE TABLE IF NOT EXISTS workshop_rate (
                    scope TEXT NOT NULL, identity TEXT NOT NULL,
                    window INTEGER NOT NULL, count INTEGER NOT NULL,
                    PRIMARY KEY(scope,identity));
                CREATE TABLE IF NOT EXISTS workshop_audit (
                    sequence INTEGER PRIMARY KEY, time INTEGER NOT NULL,
                    actor TEXT NOT NULL, action TEXT NOT NULL, target TEXT NOT NULL, note TEXT NOT NULL);
            ''')

    def now(self):
        return int(self.clock())

    def audit(self, db, actor, action, target, note=''):
        demand(db.execute('SELECT COUNT(*) FROM workshop_audit').fetchone()[0] < MAX_AUDIT,
               503, 'Workshop audit capacity reached; operator action required')
        demand(len(note) <= 2000, 400, 'Audit note too long')
        db.execute('INSERT INTO workshop_audit(time,actor,action,target,note) VALUES(?,?,?,?,?)',
                   (self.now(), actor, action, target, note))

    def rate(self, db, scope, identity, limit, seconds):
        """Persistent bounded counters; callers commit denied attempts too."""
        now = self.now()
        identity = hashlib.sha256(identity.encode('utf-8')).hexdigest()
        window = now // seconds
        # At most10k identities plus a global counter can persist in a scope.
        db.execute('DELETE FROM workshop_rate WHERE scope=? AND window<>?', (scope, window))
        row = db.execute('SELECT count FROM workshop_rate WHERE scope=? AND identity=?',
                         (scope, identity)).fetchone()
        if row is None:
            count = db.execute('SELECT COUNT(*) FROM workshop_rate WHERE scope=?', (scope,)).fetchone()[0]
            if count >= 10000:
                return False
            db.execute('INSERT INTO workshop_rate VALUES(?,?,?,1)', (scope, identity, window))
            return True
        if row[0] >= limit:
            return False
        db.execute('UPDATE workshop_rate SET count=count+1 WHERE scope=? AND identity=?', (scope, identity))
        return True

    def authentication_rate(self, peer, account):
        demand(isinstance(peer, str) and 0 < len(peer) <= 128, 400, 'Invalid client identity')
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            permitted = self.rate(db, 'auth-global', 'all', 120, 60)
            if permitted:
                permitted = self.rate(db, 'auth-peer', peer, 30, 300)
            if permitted:
                permitted = self.rate(db, 'auth-account', account[:128], 5, 300)
        demand(permitted, 429, 'Too many authentication attempts; retry later')

    def create_invite(self, name, role='creator'):
        """Local operator command only. Return the one-time secret once to its secure UI/file."""
        name = username(name)
        demand(role in ROLES, 400, 'Invalid account role')
        secret = secrets.token_urlsafe(32)
        now = self.now()
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            db.execute('DELETE FROM workshop_invites WHERE expires<=?', (now,))
            count = db.execute('SELECT COUNT(*) FROM workshop_accounts').fetchone()[0]
            count += db.execute('SELECT COUNT(*) FROM workshop_invites').fetchone()[0]
            demand(count < MAX_ACCOUNTS, 503, 'Workshop account capacity reached')
            demand(db.execute('SELECT 1 FROM workshop_accounts WHERE username=?',
                              (name,)).fetchone() is None, 409, 'Creator name already exists')
            demand(db.execute('SELECT 1 FROM workshop_invites WHERE username=?',
                              (name,)).fetchone() is None, 409, 'Creator invitation already exists')
            db.execute('INSERT INTO workshop_invites(token_hash,username,role,created,expires) VALUES(?,?,?,?,?)',
                       (token_hash(secret), name, role, now, now + INVITE_TTL))
            self.audit(db, 'system:operator', 'invite-created', name, role)
        return secret

    def recovery_invite(self, name, note):
        name = username(name)
        demand(isinstance(note, str) and 0 < len(note.strip()) <= 1800, 400, 'Record an account recovery reason')
        secret = secrets.token_urlsafe(32)
        now = self.now()
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            row = db.execute('SELECT * FROM workshop_accounts WHERE username=?', (name,)).fetchone()
            demand(row is not None, 404, 'Creator unavailable')
            db.execute('DELETE FROM workshop_invites WHERE username=?', (name,))
            db.execute('INSERT INTO workshop_invites VALUES(?,?,?,?,?,?)',
                       (token_hash(secret), name, row['role'], now, now + INVITE_TTL, row['id']))
            db.execute('UPDATE workshop_accounts SET active=0 WHERE id=?', (row['id'],))
            db.execute('DELETE FROM workshop_sessions WHERE account_id=?', (row['id'],))
            self.audit(db, 'system:operator', 'account-recovery-issued', name, note.strip())
        return secret

    def cancel_invite(self, secret):
        """Local delivery-failure cleanup; no public cancellation capability."""
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            hashed = token_hash(secret)
            row = db.execute('SELECT username FROM workshop_invites WHERE token_hash=?', (hashed,)).fetchone()
            if row is not None:
                db.execute('DELETE FROM workshop_invites WHERE token_hash=?', (hashed,))
                self.audit(db, 'system:operator', 'invitation-cancelled', row['username'])

    def redeem(self, invitation, password, peer):
        self.authentication_rate(peer, 'invite:' + str(invitation)[:43])
        hashed = token_hash(invitation)
        with self.repository.db() as db:
            invite = db.execute('SELECT * FROM workshop_invites WHERE token_hash=?', (hashed,)).fetchone()
        now = self.now()
        demand(invite is not None and invite['created'] <= now < invite['expires'],
               401, 'Invitation unavailable or expired')
        encoded = password_bytes(password, minimum=15)
        salt = secrets.token_bytes(16)
        derived = password_hash(encoded, salt)
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            current = db.execute('SELECT * FROM workshop_invites WHERE token_hash=?', (hashed,)).fetchone()
            now = self.now()
            demand(current is not None and current['created'] <= now < current['expires'],
                   401, 'Invitation unavailable or expired')
            if current['account_id'] is None:
                cursor = db.execute('INSERT INTO workshop_accounts(username,role,salt,password_hash,active,created) '
                                    'VALUES(?,?,?,?,1,?)', (current['username'], current['role'], salt, derived, now))
                account_id = cursor.lastrowid
                action = 'account-created'
            else:
                account_id = current['account_id']
                db.execute('UPDATE workshop_accounts SET salt=?,password_hash=?,active=1 WHERE id=?',
                           (salt, derived, account_id))
                action = 'account-recovered'
            db.execute('DELETE FROM workshop_invites WHERE token_hash=?', (hashed,))
            self.audit(db, 'user:' + current['username'], action, current['username'])
            return self._session(db, account_id)

    def _session(self, db, account_id):
        now = self.now()
        db.execute('DELETE FROM workshop_sessions WHERE expires<=?', (now,))
        # Retain at most5 devices. Oldest credentials become unusable immediately.
        rows = db.execute('SELECT token_hash FROM workshop_sessions WHERE account_id=? '
                          'ORDER BY created DESC, rowid DESC', (account_id,)).fetchall()
        for row in rows[4:]:
            db.execute('DELETE FROM workshop_sessions WHERE token_hash=?', (row[0],))
        secret = secrets.token_urlsafe(32)
        db.execute('INSERT INTO workshop_sessions VALUES(?,?,?,?)',
                   (token_hash(secret), account_id, now, now + SESSION_TTL))
        return {'token': secret, 'expires': now + SESSION_TTL}

    def login(self, name, password, peer):
        self.authentication_rate(peer, str(name)[:128])
        name = username(name)
        encoded = password_bytes(password)
        with self.repository.db() as db:
            account = db.execute('SELECT * FROM workshop_accounts WHERE username=?', (name,)).fetchone()
        salt = bytes(account['salt']) if account is not None else b'TSFP-dummy-salt01'
        expected = bytes(account['password_hash']) if account is not None else bytes(32)
        actual = password_hash(encoded, salt)
        valid = hmac.compare_digest(actual, expected)
        demand(valid and account is not None and account['active'], 401, 'Sign-in failed')
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            current = db.execute('SELECT * FROM workshop_accounts WHERE id=?', (account['id'],)).fetchone()
            demand(current['active'] and hmac.compare_digest(bytes(current['password_hash']), expected),
                   401, 'Sign-in failed')
            self.audit(db, 'user:' + name, 'session-created', name)
            return self._session(db, account['id'])

    def actor(self, db, actor, roles=ROLES):
        hashed = actor.session_hash if isinstance(actor, Actor) else token_hash(actor)
        row = db.execute('SELECT a.*,s.created AS session_created,s.expires FROM workshop_accounts a '
                         'JOIN workshop_sessions s ON a.id=s.account_id WHERE s.token_hash=?', (hashed,)).fetchone()
        now = self.now()
        demand(row is not None and row['active'] and row['session_created'] <= now < row['expires'],
               401, 'Authentication required')
        demand(row['role'] in roles, 403, 'Permission denied')
        return Actor(row['id'], row['username'], row['role'], hashed)

    def authenticate(self, token):
        with self.repository.db() as db:
            return self.actor(db, token)

    def logout(self, actor):
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor = self.actor(db, actor)
            db.execute('DELETE FROM workshop_sessions WHERE token_hash=?', (actor.session_hash,))
            self.audit(db, 'user:' + actor.username, 'session-revoked', actor.username)

    def change_password(self, actor, old_password, new_password, peer):
        self.authentication_rate(peer, actor.username)
        with self.repository.db() as db:
            actor = self.actor(db, actor)
            row = db.execute('SELECT * FROM workshop_accounts WHERE id=?', (actor.id,)).fetchone()
        expected = bytes(row['password_hash'])
        demand(hmac.compare_digest(password_hash(password_bytes(old_password), bytes(row['salt'])), expected),
               401, 'Sign-in failed')
        salt = secrets.token_bytes(16)
        derived = password_hash(password_bytes(new_password, minimum=15), salt)
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            actor = self.actor(db, actor)
            changed = db.execute('UPDATE workshop_accounts SET salt=?,password_hash=? WHERE id=? AND password_hash=?',
                                 (salt, derived, actor.id, expected)).rowcount
            demand(changed == 1, 409, 'Account changed; sign in again')
            db.execute('DELETE FROM workshop_sessions WHERE account_id=?', (actor.id,))
            self.audit(db, 'user:' + actor.username, 'password-changed', actor.username)

    def suspend(self, name, note):
        """Local operator recovery, including an account whose sessions were stolen."""
        name = username(name)
        demand(isinstance(note, str) and 0 < len(note.strip()) <= 2000, 400, 'Record a suspension reason')
        with self.repository.db() as db:
            db.execute('BEGIN IMMEDIATE')
            row = db.execute('SELECT id FROM workshop_accounts WHERE username=?', (name,)).fetchone()
            demand(row is not None, 404, 'Unknown creator')
            db.execute('UPDATE workshop_accounts SET active=0 WHERE id=?', (row['id'],))
            db.execute('DELETE FROM workshop_sessions WHERE account_id=?', (row['id'],))
            self.audit(db, 'system:operator', 'account-suspended', name, note.strip())
