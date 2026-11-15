"""Continuously refresh signed static downloads; never serves the private database."""
import argparse
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory, regular
from mod_live_publication import CaddyServer, LivePublisher, POLL, SnapshotStore
from mod_package import ModError, read_json, require
from mod_repository import Repository
from mod_signing import load_signer


def config(path):
    path = Path(os.path.abspath(path))
    private_directory(path.parent)
    regular(path, 65536)
    value = read_json(path.read_bytes())
    require(isinstance(value, dict) and set(value) == {'schema', 'repository', 'state', 'caddy',
                'trusted_root', 'catalog_keys', 'root_updates'} and type(value['schema']) is int
                and value['schema'] == 1, 'Invalid live publication configuration')
    for name, limit in (('catalog_keys', 4), ('root_updates', 64)):
        require(isinstance(value[name], list) and len(value[name]) <= limit, 'Invalid signing path count')
    require(value['catalog_keys'], 'Catalog signing key is required')
    for name in ('repository', 'state', 'caddy', 'trusted_root'):
        require(isinstance(value[name], str) and Path(value[name]).is_absolute(), 'Publication paths must be absolute')
    for name in ('catalog_keys', 'root_updates'):
        require(all(isinstance(p, str) and Path(p).is_absolute() for p in value[name]), 'Signing paths must be absolute')
    value['repository'] = private_directory(value['repository'])
    value['state'] = private_directory(value['state'])
    require(not value['state'].is_relative_to(value['repository'])
            and not value['repository'].is_relative_to(value['state']), 'Separate private repository and publication state')
    return value


def status(data, value):
    path = data['state'] / 'publication.json'
    if path.exists() or path.is_symlink():
        regular(path, 8192)
        old = read_json(path.read_bytes())
        require(isinstance(old, dict) and old.get('schema') == 1 and old.get('repository') == str(data['repository']),
                'Preserving unknown publication status')
    else:
        old = {}
    result = dict(schema=1, repository=str(data['repository']), time=int(time.time()), **value)
    if value['status'] == 'error':
        result['last_fault'] = dict(time=result['time'], reason='Live publication stopped; private operator inspection required')
    elif 'last_fault' in old:
        result['last_fault'] = old['last_fault']
    wire = json.dumps(result, separators=(',', ':')).encode()
    require(len(wire) <= 8192, 'Publication status exceeds budget')
    fd, temporary = tempfile.mkstemp(prefix='.publication-status-', dir=data['state'])
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(wire)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def serve(data):
    import fcntl
    import stat
    path = data['state'] / 'publisher.lock'
    fd = os.open(path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    server = None
    try:
        info = os.fstat(fd)
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid == os.geteuid()
                and not info.st_mode & 0o077, 'Unsafe publisher lock')
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        signer = load_signer(data['trusted_root'], data['catalog_keys'], data['root_updates'])
        server = CaddyServer(data['caddy'], data['state'])
        repository = Repository(data['repository'])
        store = SnapshotStore(data['state'] / 'publications', repository, signer.pinned_root)
        publisher = LivePublisher(repository, signer, store, server)
        written = 0
        while True:
            changed = publisher.tick()
            if changed or time.monotonic() - written >= 5:
                status(data, dict(status='serving' if publisher.current is not None else 'withheld',
                                  expires=publisher.expires, snapshot=publisher.current.name if publisher.current else None))
                written = time.monotonic()
            time.sleep(POLL)
    except (ModError, OSError, ValueError, sqlite3.Error, subprocess.SubprocessError):
        # A lock loser must not overwrite the real owner's status.
        if server is not None:
            server.stop()
            status(data, {'status': 'error'})
        raise
    finally:
        if server is not None:
            server.stop()
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    try:
        require(sys.platform == 'darwin', 'Run the publication controller on the Mac host')
        serve(config(args.config))
        return 0
    except KeyboardInterrupt:
        return 130
    except (ModError, OSError, ValueError, sqlite3.Error, subprocess.SubprocessError) as error:
        print('Live publication: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
