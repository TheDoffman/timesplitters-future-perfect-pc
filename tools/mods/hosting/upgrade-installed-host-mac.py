"""Inspect a preserving installed-host upgrade; no activation or service commands."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
from urllib.parse import urlsplit


def helper(filename):
    if filename not in ('host_release.py','system-scan-monitor-mac.py'):
        raise ValueError('Invalid fixed upgrade helper')
    spec=importlib.util.spec_from_file_location('upgrade_'+filename.replace('.','_'),Path(__file__).with_name(filename))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


release=helper('host_release.py')


def public_profile(verified):
    name='config/mods/public-workshop.json'
    release.require(name in verified['descriptor']['source_files'],'Installed source lacks a public workshop profile')
    value=release.decode(release.read_file(verified['root']/'source',name,release.ROOT_UID,8192),8192)
    release.require(isinstance(value,dict) and set(value)=={'schema','origin','trusted_root_sha256','status'}
        and type(value['schema']) is int and value['schema']==1 and value['status'] in ('active','pending-public-acceptance')
        and isinstance(value['trusted_root_sha256'],str) and release.SHA.fullmatch(value['trusted_root_sha256'])
        and isinstance(value['origin'],str) and 0<len(value['origin'])<=512 and value['origin'].isascii()
        and all(32<ord(c)<127 for c in value['origin']), 'Invalid installed public workshop profile')
    address=urlsplit(value['origin'])
    release.require(address.scheme=='https' and address.hostname and address.username is None
        and address.password is None and not address.path and not address.query and not address.fragment
        and (address.port is None or 1<=address.port<=65535),'Invalid installed public workshop origin')
    return value


def inspection(before,after,uid):
    release.require(isinstance(before,str) and isinstance(after,str) and release.SHA.fullmatch(before)
                    and release.SHA.fullmatch(after) and before!=after,'Choose two distinct exact installed releases')
    old=release.installed(before,uid);new=release.installed(after,uid)
    old_profile,new_profile=public_profile(old),public_profile(new)
    release.require(all(old_profile[k]==new_profile[k] for k in ('origin','trusted_root_sha256')),
                    'Upgrade changes the workshop authority; preserving it requires a separate contract')
    return dict(schema=1,phase='installed-upgrade-inspected',before=before,after=after,uid=uid,
        source_revisions=dict(before=old['descriptor']['source_revision'],after=new['descriptor']['source_revision']),
        origin=old_profile['origin'],trusted_root_sha256=old_profile['trusted_root_sha256'],
        installed_sources_verified=True,installed_runtimes_verified=True,
        services_changed=False,network_started=False,data_written=False)


def root_context(before):
    release.require(isinstance(before,str) and release.SHA.fullmatch(before),'An exact installed base release is required')
    release.require(sys.platform=='darwin' and os.geteuid()==0,'Use local Mac administrator authentication')
    release.require(sys.flags.isolated and sys.flags.no_site and sys.dont_write_bytecode,
                    'Use installed Python with -I -S -B')
    after,account=helper('system-scan-monitor-mac.py').installed()
    script=Path(__file__).absolute()
    release.require(script==after/'source/tools/mods/hosting/upgrade-installed-host-mac.py',
                    'Use the immutable installed upgrade entry point')
    verified=release.installed(after.name,account['uid'])
    release.require(str(Path(sys.executable).resolve())==str(verified['python'])
                    and Path(sys.prefix).resolve().is_relative_to(after/'runtime')
                    and 'tools/mods/hosting/upgrade-installed-host-mac.py' in verified['descriptor']['source_files'],
                    'Upgrade entry point or Python does not belong to this installed release')
    return after.name,account['uid']


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspect',action='store_true')
    parser.add_argument('--from-release')
    args=parser.parse_args()
    try:
        if not args.inspect:
            release.require(args.from_release is None,'Choose explicit installed upgrade inspection')
            value=dict(schema=1,phase='plan-only',installed_releases=2,jobs=10,services_changed=False,
                       network_started=False,data_written=False)
        else:
            after,uid=root_context(args.from_release)
            value=inspection(args.from_release,after,uid)
        print(json.dumps(value,indent=2));return 0
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print('Installed host upgrade: '+str(error)[:1000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
