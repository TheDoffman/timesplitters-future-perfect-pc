"""Mac scanner sandbox acceptance against disposable canaries, never personal files."""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
from mod_scan_sandbox import ScanSandbox, sandbox_profile
from mod_runtime_files import runtime_access


@contextmanager
def isolated(executable, content, database, access):
    # Harmless system/C canaries need their own exec literal. Reuse the actual
    # bundled scanner's verified read/map plan; production offers no such override.
    with ScanSandbox(executable, content, database) as worker:
        if access is not None:
            worker.profile = sandbox_profile(Path(executable), content, database, worker.scratch, access)
            worker.environment['CVD_CERTS_DIR'] = str(access['certificate'].parent)
        yield worker


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--runtime-executable', type=Path,
                        help='Use this packaged scanner\'s verified library/certificate permissions for canaries')
    args = parser.parse_args()
    access = runtime_access(args.runtime_executable) if args.runtime_executable else None
    assert not args.runtime_executable or access is not None, 'Packaged runtime receipt is required'
    with tempfile.TemporaryDirectory(prefix='tsfp-isolation-canaries-') as temporary:
        root = Path(temporary).resolve()
        content = root / 'content'
        content.mkdir()
        allowed = content / 'clean.lua'
        allowed.write_text('return {}\n')
        secret = root / 'private-canary'
        secret.write_text('Disposable secret must remain private\n')
        probe = root / 'limits-probe'
        subprocess.run(['clang','-std=c11',str(ROOT / 'tests/mods/scan_limits_probe.c'),'-o',str(probe)],
                       check=True, capture_output=True, timeout=30)
        with isolated(str(probe), content, args.database, access) as worker:
            result = worker.run([], capture_output=True, timeout=10)
            assert result.returncode == 0, result
            limits = json.loads(result.stdout)
            assert 170 <= limits.pop('alarm') <= 180, limits
            assert limits == dict(cpu=150, file=512*1024*1024, fds=128, core=0), limits
        with isolated('/bin/cat', content, args.database, access) as worker:
            permitted = worker.run([str(allowed)], capture_output=True, timeout=10)
            assert permitted.returncode == 0 and permitted.stdout == allowed.read_bytes(), permitted
            denied = worker.run([str(secret)], capture_output=True, timeout=10)
            assert denied.returncode != 0 and denied.stdout == b'', denied
            escape = worker.scratch / 'symlink-escape'
            escape.symlink_to(secret)
            denied = worker.run([str(escape)], capture_output=True, timeout=10)
            assert denied.returncode != 0 and denied.stdout == b'', denied
        # macOS27 /bin/sh dispatches to /bin/bash via exec; use the actual
        # executable so the test does not request an intentionally denied exec.
        with isolated('/bin/bash', content, args.database, access) as worker:
            limits = worker.run(['-c', 'ulimit -t; ulimit -n; ulimit -c'],
                                capture_output=True, timeout=10)
            assert limits.returncode == 0 and limits.stdout.splitlines() == [b'150', b'128', b'0'], limits
            denied = worker.run(['-c', 'printf changed > "$1"', 'probe', str(secret)],
                                capture_output=True, timeout=10)
            assert denied.returncode != 0, denied
            assert secret.read_text() == 'Disposable secret must remain private\n'
            target = worker.scratch / 'permitted-output'
            permitted = worker.run(['-c', 'printf allowed > "$1"', 'probe', str(target)],
                                   capture_output=True, timeout=10)
            assert permitted.returncode == 0 and target.read_text() == 'allowed', permitted
            denied = worker.run(['-c', '/bin/cat "$1"', 'probe', str(allowed)],
                                capture_output=True, timeout=10)
            assert denied.returncode != 0 and denied.stdout == b'', denied
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            listener.settimeout(3)
            connections = []
            def accept():
                try:
                    connection, _ = listener.accept()
                    connections.append(True)
                    with connection:
                        connection.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n')
                except TimeoutError:
                    pass
            thread = threading.Thread(target=accept)
            thread.start()
            with isolated('/usr/bin/curl', content, args.database, access) as worker:
                denied = worker.run(['--connect-timeout', '1', '--max-time', '2', '--noproxy', '*',
                                     'http://127.0.0.1:' + str(listener.getsockname()[1])],
                                    capture_output=True, timeout=10)
                assert denied.returncode != 0, denied
            thread.join(timeout=5)
            assert not thread.is_alive() and not connections, 'Sandbox connected to the private listener'
        print('PASS: permitted input/temp writes; denied private read/write, child exec and network; '
              + ('verified packaged runtime literals' if access is not None else 'unbundled runtime policy'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
