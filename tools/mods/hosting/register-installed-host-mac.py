"""New-only registration of nine fixed private workshop jobs; no public routing."""
import argparse
import importlib.util
from itertools import islice
import json
import os
from pathlib import Path
import plistlib
import stat
import subprocess
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from mod_host_watch import run_command,managed_pid
from mod_package import ModError,read_json,require
from mod_system_host import ROOT,ROLES,paths

LAUNCHD=Path('/Library/LaunchDaemons')
ROOT_UID=0


def helper(name):
    require(name in ('system-scan-monitor-mac.py','system-host-monitor-mac.py'),'Invalid registration helper')
    spec=importlib.util.spec_from_file_location('register_'+name.replace('.','_'),Path(__file__).with_name(name))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def jobs(release,account,python):
    scan=helper('system-scan-monitor-mac.py');host=helper('system-host-monitor-mac.py')
    worker,controller=scan.plists(release,account,python)
    monitors=[(ROOT/'monitor/monitor.plist',controller)]
    workers=[]
    for role in ROLES:
        child,watch=host.plists(role,release,python)
        monitors.append((paths(role)['monitor']/'monitor.plist',watch))
        workers.append((paths(role)['monitor']/'worker.plist',child))
    return [*monitors,*workers,(ROOT/'monitor/scan.plist',worker),
            (ROOT/'monitor/definitions.plist',scan.definitions_plist(release,python))]


def admission(release,account,clock=time.time):
    scan=helper('system-scan-monitor-mac.py')
    value=scan.root_json(ROOT/'monitor','admission.json')
    profile=scan.root_json(release/'source/config/mods','public-workshop.json')
    stamp=value.get('time')
    require(type(value.get('schema')) is int and value['schema']==1
            and value.get('producer')=='tsfp-private-host-admission-v1' and value.get('release')==str(release)
            and type(value.get('uid')) is int and value['uid']==account['uid']
            and value.get('origin')==profile.get('origin') and value.get('trusted_root_sha256')==profile.get('trusted_root_sha256')
            and type(stamp) is int and 0<=clock()-stamp<=300 and value.get('services_started') is False
            and value.get('public_endpoint_started') is False,'Fresh protected host admission is required')
    scanner=value.get('scanner')
    require(isinstance(scanner,dict) and scanner.get('phase')=='scanner-admitted'
            and scanner.get('release')==release.name and type(scanner.get('uid')) is int and scanner['uid']==account['uid']
            and scanner.get('temporary_state_removed') is True and scanner.get('services_started') is False
            and scanner.get('network_started') is False
            and scanner.get('results')==dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error'),
            'Protected scanner admission differs')
    return value


def read_plist(path,expected):
    scan=helper('system-scan-monitor-mac.py')
    fd=scan.root_directory(path.parent)
    try:child=os.open(path.name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=fd)
    finally:os.close(fd)
    with os.fdopen(child,'rb') as source:
        before=os.fstat(source.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_uid==ROOT_UID and before.st_nlink==1
                and stat.S_IMODE(before.st_mode)==0o600 and 0<before.st_size<=32768,'Unsafe generated system plist')
        wire=source.read(32769);after=os.fstat(source.fileno())
        fields=lambda s:(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)
        require(len(wire)==before.st_size and fields(before)==fields(after) and wire==plistlib.dumps(expected),
                'Generated plist differs from the fixed immutable policy')
    return wire


class Receipt:
    def __init__(self,release,uid):
        self.scan=helper('system-scan-monitor-mac.py')
        self.directory=ROOT/'monitor/deployments'
        try:self.directory.mkdir(mode=0o700)
        except FileExistsError:pass
        fd=self.scan.root_directory(self.directory)
        try:
            require(stat.S_IMODE(os.fstat(fd).st_mode)==0o700,'Unsafe deployment journal')
            with os.scandir(fd) as entries:require(len(list(islice(entries,33)))<32,'Deployment journal budget exhausted')
        finally:os.close(fd)
        self.name='deployment-'+uuid.uuid4().hex+'.json'
        self.base=dict(schema=1,producer='tsfp-private-job-deployment-v1',release=str(release),uid=uid,id=self.name)
        self.write(dict(phase='preparing'),first=True)

    def write(self,report,first=False):
        if not first:
            previous=self.scan.root_json(self.directory,self.name)
            require(all(previous.get(k)==v for k,v in self.base.items()),'Preserving unknown deployment journal')
        wire=json.dumps(dict(self.base,time=int(time.time()),**report),separators=(',',':')).encode()
        require(len(wire)<=8192,'Deployment report exceeds budget')
        fd=self.scan.root_directory(self.directory)
        name=self.name if first else '.deployment-'+uuid.uuid4().hex
        try:
            child=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
            with os.fdopen(child,'wb') as output:output.write(wire);output.flush();os.fsync(output.fileno())
            if not first:os.replace(name,self.name,src_dir_fd=fd,dst_dir_fd=fd)
            os.fsync(fd)
        finally:os.close(fd)


def private_ready(release,account,python):
    result=subprocess.run([str(python),'-I','-B',str(release/'source/tools/mods/hosting/accept-private-host-services-mac.py')],
                          user=account['uid'],group=account['gid'],extra_groups=[],stdin=subprocess.DEVNULL,
                          capture_output=True,timeout=110,cwd=release,
                          env=dict(PATH='/usr/bin:/bin',HOME=str(ROOT/'data'),TMPDIR=str(ROOT/'data'),LC_ALL='C',TZ='UTC'))
    require(result.returncode==0 and len(result.stdout)<=8192 and len(result.stderr)<=8192,'Private services failed admission')
    value=read_json(result.stdout)
    require(isinstance(value,dict) and type(value.get('schema')) is int and value['schema']==1
            and value.get('phase')=='private-services-verified' and type(value.get('uid')) is int and value['uid']==account['uid']
            and value.get('release')==release.name and value.get('role_health_verified') is True
            and type(value.get('catalog_version')) is int and value['catalog_version']>=1
            and type(value.get('release_count')) is int and 0<=value['release_count']<=1000
            and value.get('transport')=='private-loopback-http' and value.get('public_endpoint_verified') is False,
            'Private readiness report differs')
    return value


def process_rows(command):
    code,wire=command(['/bin/ps','-axo','pid=,ppid=,uid='])
    require(code==0,'Cannot observe private service processes')
    rows=wire.splitlines()
    require(len(rows)<=8192,'Private process observation exceeds budget')
    result={}
    for row in rows:
        values=row.split()
        require(len(values)==3 and all(v.lstrip('-').isdigit() for v in values),'Invalid private process observation')
        pid,parent,uid=map(int,values)
        require(pid>0 and parent>=0 and pid not in result,'Ambiguous private process observation')
        result[pid]=(parent,uid)
    return result


def observe(label,uid,command):
    target='system/'+label
    code,wire=command(['/bin/launchctl','print',target])
    require(code==0,'Private restart target is not loaded')
    pid=managed_pid(wire)
    require(type(pid) is int and pid>1,'Private restart target is not running')
    rows=process_rows(command)
    tree={pid}
    while True:
        children={child for child,(parent,_) in rows.items() if parent in tree}-tree
        if not children:break
        tree.update(children)
        require(len(tree)<=8,'Private restart tree exceeds budget')
    require(all(p in rows and rows[p][1]==uid for p in tree),'Private restart tree owner differs')
    code,wire=command(['/bin/launchctl','print',target])
    require(code==0 and managed_pid(wire)==pid,'Private service changed during observation')
    return pid,tree


def lifecycle(release,account,python,entries,initial,command=run_command,clock=time.monotonic,sleep=time.sleep):
    # Only the immutable nine-job set can select targets. One-shot scan/definition
    # execution is tested separately; restart every long-lived worker and monitor.
    targets=[(v['Label'],account['uid']) for _,v in entries if v['Label'].split('.')[-1]!='monitor'
             and v.get('KeepAlive') is True]
    targets.extend((v['Label'],0) for _,v in entries if v['Label'].endswith('.monitor'))
    require(len(targets)==7,'Private lifecycle target set differs')
    results=[];previous=initial
    for label,uid in targets:
        before,tree=observe(label,uid,command)
        code,_=command(['/bin/launchctl','kill','SIGKILL','system/'+label])
        require(code==0,'Cannot restart fixed private service label')
        deadline=clock()+110
        while True:
            code,wire=command(['/bin/launchctl','print','system/'+label])
            after=managed_pid(wire) if code==0 else None
            present=process_rows(command)
            if after is not None and after!=before and not tree.intersection(present):break
            require(clock()<deadline,'Private restart or child cleanup timed out')
            sleep(.5)
        accepted=private_ready(release,account,python)
        require(accepted.get('origin')==previous.get('origin')
                and accepted.get('trusted_root_sha256')==previous.get('trusted_root_sha256')
                and type(accepted.get('catalog_version')) is int
                and accepted['catalog_version']>=previous['catalog_version'],'Private catalog authority or sequence regressed')
        previous=accepted
        results.append(dict(label=label,old_pid=before,new_pid=after,old_tree_exited=True))
    return results,previous


def rollback(fd,created,loaded,uncertain,command):
    complete=not uncertain
    for label in reversed(loaded):
        try:
            code,_=command(['/bin/launchctl','bootout','system/'+label])
            if code!=0:require(command(['/bin/launchctl','print','system/'+label])[0]!=0,'Known job remains loaded')
        except (OSError,ValueError,ModError,subprocess.SubprocessError):complete=False
    for name,identity,wire in reversed(created):
        try:
            child=os.open(name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=fd)
            with os.fdopen(child,'rb') as source:
                info=os.fstat(source.fileno());current=source.read(len(wire)+1)
                require(stat.S_ISREG(info.st_mode) and info.st_uid==ROOT_UID and info.st_nlink==1
                        and (info.st_dev,info.st_ino)==identity and len(current)<=len(wire)
                        and current==wire[:len(current)],'Preserving changed job artifact')
            os.unlink(name,dir_fd=fd)
        except (OSError,ValueError,ModError):complete=False
    os.fsync(fd)
    return complete


def register(release,account,python,command=run_command):
    proof=admission(release,account)
    entries=jobs(release,account,python)
    require(len(entries)==9 and len({v['Label'] for _,v in entries})==9,'Fixed deployment job set differs')
    payloads=[(v['Label'],path,read_plist(path,v)) for path,v in entries]
    scan=helper('system-scan-monitor-mac.py')
    fd=scan.root_directory(LAUNCHD)
    created=[];loaded=[];uncertain=[];receipt=None
    try:
        # Preflight every destination and label before creating any boot artifact.
        for label,_,_ in payloads:
            try:os.stat(label+'.plist',dir_fd=fd,follow_symlinks=False)
            except FileNotFoundError:pass
            else:raise ModError('Preserving existing system job file')
            require(command(['/bin/launchctl','print','system/'+label])[0]!=0,'Preserving existing system job label')
        receipt=Receipt(release,account['uid'])
        # Prove the live jobs from protected staging first. A crash before
        # readiness cannot leave new unverified files scheduled for next boot.
        for label,source,_ in payloads:
            require(command(['/bin/launchctl','print','system/'+label])[0]!=0,'System job label appeared during registration')
            code,_=command(['/bin/launchctl','bootstrap','system',str(source)])
            if code!=0:
                if command(['/bin/launchctl','print','system/'+label])[0]==0:uncertain.append(label)
                raise ModError('Private job bootstrap failed')
            loaded.append(label)
            receipt.write(dict(phase='starting',loaded=loaded,public_endpoint_started=False))
        ready=private_ready(release,account,python)
        require(ready.get('origin')==proof['origin'] and ready.get('trusted_root_sha256')==proof['trusted_root_sha256'],
                'Running private services differ from the admitted public pin')
        receipt.write(dict(phase='checking-restarts',loaded=loaded,public_endpoint_started=False))
        restarts,ready=lifecycle(release,account,python,entries,ready,command)
        for label,_,_ in payloads:require(command(['/bin/launchctl','print','system/'+label])[0]==0,'Private job registration vanished')
        for label,_,wire in payloads:
            name=label+'.plist'
            child=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
            with os.fdopen(child,'wb') as output:
                info=os.fstat(output.fileno());created.append((name,(info.st_dev,info.st_ino),wire))
                output.write(wire);output.flush();os.fchmod(output.fileno(),0o644);os.fsync(output.fileno())
        os.fsync(fd)
        result=dict(schema=1,phase='private-jobs-registered',release=release.name,jobs=loaded,readiness=ready,restarts=restarts,
                    persistent_files_installed=True,public_endpoint_started=False)
        receipt.write(dict(phase='complete',jobs=loaded,readiness=ready,restarts=restarts,public_endpoint_started=False))
        return result
    except BaseException:
        complete=rollback(fd,created,loaded,uncertain,command)
        if receipt is not None:receipt.write(dict(phase='failed',rollback_complete=complete,uncertain_labels=uncertain,public_endpoint_started=False))
        if not complete:raise ModError('Private deployment failed; protected journal records incomplete cleanup')
        raise
    finally:os.close(fd)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args()
    try:
        if not args.apply:value=dict(schema=1,phase='plan-only',jobs=9,persistent_files_installed=False,public_endpoint_started=False)
        else:
            scan=helper('system-scan-monitor-mac.py');release,account=scan.installed()
            require(Path(__file__).absolute()==release/'source/tools/mods/hosting/register-installed-host-mac.py',
                    'Use the immutable installed job registrar')
            value=register(release,account,Path(sys.executable).resolve())
        print(json.dumps(value,indent=2));return 0
    except (OSError,ValueError,ModError,subprocess.SubprocessError) as error:
        print('Private job registration: '+str(error)[:2000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
