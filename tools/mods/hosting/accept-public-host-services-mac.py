"""Locked-account HTTPS proof against the installed root and live local catalog."""
import hashlib
import http.client
import importlib.util
import json
import os
from pathlib import Path
import ssl
import sys
import time
from urllib.parse import urlsplit

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from mod_package import ModError,read_json,require
from mod_system_host import ROLES,monitor,paths
from mod_trust import encode,origin as checked_origin,trusted_root,verify_catalog

LIMITS={'/healthz':8192,'/v1/catalog':4*1024**2,'/v1/trust/catalog':2*1024**2}


def helper(name):
    require(name in ('system-host-service-mac.py','prepare-launcher-workshop-profile.py',
                     'accept-private-host-services-mac.py'),'Invalid public acceptance helper')
    spec=importlib.util.spec_from_file_location('public_ready_'+name.replace('.','_'),Path(__file__).with_name(name))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def fetch(path,origin):
    require(path in LIMITS,'Invalid fixed public acceptance route')
    address=urlsplit(checked_origin(origin))
    # A direct connection uses system CA/hostname verification, no proxy env,
    # redirects, cookies, credentials, or downloaded replacement trust roots.
    connection=http.client.HTTPSConnection(address.hostname,address.port or 443,
        timeout=3,context=ssl.create_default_context())
    try:
        connection.request('GET',path,headers={'Connection':'close'})
        response=connection.getresponse()
        require(response.status==200,'Public gateway is not ready')
        body=response.read(LIMITS[path]+1)
        require(0<len(body)<=LIMITS[path],'Public acceptance response exceeds budget')
        return body
    finally:connection.close()


def signed_read(fetcher,origin,authority,previous=None):
    health=read_json(fetcher('/healthz',origin))
    require(isinstance(health,dict) and set(health)=={'ready'} and health['ready'] is True,
            'Workshop API is not ready')
    signed=fetcher('/v1/trust/catalog',origin)
    catalog=fetcher('/v1/catalog',origin)
    return verify_catalog(signed,catalog,authority,previous=previous)


def checkpoint_summary(state):
    return dict(catalog_version=state['version'],metadata_sha256=state['metadata_sha256'],
        revocations_sha256=hashlib.sha256(encode(state['revoked'])).hexdigest(),revocation_count=len(state['revoked']))


def check(release,uid,origin,pinned):
    authority=trusted_root(pinned)
    require(authority['origin']==origin,'Public acceptance root origin differs')
    # The live loopback signature is the checkpoint, never a fresh empty trust
    # state. Public metadata must retain every local revocation and cannot reuse
    # its version for different bytes. No private keys/configuration are read.
    local=helper('accept-private-host-services-mac.py')
    _,baseline=signed_read(local.fetch,origin,authority)
    accepted,state=signed_read(fetch,origin,authority,previous=baseline)
    guardian=monitor()
    for role in ROLES:
        value=guardian.scan.root_json(paths(role)['monitor'],'health.json')
        guardian.healthy(role,uid,value.get('pid'),str(release))
    return dict(schema=1,phase='public-services-verified',release=release.name,uid=uid,origin=origin,
        trusted_root_sha256=hashlib.sha256(pinned).hexdigest(),loopback=checkpoint_summary(baseline),
        public=checkpoint_summary(state),release_count=len(accepted['releases']),revocations_preserved=True,
        role_health_verified=True,transport='public-https',public_endpoint_verified=True)


def wait_ready(release,uid,origin,pinned,seconds=90,clock=time.monotonic,sleep=time.sleep):
    deadline=clock()+seconds
    while True:
        try:return check(release,uid,origin,pinned)
        except (OSError,ValueError,ModError,http.client.HTTPException):
            require(clock()<deadline,'Public service acceptance timed out')
            sleep(.5)


def main():
    try:
        release=helper('system-host-service-mac.py').installed_release()
        require(Path(__file__).absolute()==release/'source/tools/mods/hosting/accept-public-host-services-mac.py',
                'Use the immutable installed public acceptance checker')
        profile,pinned=helper('prepare-launcher-workshop-profile.py').validate()
        print(json.dumps(wait_ready(release,os.geteuid(),profile['origin'],pinned),indent=2));return 0
    except (OSError,ValueError,ModError,http.client.HTTPException) as error:
        print('Public host acceptance: '+str(error)[:2000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
