"""Private readiness requires signed bytes, actual protected role health and bounds."""
import importlib.util
import json
from pathlib import Path
import sys
import time
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mods'))
spec=importlib.util.spec_from_file_location('private_readiness',ROOT/'tools/mods/hosting/accept-private-host-services-mac.py')
ready=importlib.util.module_from_spec(spec);spec.loader.exec_module(ready)
from mod_package import ModError
from mod_trust import PublicationSigner,encode,make_root


@pytest.fixture
def service(trust_keys,monkeypatch):
    now=int(time.time());origin='https://workshop.example.test:8443'
    pinned=make_root(trust_keys[:2],[trust_keys[4]],1,origin,now-60)
    catalog=encode(dict(schema=1,generated=now,expires=now+300,releases=[]))
    signed=PublicationSigner(pinned,[trust_keys[4]]).publish(catalog,1,[],now)['v1/trust/catalog']
    payloads={'/healthz':b'{"ready":true}','/v1/catalog':catalog,'/v1/trust/catalog':signed}
    calls=[]
    def fetch(path,authority):
        assert authority==origin
        return payloads[path]
    monkeypatch.setattr(ready,'fetch',fetch)
    monkeypatch.setattr(ready,'monitor',lambda:SimpleNamespace(
        scan=SimpleNamespace(root_json=lambda *_:dict(pid=456)),healthy=lambda *args:calls.append(args)))
    release=Path('/release')
    return release,origin,pinned,payloads,calls


def test_readiness_verifies_signed_catalog_and_all_fixed_roles(service):
    release,origin,pinned,_,calls=service
    result=ready.check(release,600,origin,pinned)
    assert result['phase']=='private-services-verified' and result['catalog_version']==1 and result['release_count']==0
    assert result['role_health_verified'] is True and result['public_endpoint_verified'] is False
    assert result['transport']=='private-loopback-http'
    assert calls==[(role,600,456,str(release)) for role in ready.ROLES]


@pytest.mark.parametrize('fault',['health','boolean','catalog','signature','origin','root-health'])
def test_bad_api_catalog_signature_or_role_health_is_never_admitted(service,monkeypatch,fault):
    release,origin,pinned,payloads,_=service
    if fault=='health':payloads['/healthz']=b'{"ready":false}'
    if fault=='boolean':payloads['/healthz']=b'{"ready":1}'
    if fault=='catalog':payloads['/v1/catalog']+=b' '
    if fault=='signature':payloads['/v1/trust/catalog']=b'{"payload":"e30=","signatures":[]}'
    if fault=='origin':origin='https://other.example.test'
    if fault=='root-health':monkeypatch.setattr(ready,'monitor',lambda:SimpleNamespace(
        scan=SimpleNamespace(root_json=lambda *_:dict(pid=456)),healthy=lambda *_:(_ for _ in ()).throw(ModError('stale'))))
    with pytest.raises(ModError):ready.check(release,600,origin,pinned)


@pytest.mark.parametrize('status,size',[(200,2),(302,2),(200,8193)])
def test_transport_is_fixed_loopback_bounded_and_sends_no_credentials(monkeypatch,status,size):
    class Connection:
        def __init__(self,host,port,timeout):assert (host,port,timeout)==('127.0.0.1',8768,3)
        def request(self,method,path,headers):
            assert method=='GET' and path=='/healthz'
            assert headers=={'Host':'workshop.example.test:8443','Connection':'close'}
        def getresponse(self):return SimpleNamespace(status=status,read=lambda maximum:b'x'*min(size,maximum))
        def close(self):pass
    monkeypatch.setattr(ready.http.client,'HTTPConnection',Connection)
    if status==200 and size<=8192:assert ready.fetch('/healthz','https://workshop.example.test:8443')==b'xx'
    else:
        with pytest.raises(ModError):ready.fetch('/healthz','https://workshop.example.test:8443')
    with pytest.raises(ModError):ready.fetch('/private/anything','https://workshop.example.test:8443')


def test_readiness_wait_is_bounded_and_withholds_partial_success(monkeypatch):
    monkeypatch.setattr(ready,'check',lambda *_:(_ for _ in ()).throw(ModError('not ready')))
    clock=iter([0,2]).__next__
    with pytest.raises(ModError,match='timed out'):ready.wait_ready(Path('/release'),600,'origin',b'pin',seconds=1,clock=clock,sleep=lambda _:None)
