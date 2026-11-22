"""Player-side invitation and public-address validation."""
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
import online_host


def test_local_hints_do_not_wait_for_hostname_dns(monkeypatch):
    import threading
    entered=threading.Event();release=threading.Event();finished=threading.Event()
    calls=[];result=[]
    class Route:
        def __enter__(self):return self
        def __exit__(self,*_):pass
        def connect(self,address):assert address==('192.0.2.1',9)
        def getsockname(self):return ('192.168.1.27',40000)
    def stalled(*args):
        calls.append(args);entered.set();release.wait(5)
        return [(None,None,None,None,('10.0.0.27',0))]
    monkeypatch.setattr(online_host.socket,'socket',lambda *_:Route())
    monkeypatch.setattr(online_host.socket,'getaddrinfo',stalled)
    def discover():
        try:result.append(online_host.local_ipv4_addresses('192.0.2.1'))
        finally:finished.set()
    worker=threading.Thread(target=discover,daemon=True);worker.start()
    try:
        assert entered.wait(1)
        assert finished.wait(1),'Optional hostname DNS blocked local hosting'
        assert result==[('192.168.1.27',)]
        # A stalled resolver must not create another thread on every retry.
        assert online_host.local_ipv4_addresses('192.0.2.1')==('192.168.1.27',)
        assert len(calls)==1
    finally:
        release.set();worker.join(timeout=2)
        with online_host._hint_lookup_lock:pass


def test_local_hints_keep_fast_secondary_interfaces(monkeypatch):
    class Route:
        def __enter__(self):return self
        def __exit__(self,*_):pass
        def connect(self,_):pass
        def getsockname(self):return ('192.168.1.27',40000)
    monkeypatch.setattr(online_host.socket,'socket',lambda *_:Route())
    monkeypatch.setattr(online_host.socket,'getaddrinfo',lambda *_:[
        (None,None,None,None,(address,0)) for address in
        ('192.168.1.27','127.0.0.1','10.0.0.27','8.8.8.8')])
    assert online_host.local_ipv4_addresses('192.0.2.1')==('192.168.1.27','10.0.0.27')


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
