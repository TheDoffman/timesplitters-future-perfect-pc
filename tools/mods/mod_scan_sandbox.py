"""Private Mac-host scanner process: deny by default, no network or child processes."""
import json
from contextlib import contextmanager
import hashlib
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import stat

from mod_package import require, read_json
from mod_runtime_files import runtime_access

ISOLATION_POLICY = 'macos-seatbelt-v3'
GUARDIAN_ROOT = Path(__file__).resolve().parents[2] / 'build/scan-guardian'
_AUTO_RUNTIME = object()


def verified_guardian():
    root = GUARDIAN_ROOT
    binary, receipt = root / 'scan-guardian-mac', root / 'receipt.json'
    for path in (binary, receipt):
        for member in (path, *path.parents):
            require(not member.is_symlink(), 'Linked scanner guardian path refused')
        info = path.stat()
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid in (0, os.geteuid())
                and not info.st_mode & 0o6022 and 0 < info.st_size <= (1024**2 if path == binary else 4096),
                'Unsafe scanner guardian file')
    require(not root.stat().st_mode & 0o022, 'Scanner guardian directory is writable by others')
    record = read_json(receipt.read_bytes())
    source = Path(__file__).with_name('scan_guardian_mac.c')
    require(isinstance(record, dict) and type(record.get('schema')) is int and record['schema'] == 1
            and record.get('producer') == 'tsfp-scan-guardian-v1'
            and record.get('source_sha256') == hashlib.sha256(source.read_bytes()).hexdigest()
            and record.get('sha256') == hashlib.sha256(binary.read_bytes()).hexdigest()
            and os.access(binary, os.X_OK), 'Scanner guardian receipt is missing, stale or changed')
    return binary


class GuardedChild:
    def __init__(self, process, lifetime):
        self.process, self.lifetime = process, lifetime
        self.lock = threading.Lock()

    def kill(self):
        # Never kill the guardian first. Closing this private parent endpoint
        # tells it to kill and reap its still-owned scanner PID, in any group.
        with self.lock:
            descriptor, self.lifetime = self.lifetime, None
        if descriptor is not None:
            os.close(descriptor)

    def __getattr__(self, name):
        return getattr(self.process, name)


def sandbox_profile(executable, content, database, scratch, access=_AUTO_RUNTIME):
    """Only public runtime files, this upload and official definitions are readable."""
    quote = lambda p: json.dumps(str(p), ensure_ascii=True)
    runtime = ['/System', '/usr/lib', '/usr/share', '/private/preboot/Cryptexes',
               '/opt/homebrew/Cellar', '/usr/local/Cellar']
    public_certs = ['/opt/homebrew/etc/clamav/certs', '/usr/local/etc/clamav/certs']
    access = runtime_access(executable) if access is _AUTO_RUNTIME else access
    bundled = access['images'] if access is not None else None
    certificate_reads = [access['certificate'].parent, access['certificate']] if access is not None else []
    if bundled is not None:
        runtime = runtime[:4]
        public_certs = []
    subpaths = lambda paths: ''.join('(subpath ' + quote(p) + ')' for p in paths)
    literals = lambda paths: ''.join('(literal ' + quote(p) + ')' for p in paths)
    # The root directory itself is needed by the macOS27 dyld bootstrap. It does
    # not grant traversal/read access to the contents of any other directory.
    reads = subpaths([*runtime, *public_certs, content, database, scratch]) + literals([
        '/', '/dev/null', '/dev/random', '/dev/urandom', '/private/etc/localtime', executable,
        *(bundled or []), *certificate_reads])
    return ('(version 1)(deny default)'
            # macOS process-argument sysctls use process-info checks rather
            # than the named sysctl gate; explicit denial is necessary here.
            '(deny process-info*)'
            '(allow process-exec (literal ' + quote(executable) + '))'
            # Process-argument sysctls expose other same-user environments even
            # when their files are denied. Grant only public runtime metadata.
            '(allow sysctl-read (sysctl-name-regex #"^hw\\.")'
            '(sysctl-name "kern.osversion")(sysctl-name "kern.osrelease")'
            '(sysctl-name "kern.ostype")(sysctl-name "kern.version")'
            '(sysctl-name "kern.argmax"))'
            '(allow file-read-metadata)'
            '(allow file-read* ' + reads + ')'
            '(allow file-map-executable ' + subpaths(runtime) + literals([executable, *(bundled or [])]) + ')'
            '(allow file-write* (subpath ' + quote(scratch) + '))')


class ScanSandbox:
    """No fallback to an unsandboxed engine on unsupported/broken hosts."""
    GUARDED_PARENT_LIFETIME = True
    def __init__(self, executable, content, database):
        self.requested_executable = executable
        self.content = Path(content).resolve()
        self.database = database
        self.temporary = None

    def __enter__(self):
        require(sys.platform == 'darwin' and Path('/usr/bin/sandbox-exec').is_file(),
                'Isolated scanning requires the configured Mac workshop host')
        require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL,
                'Scanner controller must retain its own child exit status')
        require(self.database is not None, 'An explicit official scanner database directory is required')
        self.database = Path(self.database).resolve()
        require(self.content.is_dir() and self.database.is_dir(), 'Missing content or scanner database')
        found = shutil.which(self.requested_executable)
        require(found is not None, 'Scanner executable is missing')
        self.executable = Path(found).resolve()
        require(self.executable.is_file(), 'Scanner executable is missing')
        require(not self.executable.stat().st_mode & 0o6022, 'Unsafe scanner executable permissions')
        access = runtime_access(self.executable)
        self.guardian = verified_guardian()
        self.temporary = tempfile.TemporaryDirectory(prefix='tsfp-scan-worker-')
        self.scratch = Path(self.temporary.name).resolve()
        os.chmod(self.scratch, 0o700)
        self.profile = sandbox_profile(self.executable, self.content, self.database, self.scratch, access)
        self.environment = dict(PATH='/usr/bin:/bin', HOME=str(self.scratch),
                                TMPDIR=str(self.scratch), LC_ALL='C', TZ='UTC')
        if access is not None:
            self.environment['CVD_CERTS_DIR'] = str(access['certificate'].parent)
        return self

    def command(self, arguments):
        # The trusted child sets hard CPU/output/fd limits before exec. Unlike a
        # preexec_fn it is safe when the parent has other threads. exec preserves
        # the PID. A separate owned-child guardian also enforces parent lifetime.
        return [sys.executable, str(Path(__file__).with_name('scan_child.py')),
                '/usr/bin/sandbox-exec', '-p', self.profile, str(self.executable), *arguments]

    @contextmanager
    def _spawn(self, arguments, merge=False):
        read, write = os.pipe()
        process = None
        try:
            process = subprocess.Popen([str(self.guardian), str(read), *self.command(arguments)],
                                       env=self.environment, cwd=self.scratch, stdin=subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT if merge else subprocess.PIPE,
                                       close_fds=True, pass_fds=(read,), start_new_session=True)
            os.close(read)
            read = None
            child = GuardedChild(process, write)
            write = None
            try:
                yield child
            finally:
                child.kill()
                process.wait(timeout=10)
        finally:
            for descriptor in (read, write):
                if descriptor is not None:
                    os.close(descriptor)

    def run(self, arguments, **kwargs):
        timeout = kwargs.pop('timeout', 10)
        check = kwargs.pop('check', False)
        kwargs.pop('capture_output', False)
        require(not kwargs, 'Unsupported scanner process options')
        result = self._collect(arguments, timeout, merge=False)
        if check:
            result.check_returncode()
        return result

    def scan(self, arguments, timeout=180):
        return self._collect(arguments, timeout, merge=True)

    def _collect(self, arguments, timeout, merge):
        """Drain diagnostics without an unbounded file, buffer, or pipe deadlock."""
        captured = [bytearray(), bytearray()]
        exceeded = []
        failures = []
        def drain(pipe, index, child):
            total = 0
            try:
                while chunk := pipe.read(65536):
                    total += len(chunk)
                    captured[index].extend(chunk[:max(0, 65536 - len(captured[index]))])
                    if total > 1024 * 1024:
                        exceeded.append(True)
                        child.kill()
                        break
            except OSError as exc:
                failures.append(str(exc))
                child.kill()
        with self._spawn(arguments, merge=merge) as child:
            pipes = [child.stdout] + ([] if merge else [child.stderr])
            readers = [threading.Thread(target=drain, args=(pipe, index, child), daemon=True)
                       for index, pipe in enumerate(pipes)]
            for reader in readers:
                reader.start()
            try:
                child.wait(timeout=timeout)
            finally:
                child.kill()
                child.wait(timeout=10)
                for reader, pipe in zip(readers, pipes):
                    reader.join(timeout=10)
                    pipe.close()
            require(not any(reader.is_alive() for reader in readers), 'Scanner diagnostic reader did not stop')
            returncode = child.returncode
        require(not failures, 'Scanner diagnostic stream failed')
        require(not exceeded, 'Scanner diagnostic output exceeded1MiB')
        return subprocess.CompletedProcess(self.command(arguments), returncode, stdout=bytes(captured[0]),
                                           stderr=None if merge else bytes(captured[1]))

    def __exit__(self, *args):
        if self.temporary is not None:
            self.temporary.cleanup()
