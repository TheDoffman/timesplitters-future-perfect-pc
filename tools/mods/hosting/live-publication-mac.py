"""Continuously refresh signed static downloads; never serves the private database."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory, regular
from mod_live_publication import CaddyServer, LivePublisher, POLL, SnapshotStore
from mod_package import ModError, read_json, require
from mod_repository import Repository
from mod_signing import load_signer
from mod_host_watch import managed_pid, run_command
from mod_service_watch import Supervisor

HEALTH_SECONDS = 10
CONTROLLER_SECONDS = 15
MEMORY_BYTES = 2 * 1024**3
SUPERVISION_POLICY = 'publication-descendants-v1'


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
    value['config'] = path
    value['uid'] = os.geteuid()
    value['label'] = 'org.timesplitters.workshop.publication.' + hashlib.sha256(str(path).encode()).hexdigest()[:24]
    return value


def supervisor(data):
    return Supervisor(data, 'publication', 'repository', SUPERVISION_POLICY,
                      MEMORY_BYTES, CONTROLLER_SECONDS, HEALTH_SECONDS)


def status(data, value, component='publication'):
    return supervisor(data).status(value, component)


def healthy(data):
    return supervisor(data).healthy()


def target(data):
    return supervisor(data).target()


def watch_once(data, observed, command=run_command, clock=time.monotonic):
    return supervisor(data).watch_once(observed, command, clock)


def watchdog(data):
    return supervisor(data).watchdog()


def generate(data, output):
    result = supervisor(data).generate(__file__, output)
    result['publisher_label'] = result.pop('controller_label')
    return result


def serve(data, managed=False):
    import fcntl
    import stat
    path = data['state'] / 'publisher.lock'
    fd = os.open(path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    server = None
    owner = False
    try:
        info = os.fstat(fd)
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid == os.geteuid()
                and not info.st_mode & 0o077, 'Unsafe publisher lock')
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        owner = True
        if managed:
            import resource
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            resource.setrlimit(resource.RLIMIT_NOFILE, (256, 256))
            resource.setrlimit(resource.RLIMIT_FSIZE, (512 * 1024**2, 512 * 1024**2))
            deadline = time.monotonic() + 60
            while True:
                status(data, {'status': 'waiting'})
                try:
                    healthy(data)
                    break
                except (ModError, OSError, ValueError):
                    if time.monotonic() >= deadline:
                        return
                    time.sleep(1)
        signer = load_signer(data['trusted_root'], data['catalog_keys'], data['root_updates'])
        server = CaddyServer(data['caddy'], data['state'])
        repository = Repository(data['repository'])
        store = SnapshotStore(data['state'] / 'publications', repository, signer.pinned_root)
        publisher = LivePublisher(repository, signer, store, server)
        written = 0
        while True:
            if managed:
                healthy(data)
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
        if owner:
            status(data, {'status': 'error'})
        raise
    finally:
        if server is not None:
            server.stop()
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--generate', type=Path)
    modes.add_argument('--watchdog', action='store_true')
    modes.add_argument('--managed', action='store_true')
    args = parser.parse_args()
    try:
        require(sys.platform == 'darwin', 'Run the publication controller on the Mac host')
        data = config(args.config)
        if args.generate:
            print(json.dumps(generate(data, args.generate)))
        elif args.watchdog:
            watchdog(data)
        else:
            serve(data, args.managed)
        return 0
    except KeyboardInterrupt:
        return 130
    except (ModError, OSError, ValueError, sqlite3.Error, subprocess.SubprocessError) as error:
        print('Live publication: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
