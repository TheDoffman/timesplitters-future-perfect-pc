"""Gateway role/origin boundaries and shared supervision; no public endpoints."""
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
from mod_package import ModError
from mod_public_sandbox import profile
spec = importlib.util.spec_from_file_location('gateway_service', ROOT / 'tools/mods/hosting/gateway-service-mac.py')
service = importlib.util.module_from_spec(spec)
spec.loader.exec_module(service)


@pytest.mark.parametrize('role,snapshot', [('gateway', Path('/private/forbidden')), ('downloads', None), ('arbitrary', None)])
def test_gateway_cannot_receive_a_publication_or_private_repository_tree(role, snapshot):
    with pytest.raises(ModError, match='role/tree'):
        profile(Path('/caddy'), snapshot, Path('/config'), Path('/work'), role)


@pytest.fixture
def data(tmp_path):
    if os.name != 'posix':
        pytest.skip('Private Unix service configuration')
    tmp_path.chmod(0o700)
    state = tmp_path / 'state'
    state.mkdir(mode=0o700)
    path = tmp_path / 'gateway.json'
    path.write_text(json.dumps(dict(schema=1, origin='https://workshop.example.test', state=str(state),
                                   caddy=str(tmp_path / 'caddy'))))
    path.chmod(0o600)
    return service.config(path)


@pytest.mark.parametrize('origin', ['http://workshop.example.test', 'https://workshop.example.test/path',
                                    'https://workshop.example.test:443', 'https://WORKSHOP.example.test',
                                    'https://workshop.example.test\nimport bad'])
def test_origin_rejects_proxy_configuration_injection_and_noncanonical_authorities(data, origin):
    path = data['config']
    value = json.loads(path.read_bytes())
    value['origin'] = origin
    path.write_text(json.dumps(value))
    with pytest.raises(ModError):
        service.config(path)


def test_gateway_config_rejects_private_repository_or_key_settings(data):
    path = data['config']
    value = json.loads(path.read_bytes())
    value['repository'] = '/private/repository'
    path.write_text(json.dumps(value))
    with pytest.raises(ModError):
        service.config(path)


def test_gateway_agents_keep_origin_bound_health_and_correct_controller_source(data, tmp_path):
    watch = service.supervisor(data)
    result = watch.generate(ROOT / 'tools/mods/hosting/gateway-service-mac.py', tmp_path / 'agents')
    assert result['controller_label'].startswith('org.timesplitters.workshop.gateway.')
    for path in (tmp_path / 'agents').iterdir():
        value = plistlib.loads(path.read_bytes())
        assert value['ProgramArguments'][0] == str(Path(sys.executable).absolute())
        assert value['ProgramArguments'][1].endswith('/hosting/gateway-service-mac.py')
        assert value['KeepAlive'] and path.stat().st_mode & 0o077 == 0
    watch.status(dict(status='watching', watched_pid=os.getpid(), supervision_policy=service.POLICY), 'watchdog')
    watch.healthy()
    path = data['state'] / 'watchdog.json'
    value = json.loads(path.read_bytes())
    value['origin'] = 'https://unrelated.example.test'
    path.write_text(json.dumps(value))
    with pytest.raises(ModError, match='not healthy'):
        watch.healthy()


def test_waiting_gateway_does_not_construct_server_before_managed_health(data, monkeypatch):
    import resource
    monkeypatch.setattr(resource, 'setrlimit', lambda *_: None)
    monkeypatch.setattr(service.time, 'monotonic', iter([0, 61]).__next__)
    monkeypatch.setattr(service, 'CaddyServer', lambda *_a, **_k: pytest.fail('Unsupervised gateway started'))
    service.serve(data, managed=True)
    assert json.loads((data['state'] / 'gateway.json').read_bytes())['status'] == 'waiting'
    assert not (data['state'] / 'caddy-runtime').exists()
