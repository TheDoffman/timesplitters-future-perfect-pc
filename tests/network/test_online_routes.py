"""Same-network directory routing retains canonical invitations and pinned TLS."""
import hashlib,json
from pathlib import Path
import pytest
import online_paths
import online_client
from online_client import Client,DirectoryError

URL='https://community.example:41820'
CERT=b'test service certificate'
PIN=hashlib.sha256(CERT).hexdigest()

@pytest.fixture
def routes(tmp_path,monkeypatch):
    monkeypatch.setenv('TSFP_ONLINE_SETTINGS',str(tmp_path/'pc-online.json'))
    path=tmp_path/'pc-online-routes.json'
    def write(**changes):
        row=dict(directory=URL,fingerprint=PIN,local_address='192.168.1.27');row.update(changes)
        path.write_text(json.dumps(dict(routes=[row])),encoding='utf-8')
    write();return path,write

@pytest.mark.parametrize('changes',[{},dict(local_address='10.0.0.27'),dict(local_address='172.16.0.27')])
def test_route_requires_matching_public_identity_and_numeric_lan(routes,changes):
    path,write=routes;write(**changes)
    assert online_paths.directory_route(URL,PIN)==changes.get('local_address','192.168.1.27')
    assert online_paths.directory_route(URL,'a'*64) is None
    assert online_paths.directory_route('https://other.example:41820',PIN) is None
    assert online_paths.directory_route(URL,'') is None
    assert online_paths.directory_route('http://127.0.0.1:41820',PIN) is None

@pytest.mark.parametrize('value',['127.0.0.1','8.8.8.8','169.254.1.2','192.168.1.27:90','local.example','::1',None,[]])
def test_invalid_route_cannot_redirect_connection(routes,value):
    path,write=routes;write(local_address=value)
    assert online_paths.directory_route(URL,PIN) is None

@pytest.mark.parametrize('raw',['bad json','[]','{}','{"routes":{}}','x'*16385,json.dumps(dict(routes=[{}]*5))])
def test_missing_or_malformed_personal_routes_preserve_public_default(routes,raw):
    path,_=routes;path.write_text(raw,encoding='utf-8')
    assert Client(URL,PIN).local_address is None
    path.unlink();assert Client(URL,PIN).local_address is None

@pytest.fixture
def connections(monkeypatch):
    calls=[];behavior={}
    class Socket:
        def __init__(self,host):self.host=host
        def getpeercert(self,binary_form):return behavior.get(self.host,{}).get('cert',CERT)
        def settimeout(self,value):pass
    class Response:
        status=200;will_close=False
        def __init__(self,host):self.status=behavior.get(host,{}).get('status',200)
        def read(self,limit):return b'{"error":"Admission denied","lobbies":[]}'
    class Connection:
        def __init__(self,host,port,timeout,context):
            self.host=host;self.sock=None;self.closed=False;calls.append(('construct',host,port,timeout))
        def connect(self):
            if behavior.get(self.host,{}).get('offline'):raise TimeoutError('unreachable')
            self.sock=Socket(self.host)
        def request(self,method,path,body,headers):calls.append(('request',self.host,method,path,headers))
        def getresponse(self):return Response(self.host)
        def close(self):self.closed=True;self.sock=None;calls.append(('close',self.host))
    monkeypatch.setattr(online_client.http.client,'HTTPSConnection',Connection)
    return calls,behavior

def test_pinned_local_route_keeps_public_url_and_reuses_verified_connection(routes,connections):
    calls,behavior=connections;client=Client(URL,PIN)
    client.request('POST','/v3/lobbies',dict(name='Room'),token='private',persistent=True)
    client.request('PUT','/v3/lobbies/example',token='private',persistent=True)
    assert client.url.geturl()==URL
    assert len([c for c in calls if c[0]=='construct'])==1
    assert [c[1] for c in calls if c[0]=='request']==['192.168.1.27']*2
    client.close_persistent()

@pytest.mark.parametrize('failure',[dict(offline=True),dict(cert=b'wrong service')])
def test_local_failure_falls_back_before_credentials_are_sent(routes,connections,failure):
    calls,behavior=connections;behavior['192.168.1.27']=failure
    client=Client(URL,PIN);client.request('POST','/v3/lobbies',token='private')
    assert [c[1] for c in calls if c[0]=='request']==['community.example']
    assert [c[3] for c in calls if c[0]=='construct']==[1,5]
    assert ('close','192.168.1.27') in calls

def test_wrong_certificate_on_all_routes_never_sends_credentials(routes,connections):
    calls,behavior=connections
    for host in ('192.168.1.27','community.example'):behavior[host]=dict(cert=b'wrong')
    with pytest.raises(ValueError,match='certificate'):Client(URL,PIN).request('POST','/v3/lobbies',token='private')
    assert not any(c[0]=='request' for c in calls)

def test_authenticated_http_error_never_replays_mutation_on_public_route(routes,connections):
    calls,behavior=connections;behavior['192.168.1.27']=dict(status=403)
    with pytest.raises(DirectoryError) as error:Client(URL,PIN).request('POST','/v3/lobbies',token='private')
    assert error.value.status==403
    assert [c[1] for c in calls if c[0]=='construct']==['192.168.1.27']
