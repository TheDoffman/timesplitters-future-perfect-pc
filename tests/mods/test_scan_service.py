"""Mac host policy, managed-job cancellation and private launchd artifacts."""
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import sys
import time

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
from mod_package import ModError
spec = importlib.util.spec_from_file_location('scan_service', ROOT / 'tools/mods/hosting/scan-service-mac.py')
service = importlib.util.module_from_spec(spec)
spec.loader.exec_module(service)


@pytest.fixture
def managed(monkeypatch):
    data = {'label': 'org.timesplitters.workshop.scan.fixture', 'uid': 123}
    monkeypatch.setattr(service, 'target', lambda _: 'gui/123/' + data['label'])
    return data


def commands(rss='1024', pid=456, rows=None):
    calls = []
    def command(args):
        calls.append(args)
        if args[1] == 'print':
            return 0, 'service = {\n pid = ' + str(pid) + '\n}'
        if args[1] == 'kill':
            return 0, ''
        return 0, rows if rows is not None else str(pid) + ' 1 ' + rss + ' 123\n'
    return command, calls


def test_memory_fault_signals_managed_service_not_sampled_pid(managed):
    command, calls = commands(str(service.MEMORY_BYTES // 1024 + 1))
    observed, result = service.watch_once(managed, {}, command)
    assert result['status'] == 'terminated' and result['reason'] == 'memory'
    assert calls[-1] == ['/bin/launchctl', 'kill', 'SIGKILL', 'gui/123/' + managed['label']]
    assert '456' not in calls[-1] and observed == {}


def test_wall_deadline_and_identity_change(managed):
    command, calls = commands()
    _, result = service.watch_once(managed, {'pid': 456, 'start': 0}, command, lambda: 481)
    assert result['reason'] == 'wall-time'
    observed, result = service.watch_once(managed, {'pid': 999, 'start': 0}, command, lambda: 481)
    assert result['status'] == 'watching' and observed['start'] == 481


def test_failed_service_stop_never_claims_termination(managed):
    run,calls = commands(str(service.MEMORY_BYTES//1024+1))
    def command(args):
        return (1,'permission denied') if args[1]=='kill' else run(args)
    with pytest.raises(ModError,match='Cannot stop'):
        service.watch_once(managed,{},command)


def test_idle_missing_service_and_ambiguous_identity(managed):
    assert service.watch_once(managed, {}, lambda _: (1, 'not loaded'))[1]['status'] == 'error'
    assert service.watch_once(managed, {}, lambda _: (0, 'state = idle'))[1]['status'] == 'idle'
    with pytest.raises(ModError, match='Ambiguous'):
        service.managed_pid('pid = 1\npid = 2\n')


@pytest.mark.parametrize('rows', ['bad data', '456 1 100 999',
                                '456 1 1 123\n' + ''.join(str(i)+' 456 1 123\n' for i in range(600,608))])
def test_bad_samples_owner_changes_and_process_budget_fail_closed(managed, rows):
    command, _ = commands(rows=rows)
    with pytest.raises(ModError):
        service.watch_once(managed, {}, command)


def test_descendants_are_counted_without_session_or_group_membership(managed):
    command, _ = commands(rows='456 1 1 123\n600 456 2 123\n601 600 3000 123\n999 1 9000000 -2\n')
    _, result = service.watch_once(managed, {}, command)
    assert result['rss_bytes'] == 3003 * 1024
    assert result['supervision_policy'] == service.SUPERVISION_POLICY


@pytest.fixture
def private_config(tmp_path):
    if os.name != 'posix':
        pytest.skip('Private Mac/POSIX service files')
    tmp_path.chmod(0o700)
    for name in ('repository', 'database', 'state'):
        (tmp_path / name).mkdir(mode=0o700)
    # Hosted Python installations can be group-writable. Keep that real host
    # policy intact and give these config-only tests their own inert executable.
    scanner = tmp_path / 'scanner-fixture'
    scanner.write_text('#!/bin/sh\nexit 99\n')
    scanner.chmod(0o700)
    value = dict(schema=1, scanner=str(scanner), **{name: str(tmp_path / name) for name in ('repository', 'database', 'state')})
    path = tmp_path / 'service.json'
    path.write_text(json.dumps(value))
    path.chmod(0o600)
    return service.config(path)


def test_generation_is_new_only_and_preserves_environment(private_config, tmp_path):
    result = service.generate(private_config, tmp_path / 'bundle')
    files = list((tmp_path / 'bundle').glob('*.plist'))
    assert len(files) == 2
    for path in files:
        value = plistlib.loads(path.read_bytes())
        assert value['ProgramArguments'][0] == str(Path(sys.executable).absolute())
        assert value['AbandonProcessGroup'] is False
        assert value['StandardOutPath'] == value['StandardErrorPath'] == '/dev/null'
        assert path.stat().st_mode & 0o077 == 0
        if value['Label'] == result['worker_label']:
            assert value['StartInterval'] == 60 and value['RunAtLoad']
        else:
            assert value['KeepAlive'] and value['ThrottleInterval'] == 5
    with pytest.raises(ModError, match='new'):
        service.generate(private_config, tmp_path / 'bundle')


def test_health_is_private_fresh_bound_and_preserves_fault(private_config):
    with pytest.raises(OSError):
        service.healthy(private_config)
    service.status(private_config, 'watchdog', {'status': 'terminated', 'reason': 'memory'})
    with pytest.raises(ModError, match='healthy'):
        service.healthy(private_config)
    service.status(private_config, 'watchdog', {'status': 'idle', 'supervision_policy': service.SUPERVISION_POLICY})
    service.healthy(private_config)
    path = service.status_file(private_config, 'watchdog')
    value = json.loads(path.read_text())
    assert value['last_fault']['reason'] == 'memory'
    value['time'] = int(time.time()) - 60
    path.write_text(json.dumps(value))
    with pytest.raises(ModError, match='healthy'):
        service.healthy(private_config)
    value['service'] = 'different-service'
    path.write_text(json.dumps(value))
    with pytest.raises(ModError, match='unrecognized'):
        service.status(private_config, 'watchdog', {'status': 'idle'})


def test_unrecognized_status_is_preserved(private_config):
    path = service.status_file(private_config, 'worker')
    path.write_text('personal data')
    with pytest.raises(ModError):
        service.status(private_config, 'worker', {'status': 'running'})
    assert path.read_text() == 'personal data'


def test_admission_waits_without_touching_repository_until_supervised(private_config, monkeypatch):
    from mod_scan_sandbox import ScanSandbox
    monkeypatch.setattr(ScanSandbox, 'GUARDED_PARENT_LIFETIME', True, raising=False)
    assert not service.admit(private_config)
    assert list(private_config['repository'].iterdir()) == []
    value = json.loads(service.status_file(private_config, 'worker').read_text())
    assert value['status'] == 'waiting' and 'last_fault' not in value
    service.status(private_config, 'watchdog', {'status': 'idle', 'supervision_policy': service.SUPERVISION_POLICY})
    assert service.admit(private_config)


def test_system_worker_ignores_worker_writable_gui_heartbeat(private_config,monkeypatch):
    from types import SimpleNamespace
    from mod_scan_sandbox import ScanSandbox
    monkeypatch.setattr(ScanSandbox,'GUARDED_PARENT_LIFETIME',True)
    service.status(private_config,'watchdog',dict(status='idle',supervision_policy=service.SUPERVISION_POLICY))
    private_config['system_monitor']=True
    def absent(_):raise ModError('No root-owned heartbeat')
    monkeypatch.setattr(service,'system_monitor',lambda:SimpleNamespace(healthy=absent))
    assert not service.admit(private_config) and list(private_config['repository'].iterdir())==[]
    calls=[]
    monkeypatch.setattr(service,'system_monitor',lambda:SimpleNamespace(healthy=lambda uid:calls.append(uid)))
    assert service.admit(private_config) and calls==[os.geteuid()]


def test_group_only_containment_is_held_until_both_components_upgrade(private_config, monkeypatch):
    from mod_scan_sandbox import ScanSandbox
    monkeypatch.setattr(ScanSandbox, 'GUARDED_PARENT_LIFETIME', False)
    assert not service.admit(private_config)
    assert json.loads(service.status_file(private_config, 'worker').read_text())['status'] == 'held'
    monkeypatch.setattr(ScanSandbox, 'GUARDED_PARENT_LIFETIME', True, raising=False)
    service.status(private_config, 'watchdog', {'status': 'idle'})
    assert not service.admit(private_config)
    assert list(private_config['repository'].iterdir()) == []


def test_repository_preflight_preserves_personal_and_unrecognized_data(private_config):
    import sqlite3
    root = private_config['repository']
    service.repository_preflight(root)
    personal = root / 'personal.txt'
    personal.write_text('preserve')
    with pytest.raises(ModError, match='nonempty'):
        service.repository_preflight(root)
    assert personal.read_text() == 'preserve'
    with sqlite3.connect(root / 'catalog.sqlite') as db:
        db.execute('CREATE TABLE personal (note TEXT)')
    with pytest.raises(ModError, match='schema'):
        service.repository_preflight(root)
    with sqlite3.connect(root / 'catalog.sqlite') as db:
        assert db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall() == [('personal',)]


def test_legacy_process_info_policy_is_held_even_with_healthy_supervision(private_config, monkeypatch):
    import mod_scan_sandbox
    monkeypatch.setattr(mod_scan_sandbox, 'ISOLATION_POLICY', 'macos-seatbelt-v2')
    service.status(private_config, 'watchdog', {'status': 'idle', 'supervision_policy': service.SUPERVISION_POLICY})
    assert not service.admit(private_config)
    assert json.loads(service.status_file(private_config, 'worker').read_text())['status'] == 'held'
    assert list(private_config['repository'].iterdir()) == []
