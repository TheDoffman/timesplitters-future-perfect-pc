"""Private signed-publication controller; public Caddy sees only one static tree."""
import hashlib
from itertools import islice
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import time
import uuid

from mod_backup import private_directory, regular
from mod_package import digest, read_json, require
from mod_publication import (DEFAULT_PUBLIC_LIMIT, MAX_CATALOG, MAX_RELEASES,
                             export_snapshot, revoked_hashes, validate_snapshot)
from mod_repository import SCAN_TTL
from mod_scan_sandbox import GuardedChild, verified_guardian
from mod_trust import trusted_root

MARGIN = 30
REFRESH = 1800
POLL = 0.5
ID = re.compile(r'[0-9a-f]{32}')


def new_json(path, value):
    wire = json.dumps(value, sort_keys=True, separators=(',', ':')).encode()
    require(len(wire) <= 256 * 1024, 'Publication receipt exceeds its budget')
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as stream:
        stream.write(wire)
        stream.flush()
        os.fsync(stream.fileno())


def inventory(root):
    """Deletion authority binds every exact owned output; never trust a prefix."""
    private_directory(root)
    files, directories, total = {}, [], 0
    entries = list(islice(root.rglob('*'), MAX_RELEASES + 141))
    require(len(entries) <= MAX_RELEASES + 140, 'Publication tree entry budget exceeded')
    for path in entries:
        require(not path.is_symlink(), 'Linked publication output refused')
        info = path.lstat()
        require(info.st_uid == os.geteuid() and not info.st_mode & 0o022, 'Unsafe publication output owner/mode')
        name = path.relative_to(root).as_posix()
        if stat.S_ISDIR(info.st_mode):
            directories.append(name)
        else:
            require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_size <= 256 * 1024**2,
                    'Unsafe publication output file')
            total += info.st_size
            require(total <= DEFAULT_PUBLIC_LIMIT + MAX_CATALOG + 2 * 1024**2, 'Publication output size exceeded')
            files[name] = dict(size=info.st_size, sha256=digest(path.read_bytes()))
    return dict(files=files, directories=sorted(directories))


class SnapshotStore:
    def __init__(self, path, repository, pinned):
        self.root = Path(path).absolute()
        private_directory(self.root.parent)
        marker = dict(schema=1, producer='tsfp-live-publication-v1', repository=str(repository.root),
                      anchor_sha256=digest(pinned))
        if not self.root.exists() and not self.root.is_symlink():
            self.root.mkdir(mode=0o700)
            new_json(self.root / 'owner.json', marker)
            for name in ('snapshots', 'receipts'):
                (self.root / name).mkdir(mode=0o700)
        private_directory(self.root)
        require(set(p.name for p in islice(self.root.iterdir(), 4)) == {'owner.json', 'snapshots', 'receipts'},
                'Preserving unrecognized publication store')
        regular(self.root / 'owner.json', 4096)
        require(read_json((self.root / 'owner.json').read_bytes()) == marker, 'Publication store identity changed')
        self.snapshots = private_directory(self.root / 'snapshots')
        self.receipts = private_directory(self.root / 'receipts')
        self.records()  # Refuse interrupted/unknown outputs before serving anything.

    def records(self):
        outputs = list(islice(self.snapshots.iterdir(), 4))
        receipts = list(islice(self.receipts.iterdir(), 4))
        require(len(outputs) <= 3 and len(receipts) <= 3, 'Publication retention budget exceeded')
        require(all(ID.fullmatch(p.name) for p in outputs)
                and all(p.suffix == '.json' and ID.fullmatch(p.stem) for p in receipts)
                and {p.name for p in outputs} == {p.stem for p in receipts},
                'Preserving interrupted or unrecognized publication outputs')
        result = {}
        for path in receipts:
            regular(path, 256 * 1024)
            value = read_json(path.read_bytes())
            require(isinstance(value, dict) and set(value) == {'schema', 'id', 'created', 'inventory'}
                    and value['schema'] == 1 and value['id'] == path.stem and type(value['created']) is int,
                    'Invalid publication receipt')
            result[path.stem] = value
        return result

    def create(self, repository, signer):
        records = self.records()
        if len(records) == 3:
            # A controller may have died after writing a complete receipt but
            # before pruning. Recover only these verified, owned outputs.
            newest = max(records.values(), key=lambda r: r['created'])['id']
            self.prune(self.snapshots / newest)
        import shutil
        require(shutil.disk_usage(self.root).free >= DEFAULT_PUBLIC_LIMIT + 512 * 1024**2,
                'Insufficient publication staging space')
        name = uuid.uuid4().hex
        path = self.snapshots / name
        result = export_snapshot(repository, path, signer=signer, min_validity=MARGIN)
        contents = inventory(path)
        new_json(self.receipts / (name + '.json'),
                 dict(schema=1, id=name, created=time.time_ns(), inventory=contents))
        # Read-only files guard accidental server writes. Private directories
        # stay0700; only this controller owns them and their retention receipts.
        for relative in contents['files']:
            (path / relative).chmod(0o444)
        return path, result

    def prune(self, current):
        records = self.records()
        keep = {current.name}
        previous = sorted((r for r in records.values() if r['id'] != current.name),
                          key=lambda r: r['created'], reverse=True)
        if previous:
            keep.add(previous[0]['id'])
        for name, record in records.items():
            if name in keep:
                continue
            root = self.snapshots / name
            actual = inventory(root)
            require(actual == record['inventory'], 'Preserving changed publication output')
            for relative in actual['files']:
                (root / relative).unlink()
            for relative in sorted(actual['directories'], key=lambda n: n.count('/'), reverse=True):
                (root / relative).rmdir()
            root.rmdir()
            (self.receipts / (name + '.json')).unlink()


def publication_view(repository):
    repository.require_recovered()
    with repository.db() as db:
        # Only the current approved/revoked view matters; login/upload audit
        # writes must not cause avoidable publication restarts.
        db.execute('BEGIN IMMEDIATE')
        releases = [r for r in repository.catalog() if r['scan']['time'] + SCAN_TTL - time.time() >= MARGIN]
        require(len(releases) <= MAX_RELEASES, 'Too many live public releases')
        value = dict(releases=releases, revoked=revoked_hashes(db))
        wire = json.dumps(value, sort_keys=True, separators=(',', ':')).encode()
        require(len(wire) <= MAX_CATALOG + 512 * 1024, 'Live publication view exceeds budget')
        return hashlib.sha256(wire).hexdigest()


class LivePublisher:
    def __init__(self, repository, signer, store, server):
        self.repository, self.signer, self.store, self.server = repository, signer, store, server
        self.current = None
        self.view = None
        self.published = 0
        self.expires = 0

    def tick(self):
        try:
            authority = trusted_root(self.signer.pinned_root, self.signer.updates)
            require(authority['expires'] - time.time() > MARGIN, 'Publication root needs rotation before expiry')
            view = publication_view(self.repository)
            if self.current is not None:
                require(self.server.running(), 'Publication server stopped unexpectedly')
            if view == self.view and time.monotonic() - self.published < REFRESH and self.expires - time.time() > MARGIN:
                return False
            # Withdraw first. Failed signatures/copy/validation never retain a
            # now-revoked or withheld old export as an operational fallback.
            self.server.stop()
            self.current = None
            path, result = self.store.create(self.repository, self.signer)
            validate_snapshot(path, self.signer.pinned_root)
            if publication_view(self.repository) != view:
                self.store.prune(path)
                self.view = None
                return False
            self.server.start(path, min(3600, int(result['expires'] - time.time())))
            self.current, self.view = path, view
            self.expires, self.published = result['expires'], time.monotonic()
            self.store.prune(path)
            return True
        except BaseException:
            self.server.stop()
            self.current = None
            raise


class CaddyServer:
    def __init__(self, executable, state):
        self.executable = Path(executable).resolve(strict=True)
        info = self.executable.stat()
        require(stat.S_ISREG(info.st_mode) and info.st_uid in (0, os.geteuid()) and not info.st_mode & 0o6022
                and os.access(self.executable, os.X_OK), 'Unsafe Caddy executable')
        self.state = private_directory(state)
        self.child = None

    def start(self, snapshot, seconds):
        require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL, 'Publisher must own child exit status')
        require(self.child is None and type(seconds) is int and 0 < seconds <= 3600, 'Invalid public server lease')
        require(not any(c in str(snapshot) for c in '"{}\r\n'), 'Invalid public snapshot path')
        guardian = verified_guardian()
        read, write = os.pipe()
        try:
            config = Path(__file__).with_name('hosting') / 'Caddyfile'
            process = subprocess.Popen([str(guardian), str(read), '--deadline-seconds', str(seconds),
                                        str(self.executable), 'run', '--config', str(config), '--adapter', 'caddyfile'],
                                       stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                       close_fds=True, pass_fds=(read,), start_new_session=True, cwd=self.state,
                                       env=dict(PATH='/usr/bin:/bin', HOME=str(self.state), TMPDIR=str(self.state),
                                                TSFP_MOD_PUBLIC_ROOT=str(snapshot), LC_ALL='C'))
            self.child = GuardedChild(process, write)
            write = None
        finally:
            os.close(read)
            if write is not None:
                os.close(write)

    def running(self):
        return self.child is not None and self.child.poll() is None

    def stop(self):
        if self.child is not None:
            self.child.kill()
            self.child.wait(timeout=10)
            self.child = None
