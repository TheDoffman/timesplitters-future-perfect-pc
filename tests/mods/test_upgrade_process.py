"""Bounded real child transport and typed installed upgrade protocols."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest

SOURCE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SOURCE/'tools/mods'))
import mod_host_upgrade_process as process
from mod_package import ModError


@pytest.mark.parametrize('case', ['success','stderr','nonzero','stdout-overflow','stderr-overflow','timeout','selector-failed'])
def test_real_owned_child_is_reaped_after_transport_success_and_failures(private_posix_path,monkeypatch,case):
    programs=dict(success="print('ok')",stderr="import sys;sys.stderr.write('private-canary')",
                  nonzero='raise SystemExit(3)',**{'stdout-overflow':"print('x'*100000)",
                  'stderr-overflow':"import sys;sys.stderr.write('x'*100000)",
                  'timeout':'import time;time.sleep(30)','selector-failed':'import time;time.sleep(30)'})
    real=process.subprocess.Popen;children=[]
    def spawn(*args,**kwargs):
        child=real(*args,**kwargs);children.append(child);return child
    monkeypatch.setattr(process.subprocess,'Popen',spawn)
    if case=='selector-failed':
        monkeypatch.setattr(process.selectors,'DefaultSelector',lambda:(_ for _ in ()).throw(OSError('selector unavailable')))
    arguments=[sys.executable,'-I','-S','-B','-c',programs[case]]
    if case=='success':assert process._capture(arguments,{},timeout=3,limit=64)==b'ok\n'
    else:
        with pytest.raises((ModError,OSError)) as error:process._capture(arguments,{},timeout=.15 if case=='timeout' else 3,limit=64)
        assert 'private-canary' not in str(error.value)
    assert len(children)==1 and children[0].poll() is not None
    assert children[0].stdout.closed and children[0].stderr.closed


@pytest.fixture
def verified(monkeypatch):
    monkeypatch.setattr(process.sys,'platform','darwin')
    monkeypatch.setattr(process.os,'geteuid',lambda:process.ROOT_UID,raising=False)
    def entry(identity):
        root=process.ROOT/'releases'/identity
        return dict(root=root,identity=identity,uid=600,python=root/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12',
                    descriptor=dict(source_files={name:{} for name in ('tools/mods/hosting/upgrade-job-specs-mac.py',
                                                                        'tools/mods/hosting/force-upgrade-backup-mac.py')}))
    return entry('a'*64),entry('b'*64)


def job_report(runtime):
    spec=importlib.util.spec_from_file_location('upgrade_process_factory_test',SOURCE/'tools/mods/hosting/register-installed-host-mac.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    entries=module.jobs(runtime['root'],dict(uid=600,gid=600),runtime['python'])
    return dict(schema=1,producer='tsfp-upgrade-job-specs-v1',kind='host',identity=runtime['identity'],
                base_release=runtime['identity'],uid=600,gid=600,kernel_groups=[],
                entries=[dict(generated=str(path),specification=value) for path,value in entries])


@pytest.mark.parametrize('damage', [None,'schema','uid','groups','identity','base','kind','duplicate','argv','unknown'])
def test_job_protocol_checks_binding_labels_credentials_and_fixed_launch_arguments(verified,monkeypatch,damage):
    runtime,current=verified;report=job_report(runtime)
    if damage=='schema':report['schema']=True
    elif damage=='uid':report['uid']=True
    elif damage=='groups':report['kernel_groups']=[0]
    elif damage=='identity':report['identity']='c'*64
    elif damage=='base':report['base_release']='c'*64
    elif damage=='kind':report['kind']='backup'
    elif damage=='duplicate':report['entries'][-1]=report['entries'][0]
    elif damage=='argv':report['entries'][0]['specification']['ProgramArguments'][0]='/foreign/python'
    elif damage=='unknown':report['other']=False
    def capture(arguments,options,*,timeout,limit):
        assert arguments[:5]==[str(runtime['python']),'-I','-S','-B',str(current['root']/'source/tools/mods/hosting/upgrade-job-specs-mac.py')]
        assert arguments[5:]==['--host-release',runtime['identity']]
        assert options['user']==options['group']==600 and options['extra_groups']==[]
        assert options['env']==dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(process.ROOT/'data'),TMPDIR=str(process.ROOT/'data'),LC_ALL='C',TZ='UTC')
        assert timeout==15 and limit==65536
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.job_specs(runtime,current,600)
    else:assert len(process.job_specs(runtime,current,600))==9


@pytest.mark.parametrize('damage', [None,'not-due','unverified','before-quiescence','future','uid','base','hash','name','unknown'])
def test_forced_backup_protocol_requires_new_verified_capture_after_quiescence(verified,monkeypatch,damage):
    runtime,current=verified;now=int(time.time());identity='c'*64
    report=dict(schema=1,producer='tsfp-upgrade-backup-v1',identity=identity,base_release=runtime['identity'],uid=600,
                status='captured',backup='snapshot-'+('d'*32)+'-'+str(now)+'-00000001',
                verified_manifest_sha256='e'*64,created=now-1,started=now-2,finished=now,bytes=100,forced=True,verified=True)
    if damage=='not-due':report['status']='not-due'
    elif damage=='unverified':report['verified']=False
    elif damage=='before-quiescence':report.update(started=now-10,created=now-9)
    elif damage=='future':report['finished']=now+3600
    elif damage=='uid':report['uid']=601
    elif damage=='base':report['base_release']='f'*64
    elif damage=='hash':report['verified_manifest_sha256']='E'*64
    elif damage=='name':report['backup']='snapshot-../private'
    elif damage=='unknown':report['other']=True
    def capture(arguments,options,*,timeout,limit):
        assert arguments[-2:]==['--backup-release',identity] and '--capture' in arguments
        assert timeout==1800 and limit==8192 and options['user']==600
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.forced_backup(runtime,current,600,identity,quiesced_at=now-3)
    else:assert process.forced_backup(runtime,current,600,identity,quiesced_at=now-3)['forced']


def test_child_context_refuses_unprivileged_invocation_before_spawn(verified,monkeypatch):
    runtime,current=verified
    monkeypatch.setattr(process.os,'geteuid',lambda:501,raising=False)
    monkeypatch.setattr(process,'_capture',lambda *args,**kwargs:pytest.fail('Spawned before root admission'))
    with pytest.raises(ModError):process.job_specs(runtime,current,600)


def test_backup_factory_uses_its_independent_retained_runtime(verified,monkeypatch):
    import mod_backup_release as backup
    runtime,current=verified;identity='c'*64;folder=process.ROOT/'backup-releases'/identity
    binding=dict(identity=identity,python=str(runtime['python']))
    report=dict(schema=1,producer='tsfp-upgrade-job-specs-v1',kind='backup',identity=identity,
                base_release=runtime['identity'],uid=600,gid=600,kernel_groups=[600],
                entries=[dict(generated=str(process.ROOT/'backup-jobs'/identity/'worker.plist'),
                              specification=backup.job(folder,binding))])
    def capture(arguments,options,*,timeout,limit):
        assert arguments[0]==str(runtime['python']) and arguments[-2:]==['--backup-release',identity]
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    assert process.job_specs(runtime,current,600,backup_identity=identity)[0][1]['Label']=='org.timesplitters.workshop.backup'


def test_preflight_verifies_independent_backup_binding_and_combines_only_attested_job_metadata(monkeypatch):
    from types import SimpleNamespace
    spec=importlib.util.spec_from_file_location('upgrade_preflight_composition',SOURCE/'tools/mods/hosting/upgrade-installed-host-mac.py')
    upgrade=importlib.util.module_from_spec(spec);spec.loader.exec_module(upgrade)
    before,after,retained,backup_id='a'*64,'b'*64,'c'*64,'d'*64
    def entry(identity):return dict(identity=identity,root=process.ROOT/'releases'/identity,uid=600,
        python=process.ROOT/'releases'/identity/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12')
    old,new,backup_runtime=entry(before),entry(after),entry(retained)
    new['descriptor']=dict(source_files={key:{} for key in ('tools/mods/mod_host_upgrade_process.py',
        'tools/mods/mod_host_cutover.py','tools/mods/mod_backup_release.py','tools/mods/hosting/upgrade-job-specs-mac.py')})
    profile=dict(origin='https://workshop.example.test:8443',trusted_root_sha256='f'*64)
    monkeypatch.setattr(upgrade,'verified_pair',lambda *args:(old,new,profile))
    monkeypatch.setattr(upgrade.release,'installed',lambda identity,uid:backup_runtime if identity==retained and uid==600 else pytest.fail('Unexpected retained runtime'))
    monkeypatch.setattr(upgrade.release,'INSTALLED_ROOT',process.ROOT)
    monkeypatch.setattr(sys,'path',list(sys.path))
    calls=[]
    def job_specs(runtime,current,uid,**kwargs):
        assert current is new and uid==600
        calls.append((runtime['identity'],kwargs))
        return ['host'] if runtime is old else ['backup']
    def observed(entries,uid,command):
        assert entries==['host','backup'] and uid==600
        return dict(files={'only':'attested'},observation=dict(tracked=dict(pids=[123],groups=[123])))
    monkeypatch.setitem(sys.modules,'mod_host_upgrade_process',SimpleNamespace(job_specs=job_specs))
    monkeypatch.setitem(sys.modules,'mod_host_cutover',SimpleNamespace(preflight=observed))
    binding=dict(uid=600,gid=600,base_release=retained,python=str(backup_runtime['python']))
    monkeypatch.setitem(sys.modules,'mod_backup_release',SimpleNamespace(installed=lambda folder:({},binding)))
    monkeypatch.setitem(sys.modules,'mod_host_watch',SimpleNamespace(run_command=lambda _:pytest.fail('Unexpected real command')))
    report=upgrade.preflight(before,after,600,backup_id)
    assert calls==[(before,{}),(retained,dict(backup_identity=backup_id))]
    assert report['jobs']==10 and report['protected_job_files']==20 and report['backup_base_release']==retained
    assert len(report['preflight_sha256'])==64 and not report['data_written'] and not report['services_changed']


def test_job_reader_default_plan_and_ordinary_active_refusal(monkeypatch):
    path=SOURCE/'tools/mods/hosting/upgrade-job-specs-mac.py'
    result=subprocess.run([sys.executable,'-I','-S','-B',str(path)],capture_output=True,check=True,timeout=10)
    assert json.loads(result.stdout)==dict(schema=1,phase='plan-only',services_changed=False,data_written=False)
    spec=importlib.util.spec_from_file_location('job_reader_guard_test',path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    monkeypatch.setattr(module.sys,'platform','darwin');monkeypatch.setattr(module.os,'geteuid',lambda:501,raising=False)
    monkeypatch.setattr(module,'protected_program',lambda _:pytest.fail('Read protected code before identity admission'))
    with pytest.raises(ValueError,match='isolated locked'):module.active('host','a'*64)
