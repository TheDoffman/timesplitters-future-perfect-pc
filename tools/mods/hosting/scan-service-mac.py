"""Mac launchd scan worker and independent sampled-RSS watchdog.

Generate private plists first. This command neither enables a public endpoint nor
loads/unloads jobs automatically. Only trusted local configuration supplies paths.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import signal
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory, regular, connect, TABLES
from mod_package import ModError, read_json, require

MEMORY_BYTES = 2 * 1024**3
MAX_PROCESSES = 8
WALL_SECONDS = 480
HEARTBEAT_SECONDS = 10
POLL_SECONDS = 0.5
SUPERVISION_POLICY = 'guardian-descendants-v1'


def config(path):
    path = Path(os.path.abspath(path))
    private_directory(path.parent)
    regular(path, 8192)
    data = read_json(path.read_bytes())
    require(isinstance(data, dict) and set(data) == {'schema', 'repository', 'scanner', 'database', 'state'}
            and type(data['schema']) is int and data['schema'] == 1, 'Invalid scan service configuration')
    for name in ('repository', 'database', 'state'):
        require(isinstance(data[name], str) and Path(data[name]).is_absolute(), 'Host paths must be absolute')
        data[name] = private_directory(data[name])
    require(isinstance(data['scanner'], str) and Path(data['scanner']).is_absolute(), 'Scanner path must be absolute')
    # An explicit local host configuration may select the installed Homebrew alias;
    # uploaded fields never take this trusted alias-resolution path.
    scanner = Path(data['scanner']).resolve(strict=True)
    info = scanner.stat()
    require(scanner.is_file() and os.access(scanner, os.X_OK) and info.st_mode & 0o022 == 0
            and info.st_uid in (0, os.geteuid()), 'Unsafe scanner executable')
    data['scanner'] = scanner
    data['config'] = path
    data['uid'] = os.geteuid()
    suffix = hashlib.sha256(str(path).encode()).hexdigest()[:24]
    data['label'] = 'org.timesplitters.workshop.scan.' + suffix
    return data


def status_file(data, component):
    return data['state'] / (component + '.json')


def status(data, component, value):
    path = status_file(data, component)
    private_directory(path.parent)
    if path.exists() or path.is_symlink():
        regular(path, 8192)
        old = read_json(path.read_bytes())
        require(isinstance(old, dict) and old.get('schema') == 1 and old.get('component') == component
                and old.get('service') == data['label'],
                'Preserving an unrecognized service status file')
    else:
        old = {}
    value = dict(value)
    if value.get('status') in ('error', 'terminated'):
        value['last_fault'] = {'time': int(time.time()), 'reason': value.get('reason', 'service error')}
    elif 'last_fault' in old:
        value['last_fault'] = old['last_fault']
    encoded = json.dumps(dict(schema=1, service=data['label'], component=component,
                              time=int(time.time()), **value), separators=(',', ':')).encode()
    require(len(encoded) <= 8192, 'Service status exceeds its budget')
    descriptor, temporary = tempfile.mkstemp(prefix='.scan-status-', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'wb') as output:
            output.write(encoded)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def healthy(data):
    path = status_file(data, 'watchdog')
    regular(path, 8192)
    value = read_json(path.read_bytes())
    require(isinstance(value, dict), 'Invalid watchdog status')
    stamp = value.get('time')
    require(value.get('schema') == 1 and value.get('component') == 'watchdog' and value.get('service') == data['label']
            and value.get('supervision_policy') == SUPERVISION_POLICY
            and type(stamp) is int and 0 <= time.time() - stamp <= HEARTBEAT_SECONDS
            and value.get('status') in ('watching', 'idle'), 'Scan watchdog is not healthy')


def repository_preflight(root):
    database = root / 'catalog.sqlite'
    if not database.exists() and not database.is_symlink():
        require(not any(root.iterdir()), 'Refusing to initialize a nonempty unrecognized repository')
        return
    from contextlib import closing
    with closing(connect(database)) as db:
        rows = db.execute("SELECT name,type FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' LIMIT 65").fetchall()
        names = {row['name'] for row in rows if row['type'] == 'table'}
        require(len(rows) <= 64 and {'releases', 'events'} <= names <= TABLES
                and all(row['type'] in ('table', 'index') for row in rows), 'Unrecognized repository schema')


def admit(data):
    from mod_scan_sandbox import ScanSandbox
    if not getattr(ScanSandbox, 'GUARDED_PARENT_LIFETIME', False):
        status(data, 'worker', {'status': 'held', 'reason': 'Scanner lifetime containment upgrade required'})
        return False
    try:
        healthy(data)
        return True
    except (OSError, ModError, ValueError):
        status(data, 'worker', {'status': 'waiting', 'reason': 'Waiting for a healthy scan watchdog'})
        return False


def run_command(arguments):
    result = subprocess.run(arguments, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=3, check=False)
    # These fixed OS utilities read only this owned job, never arbitrary uploaded
    # command output or a full user-domain environment dump.
    require(len(result.stdout) <= 1024 * 1024, 'Host utility output exceeded its budget')
    return result.returncode, result.stdout.decode('utf-8', errors='strict')


def target(data):
    return 'gui/' + str(os.geteuid()) + '/' + data['label']


def managed_pid(output):
    values = re.findall(r'^\s*pid = ([0-9]+)\s*$', output, re.MULTILINE)
    require(len(values) <= 1, 'Ambiguous managed worker identity')
    return int(values[0]) if values else None


def sample(pid, command=run_command, uid=None):
    # Numeric process metadata only: no argv/environment or personal file names.
    # Session/group changes do not alter this parent-child relationship.
    code, output = command(['/bin/ps', '-axo', 'pid=,ppid=,rss=,uid='])
    require(code == 0, 'Cannot sample managed scan descendants')
    rows = output.splitlines()
    require(len(rows) <= 8192, 'Host process metadata budget exceeded')
    processes, children = {}, {}
    for row in rows:
        # Darwin prints the unrelated system "nobody" UID as -2. Only the
        # managed subtree must match the configured nonnegative service UID.
        require(re.fullmatch(r'\s*[0-9]+\s+[0-9]+\s+[0-9]+\s+-?[0-9]+\s*', row), 'Malformed process sample')
        identity, parent, rss, owner = map(int, row.split())
        require(identity not in processes, 'Duplicate process sample')
        processes[identity] = (rss, owner)
        children.setdefault(parent, []).append(identity)
    if pid not in processes:
        return None
    expected_uid = processes[pid][1] if uid is None else uid
    pending, seen, total = [pid], set(), 0
    while pending:
        current = pending.pop()
        require(current not in seen and len(seen) < MAX_PROCESSES, 'Managed scan process budget exceeded')
        seen.add(current)
        rss, owner = processes[current]
        require(owner == expected_uid, 'Managed scan owner changed')
        total += rss * 1024
        pending.extend(children.get(current, ()))
    return total


def watch_once(data, observed, command=run_command, clock=time.monotonic):
    code, output = command(['/bin/launchctl', 'print', target(data)])
    if code != 0:
        command(['/bin/launchctl', 'kill', 'SIGKILL', target(data)])
        return {}, {'status': 'error', 'reason': 'Worker service is not loaded'}
    pid = managed_pid(output)
    if pid is None:
        return {}, {'status': 'idle', 'supervision_policy': SUPERVISION_POLICY}
    now = clock()
    start = observed.get('start', now) if observed.get('pid') == pid else now
    memory = sample(pid, command, data.get('uid'))
    if memory is None:
        return {}, {'status': 'idle', 'supervision_policy': SUPERVISION_POLICY}
    reason = 'memory' if memory > MEMORY_BYTES else 'wall-time' if now - start > WALL_SECONDS else None
    if reason:
        # Signal the launchd-owned SERVICE, not the sampled PID. If the worker
        # exited/restarted meanwhile, no unrelated process can be targeted.
        command(['/bin/launchctl', 'kill', 'SIGKILL', target(data)])
        return {}, {'status': 'terminated', 'reason': reason, 'rss_bytes': memory}
    return {'pid': pid, 'start': start}, {'status': 'watching', 'pid': pid, 'rss_bytes': memory,
                                       'supervision_policy': SUPERVISION_POLICY}


def watchdog(data):
    observed = {}
    previous, written = None, 0
    while True:
        try:
            observed, report = watch_once(data, observed)
        except (OSError, ValueError, ModError, subprocess.SubprocessError):
            # Losing supervision withholds the worker rather than silently
            # permitting an unmonitored job. launchd owns process-group cleanup.
            observed, report = {}, {'status': 'error', 'reason': 'Worker supervision failed'}
            status(data, 'watchdog', report)
            run_command(['/bin/launchctl', 'kill', 'SIGKILL', target(data)])
        key = (report['status'], report.get('pid'), report.get('reason'))
        if key != previous or time.monotonic() - written >= 5:
            status(data, 'watchdog', report)
            previous, written = key, time.monotonic()
        time.sleep(POLL_SECONDS)


def worker(data):
    import resource
    from mod_accounts import Accounts
    from mod_repository import ClamScanner, Repository
    from mod_rescan import rescan_next
    from mod_uploads import Uploads
    require(os.getpgrp() == os.getpid(), 'Scan worker must own its managed process group')
    signal.alarm(WALL_SECONDS)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_NOFILE, (256, 256))
    try:
        if not admit(data):
            return
        status(data, 'worker', {'status': 'running', 'pid': os.getpid()})
        repository_preflight(data['repository'])
        repository = Repository(data['repository'])
        repository.require_recovered()
        accounts = Accounts(repository)
        scanner = ClamScanner(str(data['scanner']), data['database'])
        queued = Uploads(accounts).process_next(scanner)
        healthy(data)
        renewed = rescan_next(accounts, scanner)
        status(data, 'worker', {'status': 'complete', 'queue_work': queued is not None,
                                'rescan_work': renewed is not None})
    except Exception:
        status(data, 'worker', {'status': 'error', 'reason': 'Scan cycle failed; inspect private repository'})
        raise
    finally:
        signal.alarm(0)


def generate(data, directory):
    output = Path(os.path.abspath(directory))
    private_directory(output.parent)
    require(not output.exists() and not output.is_symlink(), 'Choose a new service bundle directory')
    output.mkdir(mode=0o700)
    # Preserve the selected virtual environment instead of resolving its Python
    # symlink to the base interpreter and losing the pinned environment.
    arguments = [str(Path(sys.executable).absolute()), str(Path(__file__).resolve()), '--config', str(data['config'])]
    for component in ('worker', 'watchdog'):
        label = data['label'] + ('.watchdog' if component == 'watchdog' else '')
        value = dict(Label=label, ProgramArguments=arguments + ['--' + component],
                     RunAtLoad=True, ProcessType='Background', Umask=0o077,
                     AbandonProcessGroup=False, StandardOutPath='/dev/null', StandardErrorPath='/dev/null')
        if component == 'worker':
            value.update(StartInterval=60, HardResourceLimits=dict(Core=0, CPU=420, FileSize=512 * 1024**2))
        else:
            value.update(KeepAlive=True, ThrottleInterval=5)
        path = output / (label + '.plist')
        with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600), 'wb') as stream:
            plistlib.dump(value, stream)
            stream.flush()
            os.fsync(stream.fileno())
    return {'bundle': str(output), 'worker_label': data['label'], 'watchdog_label': data['label'] + '.watchdog'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--worker', action='store_true')
    modes.add_argument('--watchdog', action='store_true')
    modes.add_argument('--generate', type=Path)
    args = parser.parse_args()
    try:
        require(sys.platform == 'darwin', 'Run the launchd scan service on the Mac host')
        data = config(args.config)
        if args.generate:
            print(json.dumps(generate(data, args.generate)))
        elif args.worker:
            worker(data)
        else:
            watchdog(data)
        return 0
    except (ModError, OSError, ValueError, subprocess.SubprocessError) as error:
        print('Scan service: ' + str(error), file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 130


if __name__ == '__main__':
    raise SystemExit(main())
