"""Continuously refresh signed static downloads; never serves the private database."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
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
from mod_host_watch import managed_pid, run_command, sample

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


def status(data, value, component='publication'):
    require(component in ('publication', 'watchdog'), 'Invalid publication status component')
    path = data['state'] / (component + '.json')
    if path.exists() or path.is_symlink():
        regular(path, 8192)
        old = read_json(path.read_bytes())
        require(isinstance(old, dict) and old.get('schema') == 1 and old.get('repository') == str(data['repository'])
                and old.get('service') in (None, data['label']),
                'Preserving unknown publication status')
    else:
        old = {}
    result = dict(schema=1, repository=str(data['repository']), service=data['label'],
                  component=component, pid=os.getpid(), time=int(time.time()), **value)
    if value['status'] in ('error', 'terminated'):
        result['last_fault'] = dict(time=result['time'], reason=value.get('reason',
                                   'Live publication stopped; private operator inspection required'))
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


def healthy(data):
    path = data['state'] / 'watchdog.json'
    regular(path, 8192)
    value = read_json(path.read_bytes())
    stamp = value.get('time') if isinstance(value, dict) else None
    require(isinstance(value, dict) and value.get('schema') == 1 and value.get('service') == data['label']
            and value.get('component') == 'watchdog' and value.get('supervision_policy') == SUPERVISION_POLICY
            and value.get('status') == 'watching' and value.get('watched_pid') == os.getpid()
            and type(stamp) is int and 0 <= time.time() - stamp <= HEALTH_SECONDS,
            'Publication watchdog is not healthy')


def target(data):
    return 'gui/' + str(data['uid']) + '/' + data['label']


def watch_once(data, observed, command=run_command, clock=time.monotonic):
    code, output = command(['/bin/launchctl', 'print', target(data)])
    if code != 0:
        command(['/bin/launchctl', 'kill', 'SIGKILL', target(data)])
        return {}, dict(status='error', reason='Controller service is not loaded')
    pid = managed_pid(output)
    if pid is None:
        return {}, dict(status='idle', supervision_policy=SUPERVISION_POLICY)
    now = clock()
    start = observed.get('start', now) if observed.get('pid') == pid else now
    memory = sample(pid, command, data['uid'])
    if memory is None:
        return {}, dict(status='idle', supervision_policy=SUPERVISION_POLICY)
    reason = 'memory' if memory > MEMORY_BYTES else None
    if not reason and now - start > CONTROLLER_SECONDS:
        try:
            path = data['state'] / 'publication.json'
            regular(path, 8192)
            value = read_json(path.read_bytes())
            stamp = value.get('time') if isinstance(value, dict) else None
            require(isinstance(value, dict) and value.get('schema') == 1 and value.get('service') == data['label']
                    and value.get('pid') == pid and type(stamp) is int
                    and 0 <= time.time() - stamp <= CONTROLLER_SECONDS, 'Controller heartbeat expired')
        except (ModError, OSError, ValueError):
            reason = 'heartbeat'
    if reason:
        command(['/bin/launchctl', 'kill', 'SIGKILL', target(data)])
        return {}, dict(status='terminated', reason=reason, watched_pid=pid, rss_bytes=memory)
    return dict(pid=pid, start=start), dict(status='watching', watched_pid=pid, rss_bytes=memory,
                                          supervision_policy=SUPERVISION_POLICY)


def watchdog(data):
    observed, written, previous = {}, 0, None
    while True:
        try:
            observed, report = watch_once(data, observed)
        except (ModError, OSError, ValueError, subprocess.SubprocessError):
            observed, report = {}, dict(status='error', reason='Publication supervision failed')
            # Withhold/cancel before attempting to write a possibly damaged
            # status file; metadata errors must never leave downloads running.
            run_command(['/bin/launchctl', 'kill', 'SIGKILL', target(data)])
        key = (report['status'], report.get('watched_pid'), report.get('reason'))
        if key != previous or time.monotonic() - written >= 5:
            try:
                status(data, report, 'watchdog')
            except (ModError, OSError, ValueError):
                run_command(['/bin/launchctl', 'kill', 'SIGKILL', target(data)])
                raise
            previous, written = key, time.monotonic()
        time.sleep(POLL)


def generate(data, output):
    output = Path(os.path.abspath(output))
    private_directory(output.parent)
    require(not output.exists() and not output.is_symlink(), 'Choose a new publication agent bundle')
    output.mkdir(mode=0o700)
    arguments = [str(Path(sys.executable).absolute()), str(Path(__file__).resolve()), '--config', str(data['config'])]
    for component in ('publication', 'watchdog'):
        label = data['label'] + ('.watchdog' if component == 'watchdog' else '')
        value = dict(Label=label, ProgramArguments=arguments + ['--managed' if component == 'publication' else '--watchdog'],
                     RunAtLoad=True, KeepAlive=True, ThrottleInterval=60 if component == 'publication' else 5,
                     ProcessType='Background', Umask=0o077, AbandonProcessGroup=False,
                     HardResourceLimits=dict(Core=0, FileSize=512 * 1024**2),
                     StandardOutPath='/dev/null', StandardErrorPath='/dev/null')
        path = output / (label + '.plist')
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as stream:
            plistlib.dump(value, stream)
            stream.flush()
            os.fsync(stream.fileno())
    return dict(bundle=str(output), publisher_label=data['label'], watchdog_label=data['label'] + '.watchdog')


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
