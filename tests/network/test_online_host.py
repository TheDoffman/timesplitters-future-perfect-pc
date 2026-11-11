"""Player-side invitation and public-address validation."""
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
import online_host


@pytest.mark.parametrize('value', ['bad', 'tsfp-online:@@@', 'tsfp-online:e30', 'tsfp-online:'+('x'*2048),
    online_host.invitation('http://127.0.0.1:30820', 'a'*64, 'A'*16),
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
