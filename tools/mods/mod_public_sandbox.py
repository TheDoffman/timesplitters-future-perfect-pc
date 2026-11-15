"""Mandatory Mac static-server confinement and owned, leased temporary storage."""
from itertools import islice
import json
import os
from pathlib import Path
import re
import stat
import uuid

from mod_backup import private_directory
from mod_package import read_json, require

PRODUCER = 'tsfp-caddy-scratch-v1'
ID = re.compile(r'[0-9a-f]{32}')
MAX_FILES = 256
MAX_BYTES = 32 * 1024**2


def profile(executable, snapshot, configuration, work, role='downloads'):
    require(role in ('downloads', 'gateway') and (snapshot is not None) == (role == 'downloads'),
            'Invalid public server role/tree binding')
    quote = lambda p: json.dumps(str(p), ensure_ascii=True)
    subpaths = lambda paths: ''.join('(subpath ' + quote(p) + ')' for p in paths)
    literals = lambda paths: ''.join('(literal ' + quote(p) + ')' for p in paths)
    runtime = ['/System', '/usr/lib', '/usr/share', '/private/preboot/Cryptexes']
    port = 8766 if role == 'downloads' else 8768
    reads = [*runtime, work] + ([snapshot] if snapshot is not None else [])
    proxy = ('(allow network-outbound (remote ip "localhost:8766") (remote ip "localhost:8767"))'
             if role == 'gateway' else '')
    return ('(version 1)(deny default)(deny process-info*)'
            '(allow process-exec (literal ' + quote(executable) + '))'
            '(allow signal (target self))'
            '(allow sysctl-read (sysctl-name-regex #"^hw\\.")'
            '(sysctl-name "kern.osversion")(sysctl-name "kern.osrelease")'
            '(sysctl-name "kern.ostype")(sysctl-name "kern.version")(sysctl-name "kern.argmax"))'
            '(allow file-read-metadata)(allow file-read* '
            + subpaths(reads) + literals([
                '/', executable, configuration, '/dev/null', '/dev/random', '/dev/urandom', '/private/etc/localtime']) + ')'
            '(allow file-map-executable ' + subpaths(runtime) + literals([executable]) + ')'
            '(allow file-write* (subpath ' + quote(work) + '))'
            '(allow network-bind network-inbound (local ip "localhost:' + str(port) + '"))'
            # Darwin sendfile checks a connected server socket without a
            # supplied remote endpoint. Match only that response path: a new
            # connect/sendto supplies a destination and must remain denied,
            # even if a hostile caller first binds the permitted local port.
            '(allow network-outbound (require-all (local ip "localhost:' + str(port)
            + '") (require-not (remote ip))))' + proxy)


def marker(path, expected, create=False):
    if create:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as stream:
            stream.write(json.dumps(expected, sort_keys=True).encode())
            stream.flush()
            os.fsync(stream.fileno())
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid == os.geteuid() and info.st_nlink == 1
            and not info.st_mode & 0o077 and 0 < info.st_size <= 4096, 'Unsafe Caddy scratch marker')
    require(read_json(path.read_bytes()) == expected, 'Preserving unknown Caddy scratch owner')


def contents(work, cleanup=False):
    private_directory(work)
    pending, entries, total = [(work, 0)], [], 0
    limit = 4096 if cleanup else MAX_FILES
    maximum = 1024**3 if cleanup else MAX_BYTES
    while pending:
        directory, depth = pending.pop()
        require(depth <= (16 if cleanup else 8), 'Caddy scratch depth budget exceeded')
        with os.scandir(directory) as scan:
            for entry in scan:
                require(len(entries) < limit, 'Caddy scratch entry budget exceeded')
                path = Path(entry.path)
                info = entry.stat(follow_symlinks=False)
                require(info.st_uid == os.geteuid(), 'Preserving foreign-owned Caddy scratch data')
                is_directory = stat.S_ISDIR(info.st_mode)
                entries.append((path, is_directory))
                if is_directory:
                    pending.append((path, depth + 1))
                elif stat.S_ISREG(info.st_mode):
                    total += info.st_size
                    require(total <= maximum, 'Caddy scratch storage budget exceeded')
                    require(cleanup or info.st_nlink == 1, 'Linked running Caddy scratch file')
                else:
                    # Cleanup unlinks the link/socket/FIFO itself, never its
                    # target. No data is read from these untrusted outputs.
                    require(cleanup, 'Special running Caddy scratch entry')
    return entries


class Scratch:
    def __init__(self, unit, state, create=False):
        import fcntl
        self.unit, self.state, self.lease = unit, state, None
        private_directory(unit)
        require(ID.fullmatch(unit.name), 'Invalid Caddy scratch identity')
        expected = dict(schema=1, producer=PRODUCER, id=unit.name, state=str(state))
        marker(unit / 'owner.json', expected, create)
        flags = os.O_RDWR | os.O_NOFOLLOW | (os.O_CREAT | os.O_EXCL if create else 0)
        self.lease = os.open(unit / 'lease', flags, 0o600)
        try:
            info = os.fstat(self.lease)
            require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid == os.geteuid()
                    and not info.st_mode & 0o077, 'Unsafe Caddy scratch lease')
            fcntl.flock(self.lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.path = unit / 'work'
            if create:
                self.path.mkdir(mode=0o700)
            private_directory(self.path)
            require(set(p.name for p in islice(unit.iterdir(), 4)) == {'owner.json', 'lease', 'work'},
                    'Preserving unknown Caddy scratch unit')
        except BaseException:
            os.close(self.lease)
            self.lease = None
            raise

    def check(self):
        contents(self.path)

    def close(self):
        require(self.lease is not None, 'Caddy scratch lease already released')
        try:
            entries = contents(self.path, cleanup=True)
            # Every item is inside this newly allocated, owner-marked work
            # directory. The server cannot write the containing marker/lease.
            for path, directory in sorted(entries, key=lambda item: len(item[0].parts), reverse=True):
                if directory:
                    path.rmdir()
                else:
                    path.unlink()
            self.path.rmdir()
            (self.unit / 'lease').unlink()
            (self.unit / 'owner.json').unlink()
            self.unit.rmdir()
        finally:
            os.close(self.lease)
            self.lease = None


def acquire(state):
    state = private_directory(state)
    root = state / 'caddy-runtime'
    expected = dict(schema=1, producer=PRODUCER, state=str(state))
    if not root.exists() and not root.is_symlink():
        root.mkdir(mode=0o700)
        marker(root / 'owner.json', expected, True)
        (root / 'units').mkdir(mode=0o700)
    private_directory(root)
    marker(root / 'owner.json', expected)
    require(set(p.name for p in islice(root.iterdir(), 3)) == {'owner.json', 'units'},
            'Preserving unknown Caddy runtime directory')
    units = private_directory(root / 'units')
    previous = list(islice(units.iterdir(), 5))
    require(len(previous) <= 4, 'Caddy scratch recovery budget exceeded')
    for unit in previous:
        # The guardian inherits the lease FD, while its child inherits only
        # stdio. A crashed controller cannot make live server output reclaimable.
        scratch = Scratch(unit, state)
        scratch.close()
    unit = units / uuid.uuid4().hex
    unit.mkdir(mode=0o700)
    return Scratch(unit, state, True)
