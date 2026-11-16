"""Small backup-code releases bound to an existing immutable Mac host runtime."""
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess

ROOT=Path('/Library/Application Support/TSFP Mod Workshop')
ROOT_UID=0
LABEL='org.timesplitters.workshop.backup'
PRODUCER='tsfp-backup-code-v1'
FILES={name:'tools/mods/'+name for name in
       ('mod_backup.py','mod_backup_pool.py','mod_package.py','mod_host_identity.py',
        'mod_backup_release.py','mod_backup_service.py')}
FILES['backup-bootstrap-mac.py']='tools/mods/hosting/backup-bootstrap-mac.py'
MAX_FILE=1024*1024


def need(condition,message):
    if not condition:raise ValueError(message)


def directory(path,uid):
    path=Path(os.path.abspath(path))
    for member in (path,*path.parents):
        info=member.lstat()
        need(stat.S_ISDIR(info.st_mode) and info.st_uid in (0,uid) and not info.st_mode&0o022,
             'Unsafe backup-code ancestor')
    need(path.stat().st_uid==uid,'Backup-code directory owner differs')
    return path


def read(path,uid,maximum=MAX_FILE):
    path=Path(os.path.abspath(path))
    # Pin every ancestor while walking; an operator-owned preparation directory
    # can be renamed between validation and copying by the privileged installer.
    parent=os.open(path.anchor,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        for name in path.parts[1:-1]:
            child=os.open(name,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=parent)
            os.close(parent);parent=child
            info=os.fstat(parent)
            need(info.st_uid in (0,uid) and not info.st_mode&0o022,'Unsafe backup-code ancestor')
        need(os.fstat(parent).st_uid==uid,'Backup-code directory owner differs')
        descriptor=os.open(path.name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=parent)
    finally:os.close(parent)
    with os.fdopen(descriptor,'rb') as source:
        before=os.fstat(source.fileno())
        need(stat.S_ISREG(before.st_mode) and before.st_uid==uid and before.st_nlink==1
             and not before.st_mode&0o022 and 0<before.st_size<=maximum,'Unsafe backup-code file')
        data=source.read(maximum+1);after=os.fstat(source.fileno())
        fields=lambda s:(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)
        need(len(data)==before.st_size and fields(before)==fields(after),'Backup-code file changed')
    return data


def decode(data):
    def pairs(items):
        result={}
        for key,value in items:
            need(key not in result,'Duplicate backup descriptor field');result[key]=value
        return result
    return json.loads(data,object_pairs_hook=pairs)


def wire(value):return (json.dumps(value,sort_keys=True,separators=(',',':'))+'\n').encode()


def checked_descriptor(data):
    value=decode(data)
    need(isinstance(value,dict) and set(value)=={'schema','producer','source_revision','base_release','owner_uid','files'}
         and type(value['schema']) is int and value['schema']==1 and value['producer']==PRODUCER,'Invalid backup descriptor')
    need(isinstance(value['source_revision'],str) and re.fullmatch('[0-9a-f]{40}',value['source_revision'])
         and isinstance(value['base_release'],str) and re.fullmatch('[0-9a-f]{64}',value['base_release'])
         and type(value['owner_uid']) is int and value['owner_uid']>=0,'Invalid backup release identity')
    need(isinstance(value['files'],dict) and set(value['files'])==set(FILES),'Unexpected backup source inventory')
    for name,record in value['files'].items():
        need(isinstance(record,dict) and set(record)=={'sha256','bytes'} and type(record['bytes']) is int
             and 0<record['bytes']<=MAX_FILE and isinstance(record['sha256'],str)
             and re.fullmatch('[0-9a-f]{64}',record['sha256']),'Invalid backup source metadata')
    return value


def prepare(source,output,base_release):
    source=Path(source).resolve();output=Path(os.path.abspath(output));uid=os.getuid()
    directory(output.parent,uid)
    need(not output.exists() and not output.is_symlink(),'Preserving existing backup preparation')
    need(isinstance(base_release,str) and re.fullmatch('[0-9a-f]{64}',base_release),'Invalid base release')
    def git(*args):return subprocess.check_output(['git','-C',str(source),*args])
    need(not git('status','--porcelain').strip(),'Commit backup source before preparing installation')
    revision=git('rev-parse','HEAD').decode().strip();payload={}
    for name,path in FILES.items():
        data=read(source/path,uid)
        need(data==git('show',revision+':'+path),'Backup source differs from committed bytes')
        payload[name]=data
    value=dict(schema=1,producer=PRODUCER,source_revision=revision,base_release=base_release,owner_uid=uid,
               files={name:dict(sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)) for name,data in payload.items()})
    descriptor=wire(value);checked_descriptor(descriptor)
    need(not git('status','--porcelain').strip() and git('rev-parse','HEAD').decode().strip()==revision,'Source changed during preparation')
    output.mkdir(mode=0o700)
    for name,data in dict(payload,**{'release.json':descriptor}).items():
        with (output/name).open('xb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
        (output/name).chmod(0o444)
    return dict(bundle=str(output),sha256=hashlib.sha256(descriptor).hexdigest(),source_revision=revision,
                base_release=base_release,files=len(payload),bytes=sum(map(len,payload.values())),
                services_started=False,root_modified=False)


def plan(bundle,identity):
    need(isinstance(identity,str) and re.fullmatch('[0-9a-f]{64}',identity),'Invalid backup descriptor fingerprint')
    bundle=Path(os.path.abspath(bundle))
    owner=bundle.lstat().st_uid
    data=read(bundle/'release.json',owner,65536)
    need(hashlib.sha256(data).hexdigest()==identity,'Backup descriptor fingerprint differs')
    value=checked_descriptor(data);need(value['owner_uid']==owner,'Backup operator identity differs')
    need({p.name for p in bundle.iterdir()}==set(FILES)|{'release.json'},'Unexpected backup bundle entries')
    for name,record in value['files'].items():
        content=read(bundle/name,owner,record['bytes'])
        need(len(content)==record['bytes'] and hashlib.sha256(content).hexdigest()==record['sha256'],'Backup source digest differs')
    return dict(bundle=bundle,identity=identity,wire=data,descriptor=value,target=ROOT/'backup-releases'/identity)


def installed(folder):
    folder=Path(os.path.abspath(folder))
    need(folder.parent==ROOT/'backup-releases' and re.fullmatch('[0-9a-f]{64}',folder.name),'Use installed backup code')
    data=read(folder/'release.json',ROOT_UID,65536)
    need(hashlib.sha256(data).hexdigest()==folder.name,'Installed backup identity differs')
    value=checked_descriptor(data)
    need({p.name for p in folder.iterdir()}==set(FILES)|{'release.json','installation.json'},'Unexpected installed backup entries')
    for name,record in value['files'].items():
        content=read(folder/name,ROOT_UID,record['bytes'])
        need(len(content)==record['bytes'] and hashlib.sha256(content).hexdigest()==record['sha256'],'Installed backup code changed')
    binding=decode(read(folder/'installation.json',ROOT_UID,8192))
    need(isinstance(binding,dict) and set(binding)=={'schema','producer','identity','base_release','uid','gid','python'}
         and type(binding['schema']) is int and binding['schema']==1 and binding['producer']==PRODUCER
         and binding['identity']==folder.name and binding['base_release']==value['base_release']
         and type(binding['uid']) is int and binding['uid']>=600 and type(binding['gid']) is int
         and binding['gid']==binding['uid'] and isinstance(binding['python'],str),'Invalid installed backup binding')
    base=ROOT/'releases'/binding['base_release'];python=Path(binding['python'])
    need(python.is_absolute() and python.is_relative_to(base/'runtime') and python==python.resolve(),'Backup Python escaped protected runtime')
    return value,binding


def job(folder,binding):
    need(Path(folder)==ROOT/'backup-releases'/binding['identity'],'Backup job identity differs')
    return dict(Label=LABEL,ProgramArguments=[binding['python'],'-I','-S','-B',str(Path(folder)/'backup-bootstrap-mac.py'),'--once'],
        RunAtLoad=True,StartInterval=3600,KeepAlive=False,AbandonProcessGroup=False,ExitTimeOut=20,
        ProcessType='Background',LowPriorityIO=True,Nice=10,Umask=0o077,
        StandardOutPath='/dev/null',StandardErrorPath='/dev/null',
        EnvironmentVariables=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(ROOT/'data'),TMPDIR=str(ROOT/'data'),LC_ALL='C',TZ='UTC'),
        SoftResourceLimits=dict(Core=0,NumberOfFiles=64,CPU=600),HardResourceLimits=dict(Core=0,NumberOfFiles=64,CPU=600))
