"""Exercise parent output and timeout bounds using actual harmless child processes."""
import os
from contextlib import contextmanager
from pathlib import Path
import subprocess
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_package import ModError
from mod_scan_sandbox import ScanSandbox


def process(tmp_path, script, monkeypatch):
    # Bypass only the platform command builder in this test. Public scanner
    # construction still requires Mac Seatbelt and has no disable-isolation flag.
    worker = ScanSandbox(sys.executable, tmp_path, tmp_path)
    worker.scratch = tmp_path
    worker.environment = os.environ.copy()
    monkeypatch.setattr(worker, 'command', lambda arguments: [sys.executable, '-c', script])
    @contextmanager
    def spawn(arguments, merge=False):
        # Test-only seam for portable pipe/timeout tests. Production has no
        # unguarded fallback; real Darwin guardian lifetimes are tested separately.
        child = subprocess.Popen(worker.command(arguments), env=worker.environment, cwd=worker.scratch,
                                 stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT if merge else subprocess.PIPE)
        try:
            yield child
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
    monkeypatch.setattr(worker, '_spawn', spawn)
    return worker


def test_output_flood_is_killed_and_withheld(tmp_path, monkeypatch):
    worker = process(tmp_path, 'import sys\nwhile True: sys.stdout.buffer.write(b"x"*65536)', monkeypatch)
    with pytest.raises(ModError, match='output exceeded'):
        worker.scan([], timeout=10)


def test_timeout_stops_real_child(tmp_path, monkeypatch):
    worker = process(tmp_path, 'import time; time.sleep(30)', monkeypatch)
    with pytest.raises(subprocess.TimeoutExpired):
        worker.scan([], timeout=.25)


def test_success_and_error_diagnostics_preserve_exit_status(tmp_path, monkeypatch):
    worker = process(tmp_path, 'import sys; print("engine error", file=sys.stderr); sys.exit(2)', monkeypatch)
    result = worker.scan([], timeout=10)
    assert result.returncode == 2 and result.stdout.strip() == b'engine error'
    worker = process(tmp_path, 'print("scan result")', monkeypatch)
    result = worker.scan([], timeout=10)
    assert result.returncode == 0 and result.stdout.strip() == b'scan result'


def test_version_stderr_flood_is_bounded_too(tmp_path, monkeypatch):
    worker = process(tmp_path, 'import sys\nwhile True: sys.stderr.buffer.write(b"x"*65536)', monkeypatch)
    with pytest.raises(ModError, match='output exceeded'):
        worker.run([], capture_output=True, timeout=10)


def test_version_streams_and_check_semantics(tmp_path, monkeypatch):
    worker = process(tmp_path, 'import sys; print("version"); print("warning",file=sys.stderr);sys.exit(2)', monkeypatch)
    result = worker.run([], capture_output=True, timeout=10)
    assert result.stdout.strip() == b'version' and result.stderr.strip() == b'warning'
    with pytest.raises(subprocess.CalledProcessError):
        worker.run([], capture_output=True, check=True, timeout=10)


def test_real_bootstrap_ignores_poisoned_python_path_and_writes_no_bytecode(tmp_path):
    marker = tmp_path / 'customization-executed'
    (tmp_path / 'sitecustomize.py').write_text('from pathlib import Path\nPath(' + repr(str(marker)) + ').write_text("unexpected")\n')
    environment = dict(os.environ, PYTHONPATH=str(tmp_path))
    subprocess.run([sys.executable, '-B', '-c', 'pass'], env=environment, check=True, capture_output=True, timeout=10)
    assert marker.read_text() == 'unexpected'
    marker.unlink()
    worker = ScanSandbox(sys.executable, tmp_path, tmp_path)
    worker.executable = tmp_path / 'missing-scanner'
    worker.profile = '(version 1)(deny default)'
    result = subprocess.run(worker.command([]), env=environment, cwd=tmp_path, capture_output=True, timeout=10)
    # The nonexistent target is intentional; only interpreter startup is under test.
    assert result.returncode != 0
    assert not marker.exists() and not (tmp_path / '__pycache__').exists()
