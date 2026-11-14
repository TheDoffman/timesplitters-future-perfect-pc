"""Exercise parent output and timeout bounds using actual harmless child processes."""
import os
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
