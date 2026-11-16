"""New-only protected backup installation; preserves existing host jobs and data."""
import importlib.util
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import stat
import subprocess
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).absolute().parent))
import mod_backup_release as release

LAUNCHD=Path('/Library/LaunchDaemons')


def command(arguments):
    result=subprocess.run(arguments,stdin=subprocess.DEVNULL,capture_output=True,timeout=10,
                          env=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',LC_ALL='C'))
    release.need(len(result.stdout)+len(result.stderr)<=65536,'Backup utility output exceeded budget')
    return result.returncode,(result.stdout+result.stderr).decode('utf-8','replace')


def sync(path):
    fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)


def write_new(path,data,mode):
    with os.fdopen(os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600),'wb') as output:
        output.write(data);output.flush();os.fchmod(output.fileno(),mode);os.fsync(output.fileno())
    sync(path.parent)


def root_directory(path):
    release.directory(path,release.ROOT_UID)
    release.need(stat.S_IMODE(path.lstat().st_mode)==0o755,'Protected backup directory mode differs')


def new_root_directory(path):
    root_directory(path.parent);path.mkdir(mode=0o700)
    if sys.platform=='darwin':
        code,_=command(['/bin/chmod','-N',str(path)]);release.need(code==0,'Cannot strip new directory ACL')
    path.chmod(0o755);root_directory(path);sync(path.parent)


def load_helper(path):
    release.read(path,release.ROOT_UID)
    spec=importlib.util.spec_from_file_location('backup_install_'+uuid.uuid4().hex,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


def base_binding(plan):
    base=release.ROOT/'releases'/plan['descriptor']['base_release']
    marker=release.decode(release.read(base/'installation.json',release.ROOT_UID,8192))
    release.need(isinstance(marker,dict) and marker.get('producer')=='tsfp-installed-host-v1'
                 and marker.get('phase')=='complete' and marker.get('descriptor_sha256')==base.name
                 and marker.get('release')==str(base),'Base host release is incomplete')
    metadata=release.decode(release.read(base/'runtime/runtime.json',release.ROOT_UID,4*1024**2))
    relative=metadata.get('python') if isinstance(metadata,dict) else None
    release.need(isinstance(relative,str) and not Path(relative).is_absolute() and '..' not in Path(relative).parts,'Invalid base Python path')
    python=(base/'runtime'/relative).resolve()
    release.need(python.is_relative_to(base/'runtime'),'Base Python escaped runtime')
    release.directory(python.parent,release.ROOT_UID);info=python.lstat()
    release.need(stat.S_ISREG(info.st_mode) and info.st_uid==release.ROOT_UID and info.st_nlink==1
                 and not info.st_mode&0o022 and info.st_mode&0o111,'Unsafe base Python')
    account=load_helper(base/'source/tools/mods/hosting/provision-service-mac.py').existing()
    release.need(type(account.get('uid')) is int and account['uid']>=600 and account.get('gid')==account['uid']
                 and marker.get('uid')==account['uid'],'Base service identity differs')
    return dict(schema=1,producer=release.PRODUCER,identity=plan['identity'],base_release=base.name,
                uid=account['uid'],gid=account['gid'],python=str(python))


def stage(plan,binding):
    target=plan['target'];parent=target.parent
    root_directory(release.ROOT)
    if not parent.exists():new_root_directory(parent)
    root_directory(parent)
    if target.exists() or target.is_symlink():
        descriptor,existing=release.installed(target)
        release.need(descriptor==plan['descriptor'] and existing==binding,'Preserving existing backup release')
        return target
    # Re-read and hash every operator-owned byte before creating root output.
    payload={}
    for name,record in plan['descriptor']['files'].items():
        data=release.read(plan['bundle']/name,plan['descriptor']['owner_uid'],record['bytes'])
        release.need(len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],'Prepared backup bytes changed')
        payload[name]=data
    new_root_directory(target)
    for name,data in payload.items():write_new(target/name,data,0o444)
    write_new(target/'release.json',plan['wire'],0o444)
    write_new(target/'installation.json',release.wire(binding),0o444)
    release.need(release.installed(target)==(plan['descriptor'],binding),'Installed backup verification differs')
    return target


def private_parent(uid,gid):
    data=release.ROOT/'data'
    for path in (data,data/'backups'):
        if path.name=='backups' and not path.exists() and not path.is_symlink():
            path.mkdir(mode=0o700)
            if sys.platform=='darwin':
                code,_=command(['/bin/chmod','-N',str(path)]);release.need(code==0,'Cannot strip new private directory ACL')
            os.chown(path,uid,gid);path.chmod(0o700);sync(data)
        info=path.lstat()
        release.need(stat.S_ISDIR(info.st_mode) and info.st_uid==uid and info.st_gid==gid
                     and stat.S_IMODE(info.st_mode)==0o700,'Preserving unknown private backup directory')


def invoke(folder,binding,mode):
    from mod_backup_service import capture
    code,output,error=capture(binding['python'],folder/'backup-bootstrap-mac.py',mode,timeout=1900 if mode in ('--initialize','--verify') else 20)
    release.need(code==0 and not error,'Protected backup operation failed: '+error.decode('utf-8','replace')[:1000])
    value=release.decode(output)
    if mode=='--status':
        from mod_backup_service import checked_status
        checked_status(value)
        release.need(value['identity']==folder.name and value['base_release']==binding['base_release']
                     and value['uid']==binding['uid'] and value['status']=='ok','Protected backup status differs')
    else:
        release.need(isinstance(value,dict) and value.get('schema')==1 and value.get('producer')=='tsfp-backup-worker-v1'
                     and value.get('uid')==binding['uid'] and value.get('gid')==binding['gid']
                     and isinstance(value.get('kernel_groups'),list) and set(value['kernel_groups'])<={binding['uid']}
                     and isinstance(value.get('result'),dict),'Protected backup report differs')
    return value


def job_state(wire):
    runs=re.findall(r'^\s*runs = ([0-9]+)\s*$',wire,re.M)
    exits=re.findall(r'^\s*last exit code = (-?[0-9]+)\s*$',wire,re.M)
    pids=re.findall(r'^\s*pid = ([0-9]+)\s*$',wire,re.M)
    release.need(len(runs)<=1 and len(exits)<=1 and len(pids)<=1,'Ambiguous backup job state')
    return (int(runs[0]) if runs else 0,int(exits[0]) if exits else None,int(pids[0]) if pids else None)


def owned_job(path,call):
    code,wire=call(['/bin/launchctl','print','system/'+release.LABEL])
    if code!=0:return None
    paths=re.findall(r'^\s*path = (.+?)\s*$',wire,re.M)
    release.need(paths==[str(path)],'Preserving a different backup job label')
    return wire


def wait_job(path,minimum,call,clock=time.monotonic,sleep=time.sleep):
    deadline=clock()+1900
    while True:
        wire=owned_job(path,call);release.need(wire is not None,'Backup job disappeared')
        runs,code,pid=job_state(wire)
        if runs>=minimum and pid is None:
            release.need(code==0,'Scheduled backup job failed');return runs
        release.need(clock()<deadline,'Scheduled backup job exceeded admission time')
        sleep(.25)


def register(folder,binding,call=command,operation=invoke,wait=wait_job):
    """Admit a staging job twice, then register and verify its boot-file copy."""
    root_directory(LAUNCHD);final=LAUNCHD/(release.LABEL+'.plist')
    release.need(not final.exists() and not final.is_symlink(),'Preserving existing backup startup file')
    release.need(call(['/bin/launchctl','print','system/'+release.LABEL])[0]!=0,'Preserving existing backup job label')
    jobs=release.ROOT/'backup-jobs'
    if not jobs.exists():new_root_directory(jobs)
    root_directory(jobs);directory=jobs/folder.name
    if not directory.exists():new_root_directory(directory)
    root_directory(directory)
    release.need(sum(1 for _ in directory.iterdir())<32,'Backup deployment journal budget exhausted')
    source=directory/'worker.plist';wire=plistlib.dumps(release.job(folder,binding))
    if source.exists() or source.is_symlink():release.need(release.read(source,release.ROOT_UID,32768)==wire,'Preserving changed staging job')
    else:write_new(source,wire,0o444)
    journal=directory/('attempt-'+uuid.uuid4().hex+'.json')
    base=dict(schema=1,producer='tsfp-backup-deployment-v1',identity=folder.name,base_release=binding['base_release'],uid=binding['uid'])
    def record(phase,**extra):
        value=release.wire(dict(base,time=int(time.time()),phase=phase,**extra))
        temporary=directory/('.attempt-'+uuid.uuid4().hex)
        write_new(temporary,value,0o600);os.replace(temporary,journal);sync(directory)
    loaded=None;created=None;record('preparing')
    try:
        private_parent(binding['uid'],binding['gid'])
        probe=operation(folder,binding,'--probe')
        release.need(probe['result']==dict(status='probe',root_metadata_denied=True),'Backup privilege-drop probe differs')
        first=operation(folder,binding,'--initialize')
        release.need(first['result'].get('status') in ('captured','not-due'),'Initial backup was not captured')
        verified=operation(folder,binding,'--verify')
        release.need(verified['result'].get('status')=='verified','Initial backup verification failed')
        release.need(call(['/bin/launchctl','print','system/'+release.LABEL])[0]!=0,'Backup job appeared during admission')
        # Mark an uncertain bootstrap as ours only after its reported path matches.
        code,_=call(['/bin/launchctl','bootstrap','system',str(source)])
        if owned_job(source,call) is not None:loaded=source
        release.need(code==0 and loaded==source,'Backup staging bootstrap failed')
        record('checking-staging',loaded=str(source))
        runs=wait(source,1,call)
        operation(folder,binding,'--status')
        release.need(call(['/bin/launchctl','kickstart','system/'+release.LABEL])[0]==0,'Cannot repeat backup job')
        wait(source,runs+1,call);operation(folder,binding,'--status')
        release.need(owned_job(source,call) is not None,'Staging backup job changed')
        release.need(call(['/bin/launchctl','bootout','system/'+release.LABEL])[0]==0,'Cannot unload admitted staging job');loaded=None
        # No persistent startup file exists until all checks above pass.
        child=os.open(final,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
        with os.fdopen(child,'wb') as output:
            info=os.fstat(output.fileno());created=(info.st_dev,info.st_ino)
            output.write(wire);output.flush();os.fchmod(output.fileno(),0o644);os.fsync(output.fileno())
        sync(final.parent)
        code,_=call(['/bin/launchctl','bootstrap','system',str(final)])
        if owned_job(final,call) is not None:loaded=final
        release.need(code==0 and loaded==final,'Persistent backup bootstrap failed')
        wait(final,1,call);current=operation(folder,binding,'--status')
        verified=operation(folder,binding,'--verify')
        result=dict(base,phase='complete',persistent_file=str(final),staging_runs=2,persistent_run_verified=True,
                    snapshot=verified['result'],status=current,existing_host_jobs_changed=False,public_routing_changed=False)
        record('complete',result=result);return result
    except BaseException:
        complete=True
        if loaded is not None:
            try:
                if owned_job(loaded,call) is not None:
                    release.need(call(['/bin/launchctl','bootout','system/'+release.LABEL])[0]==0,'Backup rollback unload failed')
            except (OSError,ValueError,subprocess.SubprocessError):complete=False
        if created is not None:
            try:
                child=os.open(final,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
                with os.fdopen(child,'rb') as source:
                    info=os.fstat(source.fileno());current=source.read(len(wire)+1)
                release.need(stat.S_ISREG(info.st_mode) and info.st_uid==release.ROOT_UID and info.st_nlink==1
                             and (info.st_dev,info.st_ino)==created and len(current)<=len(wire)
                             and current==wire[:len(current)],'Preserving changed startup artifact')
                final.unlink();sync(final.parent)
            except (OSError,ValueError):complete=False
        record('failed',rollback_complete=complete)
        if not complete:raise ValueError('Backup deployment failed; protected journal records incomplete cleanup')
        raise


def activate():
    release.need(sys.platform=='darwin' and os.geteuid()==0,'Use local Mac administrator authentication')
    folder=Path(__file__).absolute().parent
    _,binding=release.installed(folder)
    release.need(str(Path(sys.executable).resolve())==binding['python'],'Use the bound installed Python')
    helper=load_helper(release.ROOT/'releases'/binding['base_release']/'source/tools/mods/hosting/locked-service-bootstrap-mac.py')
    base,account=helper.monitor.installed()
    release.need(base.name==binding['base_release'] and account['uid']==binding['uid'] and account['gid']==binding['gid'],'Installed backup service identity differs')
    return register(folder,binding)


if __name__=='__main__':
    try:
        release.need(sys.argv[1:]==['--activate'],'Invalid fixed installation operation')
        print(json.dumps(activate(),sort_keys=True));raise SystemExit(0)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print('Backup installation: '+str(error)[:2000],file=sys.stderr);raise SystemExit(2)
