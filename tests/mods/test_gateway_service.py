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


@pytest.mark.skipif(sys.platform != 'darwin', reason='Actual confined Mac Caddy transport')
@pytest.mark.parametrize('suffix', ['', ':8443', ':10000'])
def test_actual_gateway_bounds_http_framing_before_completed_backend_requests(tmp_path, suffix):
    import hashlib
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
    import shutil
    import socket
    import subprocess
    import threading
    import time

    caddy = shutil.which('caddy')
    if caddy is None:
        pytest.skip('Install Caddy to run the native gateway transport check')
    subprocess.run([sys.executable, str(ROOT / 'tools/build/build-scan-guardian.py')],
                   capture_output=True, check=True, timeout=90)
    tmp_path.chmod(0o700)
    completed = []

    class Backend(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass

        def do_POST(self):
            self.connection.settimeout(3)
            try:
                if self.headers.get('Transfer-Encoding', '').lower() == 'chunked':
                    chunks = []
                    while True:
                        size = int(self.rfile.readline(128).split(b';')[0], 16)
                        if not size:
                            assert self.rfile.readline(128) == b'\r\n'
                            break
                        assert size <= 32768
                        part = self.rfile.read(size)
                        assert len(part) == size and self.rfile.read(2) == b'\r\n'
                        chunks.append(part)
                    body = b''.join(chunks)
                else:
                    size = int(self.headers.get('Content-Length', '0'))
                    assert size <= 32768
                    body = self.rfile.read(size)
                    assert len(body) == size
                completed.append(dict(size=len(body), sha256=hashlib.sha256(body).hexdigest(),
                                      length=self.headers.get('Content-Length'),
                                      encoding=self.headers.get('Transfer-Encoding')))
                self.send_response(200)
                self.send_header('Content-Length', '0')
                self.end_headers()
            except (OSError, ValueError, AssertionError):
                self.close_connection = True

    backend = ThreadingHTTPServer(('127.0.0.1', 8767), Backend)
    backend.daemon_threads = True
    thread = threading.Thread(target=backend.serve_forever, daemon=True)
    thread.start()
    server = None

    def request(headers, body=b''):
        wire = (b'POST /v1/sessions HTTP/1.1\r\nHost: ' + ('workshop.example.test' + suffix).encode('ascii') +
                b'\r\nConnection: close\r\n' + headers + b'\r\n' + body)
        with socket.create_connection(('127.0.0.1', 8768), timeout=3) as connection:
            connection.settimeout(3)
            connection.sendall(wire)
            reply = bytearray()
            while True:
                try:
                    part = connection.recv(4096)
                except ConnectionResetError:
                    break
                if not part:
                    break
                reply.extend(part)
                assert len(reply) < 65536
        return int(reply.split(b' ', 2)[1])

    try:
        server = service.CaddyServer(caddy, tmp_path, role='gateway', origin='https://workshop.example.test' + suffix)
        server.start(None, 60)
        deadline = time.monotonic() + 5
        while True:
            try:
                assert request(b'Content-Length: 0\r\n') == 200
                break
            except ConnectionRefusedError:
                assert server.running() and time.monotonic() < deadline
                time.sleep(.05)
        completed.clear()
        for headers, body, status in [
            (b'Content-Length: 4\r\nContent-Length: 5\r\n', b'12345', 400),
            (b'Transfer-Encoding: nonsense\r\n', b'', 501),
            (b'X-Filler: ' + b'x' * 32768 + b'\r\nContent-Length: 0\r\n', b'', 431),
            (b'Content-Length: 16385\r\n', b'x' * 16385, 413),
            (b'Transfer-Encoding: chunked\r\n', b'4001\r\n' + b'x' * 16385 + b'\r\n0\r\n\r\n', 413),
        ]:
            assert request(headers, body) == status
        assert request(b'Transfer-Encoding: chunked\r\n',
                       b'4000\r\n' + b'x' * 16384 + b'\r\n0\r\n\r\n') == 200
        assert request(b'Content-Length: 999\r\nTransfer-Encoding: chunked\r\n',
                       b'4\r\n1234\r\n0\r\n\r\n') == 200
        assert server.running()
    finally:
        if server is not None:
            server.stop()
        backend.shutdown()
        backend.server_close()
        thread.join(timeout=3)
    assert completed == [dict(size=len(body), sha256=hashlib.sha256(body).hexdigest(),
                              length=None, encoding='chunked') for body in (b'x' * 16384, b'1234')]
    assert not list((tmp_path / 'caddy-runtime/units').iterdir())
