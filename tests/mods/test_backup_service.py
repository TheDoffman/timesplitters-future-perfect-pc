"""Immutable backup binding and real bounded POSIX child-process supervision."""
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

import mod_backup_release as release


def descriptor():
    return dict(schema=1,producer=release.PRODUCER,source_revision='1'*40,base_release='2'*64,owner_uid=501,
                files={name:dict(bytes=1,sha256=hashlib.sha256(b'x').hexdigest()) for name in release.FILES})


def test_descriptor_roundtrip_and_fixed_launchd_job():
    value=descriptor()
    assert release.checked_descriptor(release.wire(value))==value
    binding=dict(identity='3'*64,python='/protected/runtime/python',base_release='2'*64,uid=600,gid=600)
    job=release.job(release.ROOT/'backup-releases'/binding['identity'],binding)
    assert job['Label']=='org.timesplitters.workshop.backup'
    assert job['ProgramArguments']==[binding['python'],'-I','-S','-B',str(release.ROOT/'backup-releases'/binding['identity']/'backup-bootstrap-mac.py'),'--once']
    assert job['StartInterval']==3600 and job['RunAtLoad'] and not job['KeepAlive'] and not job['AbandonProcessGroup']
    assert job['HardResourceLimits']==dict(Core=0,NumberOfFiles=64,CPU=600)
    with pytest.raises(ValueError):release.job(Path('/other'),binding)


@pytest.mark.parametrize('field,value',[('schema',True),('schema',2),('owner_uid',True),('owner_uid',-1),
    ('source_revision','../bad'),('base_release','0'*63),('files',{}),('producer','foreign')])
def test_descriptor_rejects_malformed_bindings(field,value):
    data=descriptor();data[field]=value
    with pytest.raises(ValueError):release.checked_descriptor(release.wire(data))


def test_descriptor_rejects_duplicate_and_unbounded_payload_metadata():
    with pytest.raises(ValueError,match='Duplicate'):release.decode(b'{"schema":1,"schema":1}')
    for replacement in ({'bytes':True,'sha256':'0'*64},{'bytes':release.MAX_FILE+1,'sha256':'0'*64},
                        {'bytes':1,'sha256':'../bad'},{'bytes':1,'sha256':'0'*64,'path':'../escape'}):
        data=descriptor();data['files'][next(iter(release.FILES))]=replacement
        with pytest.raises(ValueError):release.checked_descriptor(release.wire(data))


def test_read_refuses_linked_or_writable_inputs(private_posix_path):
    root=private_posix_path;file=root/'code';file.write_bytes(b'trusted');file.chmod(0o444)
    assert release.read(file,os.getuid())==b'trusted'
    link=root/'link';link.symlink_to(file)
    with pytest.raises(OSError):release.read(link,os.getuid())
    linked=root/'alias';os.link(file,linked)
    with pytest.raises(ValueError):release.read(file,os.getuid())
    linked.unlink();file.chmod(0o666)
    with pytest.raises(ValueError):release.read(file,os.getuid())
    file.chmod(0o444);root.chmod(0o777)
    try:
        with pytest.raises(ValueError):release.read(file,os.getuid())
    finally:root.chmod(0o700)


def test_read_refuses_linked_ancestor(private_posix_path):
    root=private_posix_path;real=root/'real';real.mkdir();(real/'code').write_bytes(b'x')
    (root/'alias').symlink_to(real,target_is_directory=True)
    with pytest.raises(OSError):release.read(root/'alias/code',os.getuid())


def test_preparation_binds_committed_bytes_and_preserves_existing_output(private_posix_path):
    root=private_posix_path;source=root/'source';source.mkdir()
    for path in release.FILES.values():
        target=source/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b'# test code\n')
    def git(*args):return subprocess.run(['git','-C',str(source),*args],capture_output=True,check=True)
    git('init');git('add','.');git('-c','user.name=Test','-c','user.email=test@example.invalid','commit','-m','fixture')
    target=root/'bundle';report=release.prepare(source,target,'2'*64)
    plan=release.plan(target,report['sha256'])
    assert plan['descriptor']['source_revision']==git('rev-parse','HEAD').stdout.decode().strip()
    assert not report['root_modified'] and not report['services_started']
    before={p.name:p.read_bytes() for p in target.iterdir()}
    with pytest.raises(ValueError,match='Preserving'):release.prepare(source,target,'2'*64)
    assert before=={p.name:p.read_bytes() for p in target.iterdir()}
    payload=target/next(iter(release.FILES));payload.chmod(0o644);payload.write_bytes(b'changed')
    with pytest.raises(ValueError):release.plan(target,report['sha256'])
    (source/next(iter(release.FILES.values()))).write_bytes(b'# dirty\n')
    with pytest.raises(ValueError,match='Commit'):release.prepare(source,root/'other','2'*64)
    assert not (root/'other').exists()


def test_installed_binding_checks_every_code_file_and_runtime_location(private_posix_path,monkeypatch):
    root=private_posix_path;monkeypatch.setattr(release,'ROOT',root);monkeypatch.setattr(release,'ROOT_UID',os.getuid())
    value=descriptor();identity=hashlib.sha256(release.wire(value)).hexdigest()
    folder=root/'backup-releases'/identity;folder.mkdir(parents=True)
    for name in release.FILES:(folder/name).write_bytes(b'x')
    (folder/'release.json').write_bytes(release.wire(value))
    python=root/'releases'/value['base_release']/'runtime/bin/python';python.parent.mkdir(parents=True);python.write_bytes(b'fixture')
    binding=dict(schema=1,producer=release.PRODUCER,identity=identity,base_release=value['base_release'],uid=600,gid=600,python=str(python))
    target=folder/'installation.json';target.write_bytes(release.wire(binding))
    assert release.installed(folder)==(value,binding)
    for change in ({'gid':True},{'python':str(root/'external-python')},{'uid':0},{'base_release':'4'*64},{'python':None}):
        target.write_bytes(release.wire(dict(binding,**change)))
        with pytest.raises(ValueError):release.installed(folder)
    target.write_bytes(release.wire(binding))
    payload=folder/next(iter(release.FILES));payload.write_bytes(b'y')
    with pytest.raises(ValueError,match='code changed'):release.installed(folder)
    payload.write_bytes(b'x');(folder/'foreign').write_bytes(b'preserve')
    with pytest.raises(ValueError,match='entries'):release.installed(folder)


@pytest.fixture
def worker():
    if os.name!='posix':pytest.skip('Real POSIX supervision')
    import mod_backup_service
    return mod_backup_service


def test_supervisor_captures_success_and_failure(worker,tmp_path):
    program=tmp_path/'child.py';program.write_text('import sys\nprint("result")\nprint("error",file=sys.stderr)\nsys.exit(7)\n')
    code,out,err=worker.capture(sys.executable,program,'--capture',timeout=5)
    assert code==7 and out==b'result\n' and err==b'error\n'


@pytest.mark.parametrize('behavior,exception',[
    ('import time\ntime.sleep(20)\n',TimeoutError),
    ('import sys\nsys.stdout.write("x"*20000)\nsys.stdout.flush()\n',ValueError),
    ('import sys\nsys.stderr.write("x"*20000)\nsys.stderr.flush()\n',ValueError)])
def test_supervisor_reaps_child_after_timeout_or_output_overflow(worker,tmp_path,monkeypatch,behavior,exception):
    program=tmp_path/'child.py';program.write_text(behavior);children=[];original=worker.subprocess.Popen
    def spawn(*args,**kwargs):
        child=original(*args,**kwargs);children.append(child);return child
    monkeypatch.setattr(worker.subprocess,'Popen',spawn)
    with pytest.raises(exception):worker.capture(sys.executable,program,'--capture',timeout=.5)
    assert len(children)==1 and children[0].poll() is not None
    with pytest.raises(ProcessLookupError):os.kill(children[0].pid,0)


def status():
    return dict(schema=1,producer='tsfp-backup-status-v1',identity='3'*64,base_release='2'*64,uid=600,time=100,status='ok',result={'status':'captured'})


def test_private_status_roundtrip_and_failure_preserves_unknown_bytes(worker,private_posix_path):
    parent=private_posix_path;value=status()
    worker.atomic_status(parent,value)
    assert worker.load_status(parent)==value
    assert (parent/'status.json').stat().st_mode&0o777==0o600
    changed=copy.deepcopy(value);changed['identity']='4'*64
    with pytest.raises(ValueError,match='differently bound'):worker.atomic_status(parent,changed)
    assert worker.load_status(parent)==value
    target=parent/'status.json';target.write_bytes(b'[]');before=target.read_bytes()
    with pytest.raises(ValueError):worker.atomic_status(parent,value)
    assert target.read_bytes()==before and not list(parent.glob('.status-*'))


def test_status_rejects_links_and_oversized_replacement(worker,private_posix_path):
    parent=private_posix_path;outside=parent/'original';outside.write_bytes(b'preserve')
    (parent/'status.json').symlink_to(outside)
    with pytest.raises(OSError):worker.atomic_status(parent,status())
    assert outside.read_bytes()==b'preserve'
    (parent/'status.json').unlink();value=status();value['result']={'payload':'x'*20000}
    with pytest.raises(ValueError,match='byte budget'):worker.atomic_status(parent,value)
    assert not (parent/'status.json').exists()
