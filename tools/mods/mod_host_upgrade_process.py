"""Bounded isolated child transport for fixed installed upgrade operations."""
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time
import re
import hashlib

from mod_package import read_json,require
from mod_system_host import ROOT

ROOT_UID=0
STDERR_LIMIT=8192


def _capture(arguments,options,*,timeout,limit):
    """Transport only; public callers below choose fixed commands/credentials.

    Cleanup uses the still-owned Popen child handle, never a sampled PID/group.
    Metadata/backup programs do not create descendants. Routing clients must not
    signal unrelated operator services when an operation becomes uncertain.
    """
    child=subprocess.Popen(arguments,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,close_fds=True,**options)
    selector=None;output={};started=time.monotonic()
    try:
        selector=selectors.DefaultSelector()
        for stream in (child.stdout,child.stderr):
            os.set_blocking(stream.fileno(),False);selector.register(stream,selectors.EVENT_READ)
            output[stream]=bytearray()
        while selector.get_map() or child.poll() is None:
            remaining=timeout-(time.monotonic()-started)
            require(remaining>0,'Installed upgrade child exceeded its wall-time limit')
            for key,_ in selector.select(min(.1,remaining)):
                data=os.read(key.fileobj.fileno(),4096)
                if not data:selector.unregister(key.fileobj);continue
                output[key.fileobj]+=data
                require(len(output[key.fileobj]) <= (limit if key.fileobj is child.stdout else STDERR_LIMIT),
                        'Installed upgrade child exceeded its output budget')
        code=child.wait()
        require(code==0 and not output[child.stderr],'Installed upgrade child failed')
        return bytes(output[child.stdout])
    finally:
        if selector is not None:selector.close()
        if child.poll() is None:
            child.terminate()
            try:child.wait(timeout=5)
            except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
        child.stdout.close();child.stderr.close()


def _context(runtime,current,uid,name):
    require(sys.platform=='darwin' and os.geteuid()==ROOT_UID and type(uid) is int and uid>=600,
            'Installed upgrade children require root and a locked service identity')
    require(all(isinstance(v.get('identity'),str) and re.fullmatch('[0-9a-f]{64}',v['identity']) for v in (runtime,current))
            and name in ('upgrade-job-specs-mac.py','force-upgrade-backup-mac.py','migrate-private-generation-mac.py',
                         'accept-installed-scanner-mac.py','accept-private-host-services-mac.py',
                         'accept-public-host-services-mac.py')
            and runtime['uid']==current['uid']==uid
            and runtime['root']==ROOT/'releases'/runtime['identity']
            and current['root']==ROOT/'releases'/current['identity']
            and 'tools/mods/hosting/'+name in current['descriptor']['source_files'],
            'Fixed upgrade program is not in the verified installed closure')
    python=runtime['python'];program=current['root']/'source/tools/mods/hosting'/name
    require(python==runtime['root']/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12',
            'Installed upgrade Python entry point differs')
    options=dict(user=uid,group=uid,extra_groups=[],cwd=runtime['root'],
        env=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(ROOT/'data'),TMPDIR=str(ROOT/'data'),LC_ALL='C',TZ='UTC'))
    # Configuration/scanner helpers use the verified runtime's application
    # packages. User site/PYTHONPATH stay off; metadata/backup remain no-site.
    flags=['-I','-B'] if name in ('migrate-private-generation-mac.py','accept-installed-scanner-mac.py',
                                'accept-private-host-services-mac.py','accept-public-host-services-mac.py') else ['-I','-S','-B']
    return [str(python),*flags,str(program)],options


def checked_migration_plan(data,before,after,profile):
    from mod_host_migration import FIELDS,GUARDS,PRODUCER,wire
    require(isinstance(data,dict) and set(data)=={'manifest','sha256','files','services_changed'}
            and type(data['files']) is int and data['files']==len(FIELDS) and data['services_changed'] is False,
            'Private migration inspection report differs')
    manifest=data['manifest']
    require(isinstance(manifest,dict) and set(manifest)=={'schema','producer','before','after','origin','guards','files'}
            and type(manifest['schema']) is int and manifest['schema']==1 and manifest['producer']==PRODUCER
            and manifest['before']==before and manifest['after']==after and before!=after
            and manifest['origin']==profile['origin'] and isinstance(manifest['guards'],dict)
            and set(manifest['guards'])==set(GUARDS) and all(isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) for v in manifest['guards'].values())
            and manifest['guards']['host/trusted-root.json']==profile['trusted_root_sha256']
            and isinstance(manifest['files'],dict) and set(manifest['files'])==set(FIELDS),
            'Private migration plan changes the admitted authority or output set')
    for record in manifest['files'].values():
        require(isinstance(record,dict) and set(record)=={'before','after'} and record['before']!=record['after']
                and all(isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) for v in record.values()),
                'Invalid private migration file fingerprints')
    require(isinstance(data['sha256'],str) and data['sha256']==hashlib.sha256(wire(manifest)).hexdigest(),
            'Private migration plan fingerprint differs')
    return data


def migration_plan(current,uid,before,profile):
    require(isinstance(before,str) and re.fullmatch('[0-9a-f]{64}',before) and before!=current['identity'],
            'Invalid private migration base generation')
    arguments,options=_context(current,current,uid,'migrate-private-generation-mac.py')
    arguments += ['--inspect','--from-release',before]
    data=read_json(_capture(arguments,options,timeout=30,limit=16384))
    return checked_migration_plan(data,before,current['identity'],profile)


def migration_step(current,uid,before,nonce,plan_sha256,*,rollback=False):
    require(all(isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) for v in (before,nonce,plan_sha256))
            and before!=current['identity'] and type(rollback) is bool,'Invalid fixed private migration request')
    arguments,options=_context(current,current,uid,'migrate-private-generation-mac.py')
    arguments += ['--rollback' if rollback else '--apply','--from-release',before,'--nonce',nonce]
    result=read_json(_capture(arguments,options,timeout=30,limit=8192))
    return checked_migration_result(result,before,current['identity'],plan_sha256,rollback=rollback)


def checked_migration_result(result,before,after,plan_sha256,*,rollback=False):
    require(isinstance(result,dict) and set(result)=={'schema','phase','plan_sha256','before','after','files',
            'services_changed','repository_modified','keys_modified'} and type(result['schema']) is int and result['schema']==1
            and result['phase']==('rolled-back' if rollback else 'applied') and result['plan_sha256']==plan_sha256
            and result['before']==before and result['after']==after and type(result['files']) is int and result['files']==6
            and all(result[k] is False for k in ('services_changed','repository_modified','keys_modified')),
            'Private migration result differs from the requested generation/plan')
    return result


def checked_scanner_result(result,release,uid):
    require(isinstance(result,dict) and set(result)=={'schema','phase','uid','release','version','results',
            'temporary_state_removed','services_started','network_started'}
            and type(result['schema']) is int and result['schema']==1 and result['phase']=='scanner-admitted'
            and type(result['uid']) is int and result['uid']==uid and result['release']==release
            and isinstance(result['version'],str) and result['version'].startswith('ClamAV ')
            and len(result['version'])<=512 and not any(c in result['version'] for c in '\r\n\0')
            and result['results']==dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error')
            and result['temporary_state_removed'] is True and result['services_started'] is False
            and result['network_started'] is False,'Installed scanner controls differ from the fixed release')
    return result


def scanner_check(current,uid):
    arguments,options=_context(current,current,uid,'accept-installed-scanner-mac.py')
    result=read_json(_capture(arguments,options,timeout=900,limit=8192))
    return checked_scanner_result(result,current['identity'],uid)


def checked_private_result(result,release,uid,profile,*,floor=1):
    require(type(floor) is int and floor>=1 and isinstance(result,dict) and set(result)=={'schema','phase','release','uid',
            'origin','trusted_root_sha256','catalog_version','release_count','role_health_verified','transport','public_endpoint_verified'}
            and type(result['schema']) is int and result['schema']==1 and result['phase']=='private-services-verified'
            and result['release']==release and type(result['uid']) is int and result['uid']==uid
            and result['origin']==profile['origin'] and result['trusted_root_sha256']==profile['trusted_root_sha256']
            and isinstance(result['trusted_root_sha256'],str) and re.fullmatch('[0-9a-f]{64}',result['trusted_root_sha256'])
            and type(result['catalog_version']) is int and floor<=result['catalog_version']<2**63
            and type(result['release_count']) is int and 0<=result['release_count']<=1000
            and result['role_health_verified'] is True and result['transport']=='private-loopback-http'
            and result['public_endpoint_verified'] is False,'Private readiness changes the admitted authority or catalog floor')
    return result


def private_readiness(current,uid,profile,*,floor=1):
    arguments,options=_context(current,current,uid,'accept-private-host-services-mac.py')
    result=read_json(_capture(arguments,options,timeout=110,limit=8192))
    return checked_private_result(result,current['identity'],uid,profile,floor=floor)


def checked_public_result(result,release,uid,profile,*,floor=1):
    require(type(floor) is int and 1<=floor<=2**53-1 and isinstance(result,dict)
            and set(result)=={'schema','phase','release','uid','origin','trusted_root_sha256','loopback','public',
                             'release_count','revocations_preserved','role_health_verified','transport','public_endpoint_verified'}
            and type(result['schema']) is int and result['schema']==1 and result['phase']=='public-services-verified'
            and result['release']==release and type(result['uid']) is int and result['uid']==uid
            and result['origin']==profile['origin'] and result['trusted_root_sha256']==profile['trusted_root_sha256']
            and isinstance(result['trusted_root_sha256'],str) and re.fullmatch('[0-9a-f]{64}',result['trusted_root_sha256'])
            and type(result['release_count']) is int and 0<=result['release_count']<=1000
            and result['revocations_preserved'] is True and result['role_health_verified'] is True
            and result['transport']=='public-https' and result['public_endpoint_verified'] is True,
            'Public acceptance changes the admitted authority or fixed HTTPS evidence')
    for key in ('loopback','public'):
        value=result[key]
        require(isinstance(value,dict) and set(value)=={'catalog_version','metadata_sha256','revocations_sha256','revocation_count'}
                and type(value['catalog_version']) is int and floor<=value['catalog_version']<=2**53-1
                and type(value['revocation_count']) is int and 0<=value['revocation_count']<=4000
                and all(isinstance(value[name],str) and re.fullmatch('[0-9a-f]{64}',value[name])
                        for name in ('metadata_sha256','revocations_sha256')),'Invalid public catalog checkpoint')
    local,public=result['loopback'],result['public']
    require(public['catalog_version']>=local['catalog_version'] and public['revocation_count']>=local['revocation_count']
            and (public['catalog_version']!=local['catalog_version'] or public==local),
            'Public catalog lost the live local checkpoint')
    return result


def public_readiness(current,uid,profile,*,floor=1,host_release=None):
    identity=current['identity'] if host_release is None else host_release
    require(isinstance(identity,str) and re.fullmatch('[0-9a-f]{64}',identity),'Choose an exact public host generation')
    arguments,options=_context(current,current,uid,'accept-public-host-services-mac.py')
    arguments+=['--host-release',identity]
    result=read_json(_capture(arguments,options,timeout=110,limit=8192))
    return checked_public_result(result,identity,uid,profile,floor=floor)


def job_specs(runtime,current,uid,*,backup_identity=None):
    arguments,options=_context(runtime,current,uid,'upgrade-job-specs-mac.py')
    identity=runtime['identity'] if backup_identity is None else backup_identity
    require(isinstance(identity,str) and len(identity)==64 and all(c in '0123456789abcdef' for c in identity),
            'Invalid installed factory identity')
    kind='host' if backup_identity is None else 'backup'
    arguments += ['--host-release' if kind=='host' else '--backup-release',identity]
    data=read_json(_capture(arguments,options,timeout=15,limit=65536))
    require(isinstance(data,dict) and set(data)=={'schema','producer','kind','identity','base_release','uid','gid','kernel_groups','entries'}
            and type(data['schema']) is int and data['schema']==1 and data['producer']=='tsfp-upgrade-job-specs-v1'
            and data['kind']==kind and data['identity']==identity and data['base_release']==runtime['identity']
            and type(data['uid']) is int and type(data['gid']) is int and data['uid']==data['gid']==uid
            and isinstance(data['kernel_groups'],list) and len(data['kernel_groups'])<=256
            and all(type(v) is int and v==uid for v in data['kernel_groups'])
            and isinstance(data['entries'],list) and len(data['entries'])==(9 if kind=='host' else 1),
            'Installed job factory report differs')
    result=[]
    for entry in data['entries']:
        require(isinstance(entry,dict) and set(entry)=={'generated','specification'}
                and isinstance(entry['generated'],str) and 0<len(entry['generated'])<=2048
                and isinstance(entry['specification'],dict),'Malformed installed job entry')
        spec=entry['specification'];argv=spec.get('ProgramArguments')
        require(isinstance(spec.get('Label'),str) and isinstance(argv,list) and 1<=len(argv)<=16
                and all(isinstance(v,str) and 0<len(v)<=2048 and not any(c in v for c in '\r\n\t\0') for v in argv)
                and argv[0]==str(runtime['python']),'Invalid installed job command')
        result.append((Path(entry['generated']),spec))
    from mod_host_migration import jobs
    expected=[v for v in jobs() if v!='org.timesplitters.workshop.backup'] if kind=='host' else ['org.timesplitters.workshop.backup']
    require(sorted(spec['Label'] for _,spec in result)==sorted(expected),'Installed factory returned a foreign or duplicate label')
    return result


def forced_backup(runtime,current,uid,backup_identity,*,quiesced_at):
    arguments,options=_context(runtime,current,uid,'force-upgrade-backup-mac.py')
    require(isinstance(backup_identity,str) and len(backup_identity)==64
            and all(c in '0123456789abcdef' for c in backup_identity)
            and type(quiesced_at) is int and 0<=quiesced_at<=time.time(), 'Invalid quiescent backup request')
    arguments += ['--capture','--backup-release',backup_identity]
    report=read_json(_capture(arguments,options,timeout=1800,limit=8192))
    return checked_backup_receipt(report,runtime['identity'],uid,backup_identity,quiesced_at)


def checked_backup_receipt(report,base_release,uid,backup_identity,quiesced_at,*,clock=time.time):
    require(isinstance(report,dict) and set(report)=={'schema','producer','identity','base_release','uid','status','backup',
            'verified_manifest_sha256','created','started','finished','bytes','forced','verified'}
            and type(report['schema']) is int and report['schema']==1 and report['producer']=='tsfp-upgrade-backup-v1'
            and report['identity']==backup_identity and report['base_release']==base_release
            and type(report['uid']) is int and report['uid']==uid and report['status']=='captured'
            and report['forced'] is True and report['verified'] is True
            and all(type(report[k]) is int for k in ('created','started','finished','bytes'))
            and type(quiesced_at) is int and quiesced_at>=0
            and quiesced_at<=report['started']<=report['created']<=report['finished']<=clock()
            and 0<report['bytes']<=8*1024**3 and isinstance(report['verified_manifest_sha256'],str)
            and len(report['verified_manifest_sha256'])==64
            and all(c in '0123456789abcdef' for c in report['verified_manifest_sha256'])
            and isinstance(report['backup'],str)
            and re.fullmatch(r'snapshot-[0-9a-f]{32}-[0-9]{10,12}-[0-9a-f]{8}',report['backup']),
            'Forced backup receipt differs from the quiescent request')
    return report
