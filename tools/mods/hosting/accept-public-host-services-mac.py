"""Locked-account HTTPS proof against the installed root and live local catalog."""
import argparse
import hashlib
import http.client
import importlib.util
import json
import os
from pathlib import Path
import re
import ssl
import sys
import time
from urllib.parse import urlsplit

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from mod_package import ModError,read_json,require
from mod_host_identity import require_locked_process
from mod_system_host import ROOT,ROLES,monitor,paths
from mod_trust import encode,origin as checked_origin,trusted_root,verify_catalog

LIMITS={'/healthz':8192,'/v1/catalog':4*1024**2,'/v1/trust/catalog':2*1024**2}


def helper(name):
    require(name in ('upgrade-job-specs-mac.py','prepare-launcher-workshop-profile.py',
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


def installed_context(identity):
    """Use one new verified source/runtime to observe either live generation.

    Rollback does not install this new helper into the preserved old release.
    The Root caller independently verifies both complete installed closures;
    this child binds its immutable entry/runtime and selected health identity.
    No modules or initialization markers from the selected old source are read.
    """
    require(isinstance(identity,str) and re.fullmatch('[0-9a-f]{64}',identity),
            'Choose an exact installed host generation')
    require(sys.platform=='darwin' and sys.flags.isolated and sys.dont_write_bytecode,
            'Use isolated installed Mac Python with -I -B')
    uid=require_locked_process()
    program=Path(__file__).absolute();current=program.parents[4]
    require(current.parent==ROOT/'releases' and re.fullmatch('[0-9a-f]{64}',current.name)
            and program==current/'source/tools/mods/hosting/accept-public-host-services-mac.py',
            'Use the immutable installed public acceptance checker')
    factory=helper('upgrade-job-specs-mac.py')
    factory.protected_program(program)
    factory.closure(current/'source/tools/mods')
    expected=current/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12'
    require(Path(sys.executable).resolve()==expected and Path(sys.prefix).resolve().is_relative_to(current/'runtime'),
            'Public acceptance Python differs from its installed source generation')
    release=ROOT/'releases'/identity
    factory.protected_program(release/'source/tools/mods/mod_host_identity.py')
    return release,uid


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-release',required=True)
    args=parser.parse_args()
    try:
        release,uid=installed_context(args.host_release)
        profile,pinned=helper('prepare-launcher-workshop-profile.py').validate()
        print(json.dumps(wait_ready(release,uid,profile['origin'],pinned),indent=2));return 0
    except (OSError,ValueError,ModError,http.client.HTTPException) as error:
        print('Public host acceptance: '+str(error)[:2000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
