"""Public HTTPS must retain the signed local checkpoint, including revocations."""
import importlib.util
from pathlib import Path
import ssl
import sys
import time
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mods'))
spec=importlib.util.spec_from_file_location('public_readiness',ROOT/'tools/mods/hosting/accept-public-host-services-mac.py')
ready=importlib.util.module_from_spec(spec);spec.loader.exec_module(ready)
from mod_package import ModError
from mod_trust import PublicationSigner,encode,make_root


@pytest.fixture
def service(trust_keys,monkeypatch):
    now=int(time.time());origin='https://workshop.example.test:8443'
    pinned=make_root(trust_keys[:2],[trust_keys[4]],1,origin,now-60)
    signer=PublicationSigner(pinned,[trust_keys[4]])
    def payload(version=42,revoked=None,generated=now):
        catalog=encode(dict(schema=1,generated=generated,expires=generated+300,releases=[]))
        signed=signer.publish(catalog,version,['a'*64] if revoked is None else revoked,generated)['v1/trust/catalog']
        return {'/healthz':b'{"ready":true}','/v1/catalog':catalog,'/v1/trust/catalog':signed}
    local=payload();public=payload();calls=[]
    def loopback(path,authority):
        assert authority==origin
        return local[path]
    monkeypatch.setattr(ready,'helper',lambda name:SimpleNamespace(fetch=loopback)
                        if name=='accept-private-host-services-mac.py' else None)
    monkeypatch.setattr(ready,'fetch',lambda path,authority:public[path])
    monkeypatch.setattr(ready,'monitor',lambda:SimpleNamespace(
        scan=SimpleNamespace(root_json=lambda *_:dict(pid=456)),healthy=lambda *args:calls.append(args)))
    return Path('/release'),origin,pinned,local,public,calls,payload


@pytest.mark.parametrize('newer',[False,True])
def test_public_proof_verifies_signatures_and_retains_local_checkpoint(service,newer):
    release,origin,pinned,_,public,calls,payload=service
    if newer:public.update(payload(43,['a'*64,'b'*64]))
    report=ready.check(release,600,origin,pinned)
    assert report['public_endpoint_verified'] is True and report['revocations_preserved'] is True
    assert report['transport']=='public-https' and report['loopback']['catalog_version']==42
    assert report['public']['catalog_version']==(43 if newer else 42)
    assert report['public']['revocation_count']==(2 if newer else 1)
    assert calls==[(role,600,456,str(release)) for role in ready.ROLES]


@pytest.mark.parametrize('fault',['health','boolean','catalog','signature','origin','rollback',
                                 'reused-version','forgot-revocation','local-signature','role-health','expired'])
def test_public_or_local_invalid_evidence_never_passes(service,monkeypatch,fault):
    release,origin,pinned,local,public,_,payload=service
    if fault=='health':public['/healthz']=b'{"ready":false}'
    elif fault=='boolean':public['/healthz']=b'{"ready":1}'
    elif fault=='catalog':public['/v1/catalog']+=b' '
    elif fault=='signature':public['/v1/trust/catalog']=b'{}'
    elif fault=='origin':origin='https://other.example.test:8443'
    elif fault=='rollback':public.update(payload(41))
    elif fault=='reused-version':public.update(payload(42,['a'*64,'b'*64]))
    elif fault=='forgot-revocation':public.update(payload(43,[]))
    elif fault=='local-signature':local['/v1/trust/catalog']=b'{}'
    elif fault=='role-health':monkeypatch.setattr(ready,'monitor',lambda:SimpleNamespace(
        scan=SimpleNamespace(root_json=lambda *_:dict(pid=456)),healthy=lambda *_:(_ for _ in ()).throw(ModError('stale'))))
    elif fault=='expired':
        import mod_trust
        monkeypatch.setattr(mod_trust.time,'time',lambda:10**12)
    with pytest.raises(ModError):ready.check(release,600,origin,pinned)


@pytest.mark.parametrize('status,size',[(200,2),(302,2),(200,8193),(200,0)])
def test_https_transport_uses_verified_tls_fixed_paths_and_bounded_no_credentials(monkeypatch,status,size):
    closed=[]
    class Connection:
        def __init__(self,host,port,timeout,context):
            assert (host,port,timeout)==('workshop.example.test',8443,3)
            assert context.check_hostname and context.verify_mode==ssl.CERT_REQUIRED
        def request(self,method,path,headers):
            assert (method,path,headers)==('GET','/healthz',{'Connection':'close'})
        def getresponse(self):return SimpleNamespace(status=status,read=lambda maximum:b'x'*min(size,maximum))
        def close(self):closed.append(True)
    monkeypatch.setattr(ready.http.client,'HTTPSConnection',Connection)
    if status==200 and 0<size<=8192:assert ready.fetch('/healthz','https://workshop.example.test:8443')==b'xx'
    else:
        with pytest.raises(ModError):ready.fetch('/healthz','https://workshop.example.test:8443')
    assert closed==[True]
    with pytest.raises(ModError):ready.fetch('/private/anything','https://workshop.example.test:8443')
    with pytest.raises(ModError):ready.fetch('/healthz','http://workshop.example.test:8443')


def test_certificate_failure_is_closed_and_wait_is_bounded(monkeypatch):
    closed=[]
    class Connection:
        def __init__(self,*args,**kwargs):pass
        def request(self,*args,**kwargs):raise ssl.SSLCertVerificationError('fixture')
        def close(self):closed.append(True)
    monkeypatch.setattr(ready.http.client,'HTTPSConnection',Connection)
    with pytest.raises(ssl.SSLCertVerificationError):ready.fetch('/healthz','https://workshop.example.test:8443')
    assert closed==[True]
    monkeypatch.setattr(ready,'check',lambda *_:(_ for _ in ()).throw(ssl.SSLCertVerificationError('fixture')))
    with pytest.raises(ModError,match='timed out'):
        ready.wait_ready(Path('/release'),600,'origin',b'pin',seconds=1,clock=iter([0,2]).__next__,sleep=lambda _:None)
