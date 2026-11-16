"""Locked-account loopback/readiness proof against the immutable public pin."""
import hashlib
import http.client
import importlib.util
import json
from pathlib import Path
import sys
import time
from urllib.parse import urlsplit

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from mod_package import ModError,read_json,require
from mod_system_host import ROLES,monitor,paths
from mod_trust import trusted_root,verify_catalog

LIMITS={'/healthz':8192,'/v1/catalog':4*1024**2,'/v1/trust/catalog':2*1024**2}


def helper(name):
    require(name in ('system-host-service-mac.py','prepare-launcher-workshop-profile.py'),'Invalid readiness helper')
    spec=importlib.util.spec_from_file_location('ready_'+name.replace('.','_'),Path(__file__).with_name(name))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def fetch(path,origin):
    require(path in LIMITS,'Invalid fixed readiness route')
    connection=http.client.HTTPConnection('127.0.0.1',8768,timeout=3)
    try:
        connection.request('GET',path,headers={'Host':urlsplit(origin).netloc,'Connection':'close'})
        response=connection.getresponse()
        require(response.status==200,'Private gateway is not ready')
        body=response.read(LIMITS[path]+1)
        require(0<len(body)<=LIMITS[path],'Private readiness response exceeds budget')
        return body
    finally:connection.close()


def check(release,uid,origin,pinned):
    authority=trusted_root(pinned)
    require(authority['origin']==origin,'Private readiness root origin differs')
    health=read_json(fetch('/healthz',origin))
    require(isinstance(health,dict) and set(health)=={'ready'} and health['ready'] is True,'Private API is not ready')
    signed=fetch('/v1/trust/catalog',origin);catalog=fetch('/v1/catalog',origin)
    accepted,state=verify_catalog(signed,catalog,authority)
    guardian=monitor()
    for role in ROLES:
        value=guardian.scan.root_json(paths(role)['monitor'],'health.json')
        guardian.healthy(role,uid,value.get('pid'),str(release))
    return dict(schema=1,phase='private-services-verified',release=release.name,uid=uid,origin=origin,
                trusted_root_sha256=hashlib.sha256(pinned).hexdigest(),catalog_version=state['version'],
                release_count=len(accepted['releases']),role_health_verified=True,
                transport='private-loopback-http',public_endpoint_verified=False)


def wait_ready(release,uid,origin,pinned,seconds=90,clock=time.monotonic,sleep=time.sleep):
    deadline=clock()+seconds
    while True:
        try:return check(release,uid,origin,pinned)
        except (OSError,ValueError,ModError,http.client.HTTPException):
            require(clock()<deadline,'Private service readiness timed out')
            sleep(.5)


def main():
    try:
        service=helper('system-host-service-mac.py')
        release=service.installed_release()
        require(Path(__file__).absolute()==release/'source/tools/mods/hosting/accept-private-host-services-mac.py',
                'Use the immutable installed private readiness checker')
        profile,pinned=helper('prepare-launcher-workshop-profile.py').validate()
        import os
        print(json.dumps(wait_ready(release,os.geteuid(),profile['origin'],pinned),indent=2));return 0
    except (OSError,ValueError,ModError,http.client.HTTPException) as error:
        print('Private host readiness: '+str(error)[:2000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
