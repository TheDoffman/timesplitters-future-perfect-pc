"""Operator-managed quarantine/catalog prototype; never serves unapproved bytes."""
from contextlib import contextmanager
from datetime import datetime, timezone
import json
from pathlib import Path
import sqlite3
import subprocess
import tempfile
import time

from mod_package import MAX_ARCHIVE, ModError, SHA, digest, inspect_package, require

SCAN_TTL = 24 * 60 * 60


class ClamScanner:
    """Fail closed on missing scanner, timeout, scan limits, and engine errors."""
    def __init__(self, executable='clamscan'):
        self.executable = executable

    def scan(self, folder):
        report = {'engine': 'ClamAV', 'version': '', 'status': 'error',
                  'time': int(time.time()), 'detail': ''}
        try:
            version = subprocess.run([self.executable, '--version'], capture_output=True,
                                     timeout=10, check=True)
            report['version'] = version.stdout.decode('utf-8', errors='replace')[:512].strip()
            require(report['version'].startswith('ClamAV '), 'Unexpected scanner identity')
            # Scan normalized extracted files. Every limit alert blocks approval.
            with tempfile.TemporaryFile() as log:
                result = subprocess.run(
                    [self.executable, '--recursive', '--infected', '--no-summary',
                     '--alert-exceeds-max=yes', '--max-filesize=65M', '--max-scansize=512M',
                     '--max-files=4096', '--max-recursion=8', str(Path(folder).resolve())],
                    stdout=log, stderr=subprocess.STDOUT, timeout=180)
                log.seek(0)
                report['detail'] = log.read(65536).decode('utf-8', errors='replace')
            report['status'] = {0: 'clean', 1: 'blocked'}.get(result.returncode, 'error')
        except (OSError, subprocess.SubprocessError, ModError) as exc:
            report['detail'] = str(exc)[:2048]
        return report


class Repository:
    """Trusted local operator storage. Do not let uploaders write this directory."""
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.root.mkdir(parents=True, exist_ok=True)
        (self.root / 'blobs').mkdir(exist_ok=True)
        with self.db() as db:
            db.executescript('''
                CREATE TABLE IF NOT EXISTS releases (
                    sha256 TEXT PRIMARY KEY, mod_id TEXT NOT NULL, version TEXT NOT NULL,
                    manifest TEXT NOT NULL, state TEXT NOT NULL, scan TEXT NOT NULL,
                    UNIQUE(mod_id, version));
                CREATE TABLE IF NOT EXISTS events (
                    sequence INTEGER PRIMARY KEY, sha256 TEXT NOT NULL,
                    time TEXT NOT NULL, action TEXT NOT NULL, note TEXT NOT NULL);
            ''')

    @contextmanager
    def db(self):
        db = sqlite3.connect(self.root / 'catalog.sqlite', timeout=10)
        db.row_factory = sqlite3.Row
        try:
            with db:
                yield db
        finally:
            db.close()

    def blob(self, sha):
        require(isinstance(sha, str) and SHA.fullmatch(sha), 'Invalid release digest')
        return self.root / 'blobs' / (sha + '.zip')

    @staticmethod
    def event(db, sha, action, note):
        db.execute('INSERT INTO events(sha256,time,action,note) VALUES(?,?,?,?)',
                   (sha, datetime.now(timezone.utc).isoformat(), action, note))

    def submit(self, source, scanner):
        source = Path(source)
        require(source.stat().st_size <= MAX_ARCHIVE, 'Archive too large')
        # A private snapshot binds validation, scan and moderation to identical bytes.
        with tempfile.TemporaryDirectory(dir=self.root) as temporary:
            snapshot = Path(temporary) / 'upload.zip'
            with source.open('rb') as stream:
                data = stream.read(MAX_ARCHIVE + 1)
            require(len(data) <= MAX_ARCHIVE, 'Archive too large')
            sha = digest(data)
            snapshot.write_bytes(data)
            content = Path(temporary) / 'content'
            content.mkdir()
            manifest = inspect_package(snapshot, content)
            report = scanner.scan(content)
            require(report.get('status') in ('clean', 'blocked', 'error'), 'Invalid scan report')
            state = {'clean': 'pending-review', 'blocked': 'rejected', 'error': 'scan-error'}[report['status']]
            with self.db() as db:
                db.execute('BEGIN IMMEDIATE')
                require(db.execute('SELECT 1 FROM releases WHERE mod_id=? AND version=?',
                                   (manifest['id'], manifest['version'])).fetchone() is None,
                        'This mod version already exists; use a new version or rescan')
                target = self.blob(sha)
                if target.exists():
                    require(digest(target.read_bytes()) == sha, 'Stored blob was modified')
                else:
                    # Orphans after a crash are harmless: catalog approval is authoritative.
                    with target.open('xb') as stream:
                        stream.write(data)
                db.execute('INSERT INTO releases VALUES(?,?,?,?,?,?)',
                           (sha, manifest['id'], manifest['version'], json.dumps(manifest),
                            state, json.dumps(report)))
                self.event(db, sha, 'submitted', state)
            return {'sha256': sha, 'state': state, 'scan': report}

    def get(self, sha):
        self.blob(sha)
        with self.db() as db:
            row = db.execute('SELECT * FROM releases WHERE sha256=?', (sha,)).fetchone()
        require(row is not None, 'Unknown release')
        return dict(row)

    @staticmethod
    def fresh(report):
        age = time.time() - report.get('time', 0)
        return report.get('status') == 'clean' and 0 <= age < SCAN_TTL

    def approve(self, sha, reviewer_note):
        require(isinstance(reviewer_note, str) and 0 < len(reviewer_note.strip()) <= 2000,
                'Record a reviewer and review note')
        with self.db() as db:
            db.execute('BEGIN IMMEDIATE')
            row = db.execute('SELECT * FROM releases WHERE sha256=?', (sha,)).fetchone()
            require(row is not None and row['state'] == 'pending-review', 'Release is not awaiting review')
            require(self.fresh(json.loads(row['scan'])), 'A recent clean scan is required')
            data = self.blob(sha).read_bytes()
            require(digest(data) == sha, 'Stored blob was modified')
            inspect_package(self.blob(sha))
            db.execute("UPDATE releases SET state='approved' WHERE sha256=?", (sha,))
            self.event(db, sha, 'approved', reviewer_note.strip())

    def revoke(self, sha, reason):
        require(isinstance(reason, str) and 0 < len(reason.strip()) <= 2000, 'Revocation needs a reason')
        with self.db() as db:
            db.execute('BEGIN IMMEDIATE')
            require(db.execute('SELECT 1 FROM releases WHERE sha256=?', (sha,)).fetchone(),
                    'Unknown release')
            db.execute("UPDATE releases SET state='revoked' WHERE sha256=?", (sha,))
            self.event(db, sha, 'revoked', reason.strip())

    def rescan(self, sha, scanner):
        with self.db() as db:
            # Serialize admin mutations, so a concurrent revoke cannot be undone.
            db.execute('BEGIN IMMEDIATE')
            row = db.execute('SELECT * FROM releases WHERE sha256=?', (sha,)).fetchone()
            require(row is not None and row['state'] != 'revoked', 'Unknown or revoked release')
            require(digest(self.blob(sha).read_bytes()) == sha, 'Stored blob was modified')
            with tempfile.TemporaryDirectory(dir=self.root) as temporary:
                inspect_package(self.blob(sha), temporary)
                report = scanner.scan(temporary)
            require(report.get('status') in ('clean', 'blocked', 'error'), 'Invalid scan report')
            state = {'clean': 'pending-review', 'blocked': 'rejected', 'error': 'scan-error'}[report['status']]
            db.execute('UPDATE releases SET state=?,scan=? WHERE sha256=?',
                       (state, json.dumps(report), sha))
            self.event(db, sha, 'rescanned', state)
        return {'sha256': sha, 'state': state, 'scan': report}

    def catalog(self, query=''):
        require(isinstance(query, str) and len(query) <= 200, 'Search query too long')
        with self.db() as db:
            rows = db.execute("SELECT * FROM releases WHERE state='approved' ORDER BY mod_id,version").fetchall()
        results = []
        for row in rows:
            m, scan = json.loads(row['manifest']), json.loads(row['scan'])
            text = ' '.join(m[k] for k in ('id', 'name', 'description', 'author')).casefold()
            if self.fresh(scan) and query.casefold() in text:
                results.append({'manifest': m, 'sha256': row['sha256'],
                                'download': '/v1/packages/' + row['sha256'],
                                'scan': {'engine': scan['engine'], 'version': scan['version'],
                                         'time': scan['time']}})
        return results

    def approved_blob(self, sha):
        row = self.get(sha)
        require(row['state'] == 'approved' and self.fresh(json.loads(row['scan'])),
                'Release is unavailable, revoked or awaiting a fresh scan/review')
        path = self.blob(sha)
        require(path.stat().st_size <= MAX_ARCHIVE and digest(path.read_bytes()) == sha,
                'Stored blob integrity failure')
        return path


def stage_install(package, expected_sha, mods_root):
    """Atomically stage an inactive package; never modify game data or execute Lua.

    mods_root is private operator/player storage, not an attacker-writable tree.
    Immutable version directories avoid partial upgrades and retain rollback.
    """
    require(isinstance(expected_sha, str) and SHA.fullmatch(expected_sha), 'Expected SHA-256 is required')
    package = Path(package)
    require(package.stat().st_size <= MAX_ARCHIVE, 'Archive too large')
    root = Path(mods_root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.install-', dir=root) as temporary:
        snapshot = Path(temporary) / 'package.zip'
        with package.open('rb') as stream:
            data = stream.read(MAX_ARCHIVE + 1)
        require(len(data) <= MAX_ARCHIVE and digest(data) == expected_sha, 'Download digest mismatch')
        snapshot.write_bytes(data)
        content = Path(temporary) / 'content'
        content.mkdir()
        manifest = inspect_package(snapshot, content)
        for mod_id, version in manifest['dependencies'].items():
            dependency = root / (mod_id + '@' + version)
            require(dependency.is_dir() and not dependency.is_symlink() and not dependency.is_junction(),
                    f'Install dependency first: {mod_id}@{version}')
            receipt = read_receipt(dependency)
            installed = inspect_package(dependency / 'package.zip')
            require(receipt['sha256'] == digest((dependency / 'package.zip').read_bytes())
                    and installed['id'] == mod_id and installed['version'] == version,
                    'Installed dependency integrity failure')
        target = root / (manifest['id'] + '@' + manifest['version'])
        require(not target.exists() and not target.is_symlink(), 'Version already installed')
        receipt = {'schema': 1, 'sha256': expected_sha, 'id': manifest['id'], 'version': manifest['version'],
                   'state': 'staged-inactive', 'installed_at': int(time.time())}
        (Path(temporary) / 'receipt.json').write_text(json.dumps(receipt), encoding='utf-8')
        # Rename only after the entire package and receipt have been verified/written.
        Path(temporary).rename(target)
    return receipt


def read_receipt(directory):
    from mod_package import read_json
    receipt = read_json((Path(directory) / 'receipt.json').read_bytes())
    require(isinstance(receipt, dict) and isinstance(receipt.get('sha256'), str)
            and SHA.fullmatch(receipt['sha256']), 'Invalid installation receipt')
    return receipt
