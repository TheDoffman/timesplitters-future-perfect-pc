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
    data = {'label': 'org.timesplitters.workshop.scan.fixture'}
    monkeypatch.setattr(service, 'target', lambda _: 'gui/123/' + data['label'])
    return data


def commands(rss='1024', pid=456, pgid=None):
    calls = []
    def command(args):
        calls.append(args)
        if args[1] == 'print':
            return 0, 'service = {\n pid = ' + str(pid) + '\n}'
        if args[1] == 'kill':
            return 0, ''
        if args[-1] == 'pgid=':
            return 0, str(pid if pgid is None else pgid)
        return 0, rss
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


def test_idle_missing_service_and_ambiguous_identity(managed):
    assert service.watch_once(managed, {}, lambda _: (1, 'not loaded'))[1]['status'] == 'error'
    assert service.watch_once(managed, {}, lambda _: (0, 'state = idle'))[1]['status'] == 'idle'
    with pytest.raises(ModError, match='Ambiguous'):
        service.managed_pid('pid = 1\npid = 2\n')


@pytest.mark.parametrize('rss,pgid', [('bad data', 456), ('1\n' * 9, 456), ('100', 99)])
def test_bad_samples_and_group_escape_fail_closed(managed, rss, pgid):
    command, _ = commands(rss, pgid=pgid)
    with pytest.raises(ModError):
        service.watch_once(managed, {}, command)


@pytest.fixture
def private_config(tmp_path):
    if os.name != 'posix':
        pytest.skip('Private Mac/POSIX service files')
    tmp_path.chmod(0o700)
    for name in ('repository', 'database', 'state'):
        (tmp_path / name).mkdir(mode=0o700)
    value = dict(schema=1, scanner=sys.executable, **{name: str(tmp_path / name) for name in ('repository', 'database', 'state')})
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
    service.status(private_config, 'watchdog', {'status': 'idle'})
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
