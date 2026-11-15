"""Origin-bound workshop gateway: fixed local backends, no repository/signing access."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory, regular
from mod_live_publication import CaddyServer
from mod_package import ModError, read_json, require
from mod_service_watch import Supervisor
from mod_trust import origin as checked_origin

POLICY = 'gateway-descendants-v1'


def config(path):
    path = Path(os.path.abspath(path))
    private_directory(path.parent)
    regular(path, 8192)
    data = read_json(path.read_bytes())
    require(isinstance(data, dict) and set(data) == {'schema', 'origin', 'state', 'caddy'}
            and type(data['schema']) is int and data['schema'] == 1, 'Invalid gateway configuration')
    checked_origin(data['origin'])
    for name in ('state', 'caddy'):
        require(isinstance(data[name], str) and Path(data[name]).is_absolute(), 'Gateway paths must be absolute')
    data['state'] = private_directory(data['state'])
    data['config'], data['uid'] = path, os.geteuid()
    data['label'] = 'org.timesplitters.workshop.gateway.' + hashlib.sha256(str(path).encode()).hexdigest()[:24]
    return data


def supervisor(data):
    return Supervisor(data, 'gateway', 'origin', POLICY)


def serve(data, managed=False):
    import fcntl
    fd = os.open(data['state'] / 'gateway.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    owner, server = False, None
    watch = supervisor(data)
    try:
        info = os.fstat(fd)
        require(stat.S_ISREG(info.st_mode) and info.st_uid == os.geteuid() and info.st_nlink == 1
                and not info.st_mode & 0o077, 'Unsafe gateway lock')
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        owner = True
        if managed:
            import resource
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            resource.setrlimit(resource.RLIMIT_NOFILE, (256, 256))
            resource.setrlimit(resource.RLIMIT_FSIZE, (512 * 1024**2, 512 * 1024**2))
            deadline = time.monotonic() + 60
            while True:
                watch.status({'status': 'waiting'})
                try:
                    watch.healthy()
                    break
                except (ModError, OSError, ValueError):
                    if time.monotonic() >= deadline:
                        return
                    time.sleep(1)
        server = CaddyServer(data['caddy'], data['state'], role='gateway', origin=data['origin'])
        server.start(None, 3600)
        started, written = time.monotonic(), 0
        while True:
            if managed:
                watch.healthy()
            require(server.running(), 'Gateway server stopped unexpectedly')
            if time.monotonic() - started >= 1800:
                server.stop()
                server.start(None, 3600)
                started = time.monotonic()
            if time.monotonic() - written >= 5:
                watch.status({'status': 'serving'})
                written = time.monotonic()
            time.sleep(0.5)
    except (ModError, OSError, ValueError, subprocess.SubprocessError):
        if server is not None:
            server.stop()
        if owner:
            watch.status({'status': 'error'})
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
    modes.add_argument('--managed', action='store_true')
    modes.add_argument('--watchdog', action='store_true')
    args = parser.parse_args()
    try:
        require(sys.platform == 'darwin', 'Run the gateway on the Mac workshop host')
        data = config(args.config)
        if args.generate:
            print(json.dumps(supervisor(data).generate(__file__, args.generate)))
        elif args.watchdog:
            supervisor(data).watchdog()
        else:
            serve(data, args.managed)
        return 0
    except KeyboardInterrupt:
        return 130
    except (ModError, OSError, ValueError, subprocess.SubprocessError) as error:
        print('Workshop gateway: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
