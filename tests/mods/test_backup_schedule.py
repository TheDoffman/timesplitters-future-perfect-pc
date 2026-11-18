"""Fixed scheduler action authority and safe removal of a damaged own runtime."""
from datetime import datetime, timedelta, timezone
import hashlib
import os
from pathlib import Path
import subprocess
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_backup_schedule as schedule
from mod_package import ModError
from mod_trust import encode


@pytest.fixture
def spec(tmp_path):
    root = tmp_path / 'frozen runtime'
    control = tmp_path / 'worker'
    args = ['-I', '-S', '-B', str(root / 'bootstrap.py'), '--runtime-sha256', 'a' * 64,
            '--control', str(control), '--binding-sha256', 'b' * 64]
    return dict(schema=1, type=schedule.TYPE, job_id='c' * 32, sid='S-1-5-21-1-2-3-1001',
                runtime=str(root), runtime_sha256='a' * 64, control=str(control), binding_sha256='b' * 64,
                executable=str(root / 'runtime/pythonw.exe'), arguments=subprocess.list2cmdline(args),
                start_boundary=(datetime.now(timezone.utc) + timedelta(hours=1)).replace(microsecond=0).isoformat())


@pytest.mark.parametrize('field,value', [('arguments', 'arbitrary command'), ('executable', '/other'),
                          ('runtime_sha256', 'd' * 64), ('binding_sha256', 'd' * 64),
                          ('job_id', '../escape'), ('sid', 'S-1-5-18'), ('start_boundary', '2026-11-18')])
def test_changed_action_or_authority_is_refused(spec, field, value):
    assert schedule.check(spec) == spec
    spec[field] = value
    with pytest.raises((ModError, ValueError)):
        schedule.check(spec)


@pytest.fixture
def stored(tmp_path, spec):
    path = tmp_path / 'private-schedule'
    if os.name == 'nt':
        from mod_windows_recovery import private
        private(path, create=True)
    else:
        path.mkdir(mode=0o700)
    wire = encode(spec)
    file = path / schedule.SPEC
    file.write_bytes(wire)
    if os.name != 'nt':
        file.chmod(0o600)
    return path, hashlib.sha256(wire).hexdigest(), spec


def test_remove_checks_pinned_task_identity_even_when_runtime_missing(stored, monkeypatch):
    path, pin, spec = stored
    assert not Path(spec['runtime']).exists()
    monkeypatch.setattr(schedule, 'ready', lambda *_: pytest.fail('Removal must not execute a damaged runtime'))
    calls = []
    def native(operation, file, fingerprint):
        calls.append((operation, file, fingerprint))
        return dict(schema=1, phase='backup-schedule-removed', job_id=spec['job_id'])
    monkeypatch.setattr(schedule, 'powershell', native)
    assert schedule.manage('Remove', path, pin)['phase'] == 'backup-schedule-removed'
    assert calls == [('Remove', path / schedule.SPEC, pin)]


def test_changed_schedule_cannot_select_any_native_task(stored, monkeypatch):
    path, pin, spec = stored
    monkeypatch.setattr(schedule, 'powershell', lambda *a: pytest.fail('Changed metadata must not reach the scheduler'))
    file = path / schedule.SPEC
    file.write_bytes(file.read_bytes() + b' ')
    with pytest.raises(ModError, match='fingerprint'):
        schedule.manage('Remove', path, pin)
    assert file.read_bytes().endswith(b' ')


def test_install_and_start_require_fresh_verified_backup(stored, monkeypatch):
    path, pin, spec = stored
    def refuse(*args):
        raise ModError('Receiver backup is stale')
    monkeypatch.setattr(schedule, 'ready', refuse)
    monkeypatch.setattr(schedule, 'powershell', lambda *a: pytest.fail('An unready backup must not be scheduled'))
    for operation in ('Install', 'Run'):
        with pytest.raises(ModError, match='stale'):
            schedule.manage(operation, path, pin)
