"""Private scheduled official FreshClam checks; never accepts uploaded options."""
import argparse
import hashlib
from itertools import islice
import json
import os
from pathlib import Path
import plistlib
import signal
import stat
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory, regular
from mod_package import ModError, read_json, require
from mod_scan_sandbox import GuardedChild, verified_guardian
from mod_runtime_files import runtime_access

TIMEOUT = 180
OUTPUT_LIMIT = 1024 * 1024
INTERVAL = 6 * 60 * 60


def config(path):
    path = Path(os.path.abspath(path))
    private_directory(path.parent)
    regular(path, 8192)
    value = read_json(path.read_bytes())
    require(isinstance(value, dict) and set(value) == {'schema', 'database', 'state', 'freshclam'}
            and type(value['schema']) is int and value['schema'] == 1, 'Invalid definition service configuration')
    for name in ('database', 'state', 'freshclam'):
        require(isinstance(value[name], str) and Path(value[name]).is_absolute()
                and not any(c in value[name] for c in '\r\n\0"\\'), 'Unsafe definition service path')
    value['database'] = private_directory(value['database'])
    value['state'] = private_directory(value['state'])
    executable = Path(value['freshclam']).resolve(strict=True)
    info = executable.stat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid in (0, os.geteuid())
            and not info.st_mode & 0o6022 and os.access(executable, os.X_OK), 'Unsafe FreshClam executable')
    value['freshclam'] = executable
    value['config'] = path
    value['label'] = 'org.timesplitters.workshop.definitions.' + hashlib.sha256(str(path).encode()).hexdigest()[:24]
    return value


def freshclam_config(data):
    # FreshClam's own signature and database tests stay enabled. No proxy,
    # external database, shell hook, daemon, log file or notify command is added.
    return ('DatabaseDirectory "' + str(data['database']) + '"\n'
            'DatabaseMirror database.clamav.net\nForeground yes\nLogTime yes\n'
            'MaxAttempts 2\nConnectTimeout 15\nReceiveTimeout 60\nTestDatabases yes\n').encode()


def exclusive(path, content):
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())


def prepared_config(data):
    path = data['state'] / 'freshclam.conf'
    expected = freshclam_config(data)
    if path.exists() or path.is_symlink():
        regular(path, 8192)
        require(path.read_bytes() == expected, 'Preserving changed definition updater configuration')
    else:
        exclusive(path, expected)
    return path


def status(data, result):
    path = data['state'] / 'definitions.json'
    if path.exists() or path.is_symlink():
        regular(path, 8192)
        old = read_json(path.read_bytes())
        require(isinstance(old, dict) and old.get('schema') == 1 and old.get('service') == data['label'],
                'Preserving unrecognized definition status')
    else:
        old = {}
    result = dict(schema=1, service=data['label'], time=int(time.time()), **result)
    if result['status'] == 'error':
        result['last_fault'] = dict(time=result['time'], reason=result['reason'])
    elif 'last_fault' in old:
        result['last_fault'] = old['last_fault']
    encoded = json.dumps(result, separators=(',', ':')).encode()
    require(len(encoded) <= 8192, 'Definition status exceeds its budget')
    fd, temporary = tempfile.mkstemp(prefix='.definition-status-', dir=data['state'])
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def capacity(data):
    import shutil
    entries = list(islice(data['database'].iterdir(), 129))
    require(len(entries) <= 128, 'Definition directory entry budget exceeded')
    total = 0
    for path in entries:
        total += regular(path, 512 * 1024**2).st_size
    require(total <= 1024**3 and shutil.disk_usage(data['database']).free >= 512 * 1024**2,
            'Insufficient bounded definition storage')


def command(data, configuration, timeout=TIMEOUT):
    """Own the actual trusted maintenance PID through the native lifetime helper.

    FreshClam needs network access and writes only its official definition store;
    this command is not the upload scanner and never receives mod content.
    """
    require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL, 'Updater must own child exit status')
    guardian = verified_guardian()
    access = runtime_access(data['freshclam'])
    environment = dict(PATH='/usr/bin:/bin', HOME=str(data['state']), TMPDIR=str(data['state']), LC_ALL='C', TZ='UTC')
    if access is not None:
        environment['CVD_CERTS_DIR'] = str(access['certificate'].parent)
    read, write = os.pipe()
    process = None
    child = None
    exceeded = []
    failures = []
    count = [0]
    digest = hashlib.sha256()
    def drain():
        try:
            while chunk := process.stdout.read(4096):
                count[0] += len(chunk)
                digest.update(chunk)
                if count[0] > OUTPUT_LIMIT:
                    exceeded.append(True)
                    child.kill()
                    break
        except OSError:
            failures.append(True)
            child.kill()
    try:
        process = subprocess.Popen([str(guardian), str(read), str(data['freshclam']),
                                    '--config-file=' + str(configuration), '--stdout'],
                                   stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   cwd=data['state'], env=environment,
                                   close_fds=True, pass_fds=(read,), start_new_session=True)
        os.close(read)
        read = None
        child = GuardedChild(process, write)
        write = None
        reader = threading.Thread(target=drain, daemon=True)
        reader.start()
        try:
            process.wait(timeout=timeout)
        finally:
            child.kill()
            process.wait(timeout=10)
            reader.join(timeout=10)
            process.stdout.close()
        require(not reader.is_alive() and not failures, 'Definition updater output stream failed')
        require(not exceeded, 'Definition updater output exceeded1MiB')
        require(process.returncode == 0, 'FreshClam failed with exit code ' + str(process.returncode))
        return dict(output_bytes=count[0], output_sha256=digest.hexdigest())
    finally:
        if child is not None:
            child.kill()
        for fd in (read, write):
            if fd is not None:
                os.close(fd)


def update(data):
    import fcntl
    lock = data['state'] / 'definitions.lock'
    fd = os.open(lock, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        info = os.fstat(fd)
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid == os.geteuid()
                and not info.st_mode & 0o077, 'Unsafe definition updater lock')
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return False
        try:
            capacity(data)
            configuration = prepared_config(data)
            status(data, {'status': 'running'})
            result = command(data, configuration)
            capacity(data)
            status(data, dict(status='complete', **result))
            return True
        except (OSError, ValueError, ModError, subprocess.SubprocessError):
            status(data, {'status': 'error', 'reason': 'Definition update failed; scans retain their freshness gate'})
            raise
    finally:
        os.close(fd)


def generate(data, output):
    output = Path(os.path.abspath(output))
    private_directory(output.parent)
    require(not output.exists() and not output.is_symlink(), 'Choose a new definitions bundle')
    prepared_config(data)
    output.mkdir(mode=0o700)
    value = dict(Label=data['label'], ProgramArguments=[str(Path(sys.executable).absolute()), '-I', '-B',
                 str(Path(__file__).resolve()), '--config', str(data['config']), '--once'],
                 RunAtLoad=True, StartInterval=INTERVAL, ProcessType='Background', Umask=0o077,
                 AbandonProcessGroup=False, HardResourceLimits=dict(Core=0, CPU=180, FileSize=512 * 1024**2),
                 StandardOutPath='/dev/null', StandardErrorPath='/dev/null')
    exclusive(output / (data['label'] + '.plist'), plistlib.dumps(value))
    return dict(bundle=str(output), label=data['label'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--once', action='store_true')
    modes.add_argument('--generate', type=Path)
    args = parser.parse_args()
    try:
        require(sys.platform == 'darwin', 'Run definition maintenance on the Mac host')
        data = config(args.config)
        if args.generate:
            print(json.dumps(generate(data, args.generate)))
        else:
            update(data)
        return 0
    except (ModError, OSError, ValueError, subprocess.SubprocessError) as error:
        print('Definition service: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
