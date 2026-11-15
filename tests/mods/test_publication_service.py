"""Managed publication admission, health, sampled bounds and owned agent output."""
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
spec = importlib.util.spec_from_file_location('publication_service', ROOT / 'tools/mods/hosting/live-publication-mac.py')
service = importlib.util.module_from_spec(spec)
spec.loader.exec_module(service)


def commands(rows='456 1 100 123\n600 456 100 123\n601 600 100 123\n999 1 100 -2\n'):
    calls = []
    def run(args):
        calls.append(args)
        if args[1] == 'print':
            return 0, 'pid = 456\n'
        return (0, '') if args[1] == 'kill' else (0, rows)
    return run, calls


def test_memory_budget_cancels_owned_label_including_descendants():
    data = dict(label='org.timesplitters.workshop.fixture', uid=123)
    run, calls = commands('456 1 100 123\n600 456 100 123\n601 600 3000000 123\n')
    _, value = service.watch_once(data, {}, run)
    assert value['status'] == 'terminated' and value['reason'] == 'memory'
    assert calls[-1] == ['/bin/launchctl', 'kill', 'SIGKILL', 'gui/123/' + data['label']]
    assert '456' not in calls[-1]


def test_initial_controller_grace_tracks_actual_managed_identity():
    data = dict(label='org.timesplitters.workshop.fixture', uid=123)
    run, _ = commands()
    observed, value = service.watch_once(data, {}, run, lambda: 100)
    assert value['status'] == 'watching' and value['watched_pid'] == 456
    assert value['rss_bytes'] == 300 * 1024 and observed['start'] == 100
    observed, value = service.watch_once(data, dict(pid=999, start=0), run, lambda: 101)
    assert observed['start'] == 101 and value['supervision_policy'] == service.SUPERVISION_POLICY


@pytest.fixture
def data(tmp_path):
    if os.name != 'posix':
        pytest.skip('Private POSIX host files')
    tmp_path.chmod(0o700)
    for name in ('repository', 'state'):
        (tmp_path / name).mkdir(mode=0o700)
    value = dict(schema=1, repository=str(tmp_path / 'repository'), state=str(tmp_path / 'state'),
                 caddy=str(tmp_path / 'caddy'), trusted_root=str(tmp_path / 'root.json'),
                 catalog_keys=[str(tmp_path / 'keys/private.pem')], root_updates=[])
    path = tmp_path / 'publication-config.json'
    path.write_text(json.dumps(value))
    path.chmod(0o600)
    return service.config(path)


def test_agent_generation_is_private_new_only_and_keeps_selected_runtime(data, tmp_path):
    result = service.generate(data, tmp_path / 'bundle')
    assert result['watchdog_label'] == result['publisher_label'] + '.watchdog'
    for path in (tmp_path / 'bundle').iterdir():
        value = plistlib.loads(path.read_bytes())
        assert path.stat().st_mode & 0o077 == 0
        assert value['ProgramArguments'][0] == str(Path(sys.executable).absolute())
        assert value['KeepAlive'] and value['RunAtLoad'] and value['AbandonProcessGroup'] is False
        assert value['StandardOutPath'] == value['StandardErrorPath'] == '/dev/null'
        if value['Label'] == result['publisher_label']:
            assert value['ProgramArguments'][-1] == '--managed' and value['ThrottleInterval'] == 60
        else:
            assert value['ProgramArguments'][-1] == '--watchdog' and value['ThrottleInterval'] == 5
    with pytest.raises(ModError, match='new'):
        service.generate(data, tmp_path / 'bundle')


def test_admission_binds_policy_freshness_service_and_actual_controller_pid(data):
    with pytest.raises(OSError):
        service.healthy(data)
    value = dict(status='watching', watched_pid=os.getpid(), supervision_policy=service.SUPERVISION_POLICY)
    service.status(data, value, 'watchdog')
    service.healthy(data)
    path = data['state'] / 'watchdog.json'
    for field, bad in [('watched_pid', os.getpid() + 1), ('time', int(time.time()) - 30),
                       ('service', 'unrelated'), ('supervision_policy', 'legacy'), ('status', 'idle')]:
        original = path.read_bytes()
        changed = json.loads(original)
        changed[field] = bad
        path.write_text(json.dumps(changed))
        with pytest.raises(ModError, match='not healthy'):
            service.healthy(data)
        path.write_bytes(original)


def test_stalled_controller_is_cancelled_after_startup_grace(data):
    data['uid'] = 123
    run, calls = commands()
    service.status(data, {'status': 'serving'})
    path = data['state'] / 'publication.json'
    value = json.loads(path.read_bytes())
    value.update(pid=456, time=int(time.time()) - 30)
    path.write_text(json.dumps(value))
    _, report = service.watch_once(data, dict(pid=456, start=0), run, lambda: 16)
    assert report['reason'] == 'heartbeat' and calls[-1][1:3] == ['kill', 'SIGKILL']
    value['time'] = int(time.time())
    path.write_text(json.dumps(value))
    _, report = service.watch_once(data, dict(pid=456, start=0), run, lambda: 16)
    assert report['status'] == 'watching'


def test_unknown_status_is_preserved_and_fault_survives_health(data):
    path = data['state'] / 'publication.json'
    path.write_text('{"personal":"preserve"}')
    with pytest.raises(ModError, match='Preserving'):
        service.status(data, {'status': 'waiting'})
    assert json.loads(path.read_bytes()) == {'personal': 'preserve'}
    service.status(data, dict(status='terminated', reason='memory'), 'watchdog')
    service.status(data, dict(status='watching', watched_pid=os.getpid(),
                              supervision_policy=service.SUPERVISION_POLICY), 'watchdog')
    assert 'last_fault' in json.loads((data['state'] / 'watchdog.json').read_bytes())


def test_managed_start_waits_without_loading_keys_or_public_bytes(data, monkeypatch):
    import resource
    monkeypatch.setattr(resource, 'setrlimit', lambda *_: None)
    monkeypatch.setattr(service.time, 'monotonic', iter([0, 61]).__next__)
    monkeypatch.setattr(service, 'load_signer', lambda *_: pytest.fail('Unsupervised key admission'))
    service.serve(data, managed=True)
    assert list(data['repository'].iterdir()) == []
    assert not (data['state'] / 'publications').exists()
    assert json.loads((data['state'] / 'publication.json').read_bytes())['status'] == 'waiting'
