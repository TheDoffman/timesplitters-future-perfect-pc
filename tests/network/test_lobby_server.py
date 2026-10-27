

import importlib.util
import json
from pathlib import Path
import threading
import subprocess
import sys
from urllib.error import HTTPError
from urllib.request import Request, urlopen

spec = importlib.util.spec_from_file_location('lobby_server', Path(__file__).resolve().parents[2] / 'tools/network/lobby_server.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
V1, V2 = '1' * 64, '2' * 64


def test_directory_over_http():
    now = [0.0]
    directory = module.Directory(lambda: now[0])
    with module.Server(('127.0.0.1', 0), directory) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()

        def request(path, method='GET', data=None, token=None):
            req = Request(f'http://127.0.0.1:{server.server_port}{path}', method=method,
                          data=json.dumps(data).encode() if data is not None else None)
            if token:
                req.add_header('Authorization', 'Bearer ' + token)
            try:
                response = urlopen(req, timeout=3)
            except HTTPError as error:
                response = error
            with response:
                return response.status, json.load(response)

        def create(version, public=True):
            status, lobby = request('/v1/lobbies', 'POST', dict(
                compatibility=version, name='Test lobby', port=31000, public=public))
            assert status == 201
            return lobby

        try:
            first, other, private = create(V1), create(V2), create(V1, False)
            status, listing = request('/v1/lobbies?compatibility=' + V1)
            assert status == 200
            assert [row['code'] for row in listing['lobbies']] == [first['code']]
            assert 'owner_token' not in listing['lobbies'][0]
            path = '/v1/lobbies/' + first['code']
            assert request(path + '?compatibility=' + V2)[0] == 409
            assert request(path)[0] == 400
            assert request('/v1/lobbies/' + private['code'] + '?compatibility=' + V1)[0] == 200
            assert request(path, 'DELETE', token=other['owner_token'])[0] == 403
            update = dict(compatibility=V1, players=2, capacity=2)
            assert request(path, 'PUT', update, first['owner_token'])[0] == 200
            assert request(path + '?compatibility=' + V1)[0] == 409
            update['compatibility'] = V2
            assert request(path, 'PUT', update, first['owner_token'])[0] == 409
            now[0] = module.TTL + 1
            assert request(path + '?compatibility=' + V1)[0] == 404
            assert request('/v1/lobbies?compatibility=' + V1)[1] == {'lobbies': []}
            assert request('/v1/lobbies', 'POST', {'compatibility': 'legacy'})[0] == 400
        finally:
            server.shutdown()
            thread.join(timeout=3)


def test_heartbeat_extends_lifetime_and_owner_can_close():
    now = [0]
    directory = module.Directory(lambda: now[0])
    _, lobby = directory.request('POST', '/v1/lobbies', {}, dict(
        compatibility=V1, name='Friends', port=31000, public=False), '127.0.0.1', '')
    path = '/v1/lobbies/' + lobby['code']
    now[0] = 60
    directory.request('PUT', path, {}, dict(compatibility=V1, players=2, capacity=4),
                      '127.0.0.1', lobby['owner_token'])
    now[0] = 100
    assert directory.request('GET', path, {'compatibility': [V1]}, {}, '127.0.0.2', '')[0] == 200
    assert directory.request('DELETE', path, {}, {}, '127.0.0.1', lobby['owner_token'])[0] == 200
    assert not directory.lobbies


def test_public_listener_requires_tls():
    result = subprocess.run([sys.executable, str(Path(module.__file__)), '--bind', '0.0.0.0'],
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 2
    assert 'requires --cert and --key' in result.stderr
