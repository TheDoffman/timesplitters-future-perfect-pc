"""Private Mac-host scanner process: deny by default, no network or child processes."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading

from mod_package import require

ISOLATION_POLICY = 'macos-seatbelt-v2'


def sandbox_profile(executable, content, database, scratch):
    """Only public runtime files, this upload and official definitions are readable."""
    quote = lambda p: json.dumps(str(p), ensure_ascii=True)
    runtime = ['/System', '/usr/lib', '/usr/share', '/private/preboot/Cryptexes',
               '/opt/homebrew/Cellar', '/usr/local/Cellar']
    public_certs = ['/opt/homebrew/etc/clamav/certs', '/usr/local/etc/clamav/certs']
    subpaths = lambda paths: ''.join('(subpath ' + quote(p) + ')' for p in paths)
    literals = lambda paths: ''.join('(literal ' + quote(p) + ')' for p in paths)
    # The root directory itself is needed by the macOS27 dyld bootstrap. It does
    # not grant traversal/read access to the contents of any other directory.
    reads = subpaths([*runtime, *public_certs, content, database, scratch]) + literals([
        '/', '/dev/null', '/dev/random', '/dev/urandom', '/private/etc/localtime', executable])
    return ('(version 1)(deny default)'
            '(allow process-exec (literal ' + quote(executable) + '))'
            '(allow sysctl-read)(allow file-read-metadata)'
            '(allow file-read* ' + reads + ')'
            '(allow file-map-executable ' + subpaths(runtime) + literals([executable]) + ')'
            '(allow file-write* (subpath ' + quote(scratch) + '))')


class ScanSandbox:
    """No fallback to an unsandboxed engine on unsupported/broken hosts."""
    def __init__(self, executable, content, database):
        self.requested_executable = executable
        self.content = Path(content).resolve()
        self.database = database
        self.temporary = None

    def __enter__(self):
        require(sys.platform == 'darwin' and Path('/usr/bin/sandbox-exec').is_file(),
                'Isolated scanning requires the configured Mac workshop host')
        require(self.database is not None, 'An explicit official scanner database directory is required')
        self.database = Path(self.database).resolve()
        require(self.content.is_dir() and self.database.is_dir(), 'Missing content or scanner database')
        found = shutil.which(self.requested_executable)
        require(found is not None, 'Scanner executable is missing')
        self.executable = Path(found).resolve()
        require(self.executable.is_file(), 'Scanner executable is missing')
        self.temporary = tempfile.TemporaryDirectory(prefix='tsfp-scan-worker-')
        self.scratch = Path(self.temporary.name).resolve()
        os.chmod(self.scratch, 0o700)
        self.profile = sandbox_profile(self.executable, self.content, self.database, self.scratch)
        self.environment = dict(PATH='/usr/bin:/bin', HOME=str(self.scratch),
                                TMPDIR=str(self.scratch), LC_ALL='C', TZ='UTC')
        return self

    def command(self, arguments):
        # The trusted child sets hard CPU/output/fd limits before exec. Unlike a
        # preexec_fn it is safe when the parent has other threads. exec preserves
        # the PID, so subprocess timeout kills the actual sandboxed engine.
        return [sys.executable, str(Path(__file__).with_name('scan_child.py')),
                '/usr/bin/sandbox-exec', '-p', self.profile, str(self.executable), *arguments]

    def run(self, arguments, **kwargs):
        return subprocess.run(self.command(arguments), env=self.environment, cwd=self.scratch,
                              stdin=subprocess.DEVNULL, close_fds=True, **kwargs)

    def scan(self, arguments, timeout=180):
        """Drain diagnostics without an unbounded file, buffer, or pipe deadlock."""
        command = self.command(arguments)
        child = subprocess.Popen(command, env=self.environment, cwd=self.scratch,
                                 stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, close_fds=True)
        captured = bytearray()
        exceeded = []
        failures = []
        def drain():
            total = 0
            try:
                while chunk := child.stdout.read(65536):
                    total += len(chunk)
                    captured.extend(chunk[:max(0, 65536 - len(captured))])
                    if total > 1024 * 1024:
                        exceeded.append(True)
                        child.kill()
                        break
            except OSError as exc:
                failures.append(str(exc))
                child.kill()
        reader = threading.Thread(target=drain, daemon=True)
        reader.start()
        try:
            child.wait(timeout=timeout)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=10)
            reader.join(timeout=10)
            child.stdout.close()
        require(not reader.is_alive(), 'Scanner diagnostic reader did not stop')
        require(not failures, 'Scanner diagnostic stream failed')
        require(not exceeded, 'Scanner diagnostic output exceeded1MiB')
        return subprocess.CompletedProcess(command, child.returncode, stdout=bytes(captured))

    def __exit__(self, *args):
        if self.temporary is not None:
            self.temporary.cleanup()
