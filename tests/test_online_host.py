"""Automatic hosting uses real HTTPS and sockets, without personal game data."""
from pathlib import Path
import socket
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import online_host
from online_client import Client, Session

VERSION = 'a'*64


def unused_udp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


@pytest.mark.parametrize('mode', ['arcade', 'story'])
def test_auto_host_tls_invitation_join_and_cleanup(tmp_path, mode):
    host = online_host.start_match(tmp_path, '203.0.113.7', VERSION, 'Automatic host', unused_udp_port(), True,
                                  directory_port=0, mode=mode)
    directory = host.local_directory
    guest = None
    try:
        url, fingerprint, code = online_host.parse_invitation(host.invitation)
        assert url == directory.public_url and fingerprint == directory.fingerprint and code == host.lobby['code']
        assert host.lobby['address'] == '203.0.113.7'
        client = Client(directory.address, fingerprint)
        assert client.request('GET', '/v2/lobbies?compatibility='+VERSION+'&mode='+mode)['lobbies'][0]['code'] == code
        assert client.request('GET', '/v2/lobbies?compatibility='+'b'*64)['lobbies'] == []
        with pytest.raises(ValueError, match='certificate does not match'):
            Client(directory.address, '0'*64).request('GET', '/v2/lobbies?compatibility='+VERSION)
        # Simulate the forwarded host endpoint on loopback for the real UDP handshake.
        with directory.server.directory.lock:
            directory.server.directory.online[code]['address'] = '127.0.0.1'
        guest = Session.join(client, VERSION, code, mode=mode)
        assert guest.bridge.ready.is_set()
        with pytest.raises(OSError):
            online_host.LocalDirectory(tmp_path, '203.0.113.7', directory.server.server_port)
    finally:
        if guest:
            guest.close()
        host.close()
    assert not directory.server.directory.online
    assert not directory.thread
    with online_host.Server(('0.0.0.0', directory.server.server_port)):
        pass  # The port was released and can be used by the next match.
    replacement = online_host.LocalDirectory(tmp_path, '203.0.113.7', 0)
    try:
        assert replacement.fingerprint == fingerprint  # Persistent certificate is reused.
    finally:
        replacement.close()


@pytest.mark.parametrize('value', ['bad', 'tsfp-online:@@@', 'tsfp-online:e30', 'tsfp-online:'+('x'*2048),
    online_host.invitation('http://127.0.0.1:30820', 'a'*64, 'A'*16),
    online_host.invitation('https://example.test:30820', '', 'A'*16),
    online_host.invitation('https://example.test:30820', 'a'*64, 'not a code')])
def test_invalid_invitations(value):
    with pytest.raises(ValueError):
        online_host.parse_invitation(value)


def test_public_ip_lookup_and_failure(monkeypatch, tmp_path):
    real_connection = online_host.http.client.HTTPSConnection
    class Response:
        status = 200
        data = b'{"ip":"8.8.8.8"}'
        def read(self, limit):
            assert limit == 1025
            return self.data

    class Connection:
        closed = False
        def __init__(self, host, timeout):
            assert host == 'api.ipify.org' and timeout == 5
        def request(self, method, path):
            assert method == 'GET' and path == '/?format=json'
        def getresponse(self):
            return Response()
        def close(self):
            Connection.closed = True

    monkeypatch.setattr(online_host.http.client, 'HTTPSConnection', Connection)
    assert online_host.fetch_public_ipv4() == '8.8.8.8'
    assert Connection.closed
    for data in (b'{"ip":"127.0.0.1"}', b'{"ip":"::1"}', b'bad JSON', b'x'*1025, b'{}'):
        Response.data = data
        with pytest.raises(ValueError, match='Could not detect'):
            online_host.fetch_public_ipv4()
    Response.data = b'{"ip":"8.8.8.8"}'
    monkeypatch.setattr(online_host.http.client, 'HTTPSConnection', real_connection)
    # Automatic start consumes the freshly detected address, without manual settings.
    monkeypatch.setattr(online_host, 'fetch_public_ipv4', lambda: '203.0.113.8')
    host = online_host.start_match(tmp_path, None, VERSION, 'Auto IP', unused_udp_port(), False, directory_port=0)
    try:
        assert host.lobby['address'] == '203.0.113.8'
    finally:
        host.close()
