"""Private Mac/POSIX operator snapshots and fail-closed, new-directory recovery.

No archives are extracted, no keys or operator secrets are copied, and no existing
repository/backup is replaced. Incomplete output survives for operator inspection.
"""
import argparse
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sqlite3
import stat
import time

from mod_package import MAX_ARCHIVE, SHA, ModError, read_json, require

MAX_DB = 256 * 1024**2
MAX_TOTAL = 8 * 1024**3
MAX_FILES = 4096
MANIFEST = 'backup.json'
BARRIER = '.recovery-required'
TABLES = {'releases', 'events', 'workshop_accounts', 'workshop_invites',
          'workshop_sessions', 'workshop_rate', 'workshop_audit',
          'workshop_owners', 'workshop_uploads', 'publication_sequence', 'recovery_revocations', 'periodic_scans'}


def private_directory(path):
    require(os.name == 'posix', 'Run private host recovery on the Mac/POSIX host')
    path = Path(os.path.abspath(path))
    for member in (path, *path.parents):
        require(not member.is_symlink(), 'Linked host path refused')
    info = path.stat()
    require(stat.S_ISDIR(info.st_mode) and info.st_uid == os.geteuid()
            and info.st_mode & 0o077 == 0, 'Host directory must be owned and mode0700')
    return path


def fresh_directory(path):
    path = Path(os.path.abspath(path))
    private_directory(path.parent)
    require(not path.exists() and not path.is_symlink(), 'Choose a new recovery/backup directory')
    path.mkdir(mode=0o700)
    return private_directory(path)


def regular(path, limit):
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1
            and info.st_uid == os.geteuid() and info.st_mode & 0o022 == 0
            and 0 < info.st_size <= limit, 'Unsafe, empty or oversized host file')
    return info


def copy_file(source, target, limit, expected=None):
    before = regular(source, limit)
    require(expected is None or before.st_size == expected['size'], 'Backup integrity check failed')
    digest = hashlib.sha256()
    flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK
    with os.fdopen(os.open(source, flags), 'rb') as incoming:
        held = os.fstat(incoming.fileno())
        require((held.st_dev, held.st_ino, held.st_size) ==
                (before.st_dev, before.st_ino, before.st_size), 'Host file changed before capture')
        with os.fdopen(os.open(target, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600), 'wb') as output:
            total = 0
            while chunk := incoming.read(1024 * 1024):
                total += len(chunk)
                require(total <= before.st_size, 'Host file grew during capture')
                output.write(chunk)
                digest.update(chunk)
            require(total == before.st_size, 'Host file shortened during capture')
            after = os.fstat(incoming.fileno())
            require((after.st_size, after.st_mtime_ns, after.st_ctime_ns) ==
                    (before.st_size, before.st_mtime_ns, before.st_ctime_ns), 'Host file changed during capture')
            output.flush()
            os.fsync(output.fileno())
    record = {'size': total, 'sha256': digest.hexdigest()}
    require(expected is None or record == expected, 'Backup integrity check failed')
    return record


def write_json(path, value):
    wire = (json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n').encode()
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600), 'wb') as output:
        output.write(wire)
        output.flush()
        os.fsync(output.fileno())


def sync_directory(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def connect(path, mode='ro'):
    regular(path, MAX_DB)
    db = sqlite3.connect(path.as_uri() + '?mode=' + mode, uri=True, timeout=3)
    db.row_factory = sqlite3.Row
    db.execute('PRAGMA trusted_schema=OFF')
    db.execute('PRAGMA foreign_keys=ON')
    return db


def inventory(db):
    objects = db.execute("SELECT name,type FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' LIMIT 65").fetchall()
    require(len(objects) <= 64, 'Database schema budget exceeded')
    require(all(row['type'] in ('table', 'index') for row in objects), 'Unexpected database executable schema')
    tables = {row['name'] for row in objects if row['type'] == 'table'}
    require({'releases', 'events'} <= tables <= TABLES, 'Unknown database schema')
    require(db.execute('PRAGMA quick_check').fetchone()[0] == 'ok'
            and not db.execute('PRAGMA foreign_key_check').fetchall(), 'Database integrity check failed')
    require(db.execute('PRAGMA page_count').fetchone()[0] * db.execute('PRAGMA page_size').fetchone()[0]
            <= MAX_DB, 'Database snapshot exceeds its budget')
    if 'periodic_scans' in tables:
        require(not db.execute("SELECT 1 FROM periodic_scans WHERE lease<>'' LIMIT 1").fetchone(),
                'Periodic scanner is busy; quiesce it and retry the backup')
    refs = {}
    for row in db.execute('SELECT sha256 FROM releases LIMIT ?', (MAX_FILES + 1,)):
        require(isinstance(row[0], str) and SHA.fullmatch(row[0]) is not None, 'Invalid stored release identity')
        refs['blobs/' + row[0] + '.zip'] = (row[0], True)
    if 'workshop_uploads' in tables:
        require(not db.execute("SELECT 1 FROM workshop_uploads WHERE state IN ('receiving','processing') LIMIT 1").fetchone(),
                'Upload/scanner is busy; quiesce it and retry the backup')
        for row in db.execute("SELECT id,sha256,state FROM workshop_uploads WHERE state IN ('queued','retry','held') LIMIT ?", (MAX_FILES + 1,)):
            require(re.fullmatch('[0-9a-f]{32}', row['id']) and SHA.fullmatch(row['sha256']), 'Invalid queued upload identity')
            # Retried scans may already have a repository blob and no ready copy.
            refs['incoming/' + row['id'] + '.ready'] = (row['sha256'], row['state'] == 'queued' and
                                                       'blobs/' + row['sha256'] + '.zip' not in refs)
    require(len(refs) + 1 <= MAX_FILES, 'Backup file budget exceeded')
    return refs, tables


def snapshot(repository, destination):
    root = private_directory(repository)
    require(not (root / BARRIER).exists() and not (root / BARRIER).is_symlink(), 'Finish repository recovery before backing it up')
    target = Path(os.path.abspath(destination))
    require(not target.is_relative_to(root) and not root.is_relative_to(target), 'Backup and repository must be separate')
    private_directory(target.parent)
    # Reserved SQLite writer lock blocks new admissions/claims/moderation while
    # a second read connection captures the committed DB and immutable files.
    with closing(connect(root / 'catalog.sqlite', 'rw')) as lock:
        lock.execute('BEGIN IMMEDIATE')
        refs, _ = inventory(lock)
        estimate = lock.execute('PRAGMA page_count').fetchone()[0] * lock.execute('PRAGMA page_size').fetchone()[0]
        for name, (_, required) in refs.items():
            path = root / name
            if path.exists() or path.is_symlink():
                estimate += regular(path, MAX_ARCHIVE).st_size
            else:
                require(not required, 'Required upload/blob is missing')
        require(estimate <= MAX_TOTAL and shutil.disk_usage(target.parent).free >= estimate + MAX_DB,
                'Insufficient private backup disk headroom')
        output = fresh_directory(target)
        (output / 'blobs').mkdir(mode=0o700)
        (output / 'incoming').mkdir(mode=0o700)
        db_file = output / 'catalog.sqlite'
        descriptor = os.open(db_file, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        os.close(descriptor)
        with closing(connect(root / 'catalog.sqlite')) as source, closing(sqlite3.connect(db_file)) as backup:
            source.backup(backup, pages=128)
        files = {}
        total = db_file.stat().st_size
        require(0 < total <= MAX_DB, 'Database snapshot exceeds its budget')
        with db_file.open('rb') as stream:
            files['catalog.sqlite'] = {'size': total, 'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()}
            os.fsync(stream.fileno())
        for name, (sha, required) in sorted(refs.items()):
            parent = root / name.split('/')[0]
            private_directory(parent) if parent.name == 'incoming' else require(not parent.is_symlink(), 'Linked blob storage')
            source = root / name
            if not source.exists() and not source.is_symlink():
                require(not required, 'Required upload/blob is missing')
                continue
            size = regular(source, MAX_ARCHIVE).st_size
            total += size
            require(total <= MAX_TOTAL, 'Backup storage budget exceeded')
            record = copy_file(source, output / name, MAX_ARCHIVE)
            require(record['sha256'] == sha, 'Stored package digest changed')
            files[name] = record
        # A complete manifest is the commit marker. Interrupted output is kept
        # private and cannot pass restore validation.
        sync_directory(output / 'blobs')
        sync_directory(output / 'incoming')
        write_json(output / MANIFEST, {'schema': 1, 'created': int(time.time()), 'files': files})
        sync_directory(output)
    return {'backup': str(output), 'files': len(files), 'bytes': total}


def restore(backup, destination):
    source = private_directory(backup)
    regular(source / MANIFEST, 4 * 1024**2)
    manifest = read_json((source / MANIFEST).read_bytes())
    require(isinstance(manifest, dict) and set(manifest) == {'schema', 'created', 'files'} and type(manifest['schema']) is int
            and manifest['schema'] == 1 and type(manifest['created']) is int, 'Invalid backup manifest')
    files = manifest['files']
    require(isinstance(files, dict) and 0 < len(files) <= MAX_FILES and 'catalog.sqlite' in files, 'Invalid backup inventory')
    total = 0
    for name, record in files.items():
        require(name == 'catalog.sqlite' or re.fullmatch(r'blobs/[0-9a-f]{64}\.zip|incoming/[0-9a-f]{32}\.ready', name),
                'Unexpected backup path')
        require(isinstance(record, dict) and set(record) == {'sha256', 'size'}
                and isinstance(record['sha256'], str) and SHA.fullmatch(record['sha256'])
                and type(record['size']) is int and 0 < record['size'] <= (MAX_DB if name == 'catalog.sqlite' else MAX_ARCHIVE),
                'Invalid backup file metadata')
        total += record['size']
    require(total <= MAX_TOTAL, 'Backup storage budget exceeded')
    # Fixed three-level layout only. Never follow a link or accept extra files.
    observed = set()
    for entry in source.iterdir():
        require(not entry.is_symlink(), 'Linked backup entry')
        if entry.name in ('blobs', 'incoming'):
            private_directory(entry)
            for member in entry.iterdir():
                require(member.is_file() and not member.is_symlink(), 'Unexpected backup member')
                observed.add(entry.name + '/' + member.name)
                require(len(observed) <= MAX_FILES + 1, 'Backup file budget exceeded')
        else:
            observed.add(entry.name)
            require(len(observed) <= MAX_FILES + 1, 'Backup file budget exceeded')
    require(observed == set(files) | {MANIFEST}, 'Backup contains missing or unexpected files')
    target = Path(os.path.abspath(destination))
    require(not target.is_relative_to(source) and not source.is_relative_to(target), 'Backup and recovery must be separate')
    output = fresh_directory(target)
    (output / 'blobs').mkdir(mode=0o700)
    (output / 'incoming').mkdir(mode=0o700)
    # Written before the DB: even an interrupted restore stays unpublishable.
    write_json(output / BARRIER, {'schema': 1, 'backup_created': manifest['created'],
                                 'reason': 'Reconcile publication high-water/revocations before serving'})
    for name, record in sorted(files.items()):
        copy_file(source / name, output / name, MAX_DB if name == 'catalog.sqlite' else MAX_ARCHIVE, record)
    with closing(connect(output / 'catalog.sqlite', 'rw')) as db:
        refs, tables = inventory(db)
        for name, (sha, required) in refs.items():
            require(not required or name in files, 'Restored database refers to a missing package')
            if name in files:
                require(files[name]['sha256'] == sha, 'Restored package identity mismatch')
        with db:
            if 'workshop_sessions' in tables:
                db.execute('DELETE FROM workshop_sessions')
            if 'workshop_invites' in tables:
                db.execute('DELETE FROM workshop_invites')
            if 'workshop_accounts' in tables:
                db.execute('UPDATE workshop_accounts SET active=0')
            if 'periodic_scans' in tables:
                # A restored historical review cannot renew itself automatically.
                db.execute('DELETE FROM periodic_scans')
            if 'workshop_uploads' in tables:
                db.execute("UPDATE workshop_uploads SET state='expired',error='host-restored' WHERE state='open'")
                db.execute("UPDATE workshop_uploads SET lease='',lease_until=0,retry_at=0")
            db.execute("UPDATE releases SET state='scan-error',scan=? WHERE state IN ('approved','pending-review')",
                       (json.dumps({'status': 'error', 'time': 0, 'detail': 'Host restored; fresh scan/review required'}),))
            db.execute('INSERT INTO events(sha256,time,action,note) VALUES(?,?,?,?)',
                       ('', str(int(time.time())), 'host-restored', 'Old authentication revoked; publication held'))
    sync_directory(output)
    return {'repository': str(output), 'recovery_required': True, 'accounts_suspended': True}


def finish_recovery(repository, reconciliation):
    """Explicit LOCAL operator review; never infer the current external floor.

    The operator supplies the latest published version and cumulative revocations
    from their publication journal/current metadata, including changes since the
    backup. Unknown revoked hashes survive as tombstones, not fabricated releases.
    """
    from mod_trust import MAX_REVOKED, MAX_VERSION, origin
    root = private_directory(repository)
    marker = root / BARRIER
    regular(marker, 4096)
    path = Path(os.path.abspath(reconciliation))
    private_directory(path.parent)
    regular(path, 1024 * 1024)
    review = read_json(path.read_bytes())
    require(isinstance(review, dict) and set(review) == {'schema', 'origin', 'version_floor', 'revoked', 'note'}
            and type(review['schema']) is int and review['schema'] == 1, 'Invalid reconciliation record')
    service = origin(review['origin'])
    floor = review['version_floor']
    require(type(floor) is int and 1 <= floor < MAX_VERSION, 'Invalid publication floor')
    revoked = review['revoked']
    require(isinstance(revoked, list) and len(revoked) <= MAX_REVOKED
            and all(isinstance(sha, str) and SHA.fullmatch(sha) for sha in revoked)
            and len(set(revoked)) == len(revoked), 'Invalid cumulative revocation list')
    note = review['note']
    require(isinstance(note, str) and 0 < len(note.strip()) <= 1800, 'Record the reconciliation evidence')
    with closing(connect(root / 'catalog.sqlite', 'rw')) as db:
        db.execute('BEGIN IMMEDIATE')
        _, tables = inventory(db)
        old = db.execute('SELECT version,origin FROM publication_sequence WHERE singleton=1').fetchone() if 'publication_sequence' in tables else None
        require(old is None or (old['origin'] == service and floor >= old['version']), 'Publication origin/floor would roll back')
        db.execute('CREATE TABLE IF NOT EXISTS publication_sequence (singleton INTEGER PRIMARY KEY CHECK(singleton=1), version INTEGER NOT NULL, origin TEXT NOT NULL)')
        db.execute('CREATE TABLE IF NOT EXISTS recovery_revocations (sha256 TEXT PRIMARY KEY)')
        known = {row[0] for row in db.execute('SELECT sha256 FROM recovery_revocations')}
        known.update(row[0] for row in db.execute("SELECT sha256 FROM releases WHERE state='revoked'"))
        require(len(known | set(revoked)) <= MAX_REVOKED, 'Cumulative revocation budget exceeded')
        db.executemany('INSERT OR IGNORE INTO recovery_revocations VALUES(?)', ((sha,) for sha in revoked))
        db.execute("UPDATE releases SET state='revoked' WHERE sha256 IN (SELECT sha256 FROM recovery_revocations)")
        db.execute('INSERT INTO publication_sequence VALUES(1,?,?) ON CONFLICT(singleton) DO UPDATE SET version=excluded.version', (floor, service))
        db.execute('INSERT INTO events(sha256,time,action,note) VALUES(?,?,?,?)',
                   ('', str(int(time.time())), 'recovery-reconciled', note.strip()))
        db.commit()
    marker.unlink()
    sync_directory(root)
    return {'repository': str(root), 'recovery_required': False, 'version_floor': floor,
            'accounts_suspended': True, 'fresh_scan_review_required': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('backup', 'restore', 'resume'))
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    try:
        action = {'backup': snapshot, 'restore': restore, 'resume': finish_recovery}[args.action]
        print(json.dumps(action(args.source, args.destination)))
        return 0
    except (ModError, OSError, sqlite3.Error, ValueError) as error:
        print('Workshop recovery: ' + str(error), file=__import__('sys').stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
