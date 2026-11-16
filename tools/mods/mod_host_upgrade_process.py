"""Bounded isolated child transport for fixed installed upgrade operations."""
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time
import re

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
            and name in ('upgrade-job-specs-mac.py','force-upgrade-backup-mac.py')
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
    return [str(python),'-I','-S','-B',str(program)],options


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
    require(isinstance(report,dict) and set(report)=={'schema','producer','identity','base_release','uid','status','backup',
            'verified_manifest_sha256','created','started','finished','bytes','forced','verified'}
            and type(report['schema']) is int and report['schema']==1 and report['producer']=='tsfp-upgrade-backup-v1'
            and report['identity']==backup_identity and report['base_release']==runtime['identity']
            and type(report['uid']) is int and report['uid']==uid and report['status']=='captured'
            and report['forced'] is True and report['verified'] is True
            and all(type(report[k]) is int for k in ('created','started','finished','bytes'))
            and quiesced_at<=report['started']<=report['created']<=report['finished']<=time.time()
            and 0<report['bytes']<=8*1024**3 and isinstance(report['verified_manifest_sha256'],str)
            and len(report['verified_manifest_sha256'])==64
            and all(c in '0123456789abcdef' for c in report['verified_manifest_sha256'])
            and isinstance(report['backup'],str)
            and re.fullmatch(r'snapshot-[0-9a-f]{32}-[0-9]{10,12}-[0-9a-f]{8}',report['backup']),
            'Forced backup receipt differs from the quiescent request')
    return report
