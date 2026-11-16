"""Fixed registration, preflight, private-first boot persistence and owned rollback."""
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import sys
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mods'))
import mod_system_host as roles
from mod_package import ModError


def load(name):
    spec=importlib.util.spec_from_file_location('test_'+name,ROOT/'tools/mods/hosting'/name)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result


register=load('register-installed-host-mac.py')


def test_nine_jobs_are_derived_from_fixed_generators_with_monitors_first():
    release=register.ROOT/'releases'/('a'*64)
    jobs=register.jobs(release,dict(uid=600,gid=600),release/'runtime/python')
    assert len(jobs)==9 and len({v['Label'] for _,v in jobs})==9
    assert all(value['Label'].endswith('.monitor') for _,value in jobs[:4])
    assert [v['ProgramArguments'][-1] for _,v in jobs[4:7]]==list(register.ROLES)
    assert all(v['UserName']=='root' and v['AbandonProcessGroup'] is False for _,v in jobs)


@pytest.fixture
def deployment(tmp_path,monkeypatch):
    if os.name!='posix':pytest.skip('Actual POSIX protected file/atomic journal/owned rollback layer')
    tmp_path.chmod(0o700)
    root=tmp_path/'host';root.mkdir(mode=0o700)
    launchd=tmp_path/'launchd';launchd.mkdir(mode=0o700)
    monkeypatch.setattr(register,'ROOT',root);monkeypatch.setattr(register,'LAUNCHD',launchd)
    monkeypatch.setattr(register,'ROOT_UID',os.geteuid());monkeypatch.setattr(roles,'ROOT',root)
    scan=load('system-scan-monitor-mac.py');host=load('system-host-monitor-mac.py')
    monkeypatch.setattr(scan,'ROOT',root);monkeypatch.setattr(host,'ROOT',root)
    # Root privilege/ancestor policy has separate tests and native acceptance.
    # Substitute only that filesystem owner seam for ordinary-user unit checks.
    monkeypatch.setattr(scan,'root_directory',lambda p:os.open(p,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW))
    monkeypatch.setattr(scan,'ROOT_UID',os.geteuid())
    monkeypatch.setattr(register,'helper',lambda name:scan if name=='system-scan-monitor-mac.py' else host)
    release=root/'releases'/('a'*64);account=dict(uid=600,gid=600);python=release/'runtime/python'
    entries=register.jobs(release,account,python)
    for path,value in entries:
        path.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
        path.write_bytes(plistlib.dumps(value));path.chmod(0o600)
    proof=dict(origin='https://workshop.example.test:8443',trusted_root_sha256='b'*64)
    monkeypatch.setattr(register,'admission',lambda *_:proof)
    state=dict(loaded=set(),calls=[],fail_at=None,uncertain=False,ready=True,checks=0)
    label_by_path={str(path):v['Label'] for path,v in entries}
    def command(args):
        state['calls'].append(args)
        assert args[0]=='/bin/launchctl'
        if args[1]=='print':return (0,'pid = 456') if args[2].split('/',1)[1] in state['loaded'] else (113,'not loaded')
        if args[1]=='bootstrap':
            assert args[2]=='system' and args[3] in label_by_path
            label=label_by_path[args[3]]
            if len(state['loaded'])==state['fail_at']:
                if state['uncertain']:state['loaded'].add(label)
                return 1,'failed'
            state['loaded'].add(label);return 0,''
        assert args[1]=='bootout' and args[2].startswith('system/')
        state['loaded'].remove(args[2].split('/',1)[1]);return 0,''
    def ready(*_):
        state['checks']+=1
        assert not list(launchd.iterdir()),'Boot files were installed before private readiness'
        if not state['ready']:raise ModError('private admission failed')
        return dict(proof,phase='private-services-verified')
    monkeypatch.setattr(register,'private_ready',ready)
    def lifecycle(*args):
        assert not list(launchd.iterdir()),'Boot files appeared before restart checks'
        return [dict(verified='fixture-only')],args[4]
    monkeypatch.setattr(register,'lifecycle',lifecycle)
    return SimpleNamespace(root=root,launchd=launchd,release=release,account=account,python=python,
                           entries=entries,state=state,command=command)


def test_success_persists_only_after_private_readiness(deployment):
    d=deployment
    result=register.register(d.release,d.account,d.python,d.command)
    assert result['phase']=='private-jobs-registered' and len(d.state['loaded'])==9 and d.state['checks']==1
    assert result['public_endpoint_started'] is False
    for _,value in d.entries:
        path=d.launchd/(value['Label']+'.plist')
        assert path.read_bytes()==plistlib.dumps(value) and path.stat().st_mode&0o777==0o644
    reports=list((d.root/'monitor/deployments').glob('deployment-*.json'))
    assert len(reports)==1 and reports[0].stat().st_mode&0o777==0o600
    assert json.loads(reports[0].read_bytes())['phase']=='complete'


@pytest.mark.parametrize('fault',['file','label','source'])
def test_preexisting_or_changed_items_are_refused_without_side_effects(deployment,fault):
    d=deployment;path,value=d.entries[0];label=value['Label']
    if fault=='file':(d.launchd/(label+'.plist')).write_bytes(b'preserve existing file')
    if fault=='label':d.state['loaded'].add(label)
    if fault=='source':path.write_bytes(b'preserve changed source')
    before={p.name:p.read_bytes() for p in d.launchd.iterdir()}
    with pytest.raises(ModError):register.register(d.release,d.account,d.python,d.command)
    assert before=={p.name:p.read_bytes() for p in d.launchd.iterdir()}
    assert not any(args[1] in ('bootstrap','bootout') for args in d.state['calls'])


@pytest.mark.parametrize('fault',['bootstrap','readiness','copy','uncertain-bootstrap'])
def test_failures_unload_only_confirmed_jobs_and_remove_only_created_files(deployment,monkeypatch,fault):
    d=deployment
    if fault.endswith('bootstrap'):
        d.state['fail_at']=4;d.state['uncertain']=fault=='uncertain-bootstrap'
    if fault=='readiness':d.state['ready']=False
    if fault=='copy':monkeypatch.setattr(register.os,'fchmod',lambda *_:(_ for _ in ()).throw(OSError('injected disk failure')))
    with pytest.raises((OSError,ModError)):register.register(d.release,d.account,d.python,d.command)
    assert not list(d.launchd.iterdir())
    assert len(d.state['loaded'])==(1 if fault=='uncertain-bootstrap' else 0)
    receipt=json.loads(next((d.root/'monitor/deployments').glob('deployment-*.json')).read_bytes())
    assert receipt['phase']=='failed' and receipt['rollback_complete'] is (fault!='uncertain-bootstrap')
    confirmed=[a[3] for a in d.state['calls'] if a[1]=='bootstrap']
    stopped=[a[2].split('/',1)[1] for a in d.state['calls'] if a[1]=='bootout']
    assert set(stopped)<={v['Label'] for _,v in d.entries}
    assert len(stopped)<=len(confirmed)


def test_changed_created_inode_is_preserved_and_cleanup_is_not_claimed(deployment,monkeypatch):
    d=deployment
    def tamper(fd,mode):
        os.lseek(fd,0,os.SEEK_SET);os.write(fd,b'foreign edited bytes')
        raise OSError('foreign change after creation')
    monkeypatch.setattr(register.os,'fchmod',tamper)
    with pytest.raises(ModError,match='incomplete cleanup'):register.register(d.release,d.account,d.python,d.command)
    assert not d.state['loaded']
    assert len(list(d.launchd.iterdir()))==1
    assert next(d.launchd.iterdir()).read_bytes().startswith(b'foreign edited bytes')


@pytest.mark.parametrize('fault',['stale','future','uid','release','pin','scanner','boolean'])
def test_registration_requires_fresh_protected_admission(monkeypatch,fault):
    release=register.ROOT/'releases'/('a'*64);profile=dict(origin='https://workshop.example.test',trusted_root_sha256='b'*64)
    scanner=dict(phase='scanner-admitted',release=release.name,uid=600,temporary_state_removed=True,services_started=False,network_started=False,
                 results=dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error'))
    value=dict(schema=1,producer='tsfp-private-host-admission-v1',release=str(release),uid=600,time=1000,scanner=scanner,
               services_started=False,public_endpoint_started=False,**profile)
    if fault=='stale':value['time']=699
    if fault=='future':value['time']=1001
    if fault=='uid':value['uid']=501
    if fault=='release':value['release']='/other'
    if fault=='pin':value['trusted_root_sha256']='c'*64
    if fault=='scanner':scanner['results']['blocked']='clean'
    if fault=='boolean':value['schema']=True
    monkeypatch.setattr(register,'helper',lambda _:SimpleNamespace(root_json=lambda directory,name:profile if name=='public-workshop.json' else value))
    with pytest.raises(ModError):register.admission(release,dict(uid=600),clock=lambda:1000)


@pytest.mark.parametrize('fault',[None,'owner','surviving-child','same-pid','rollback'])
def test_restarts_verify_new_identity_old_tree_exit_and_catalog_continuity(monkeypatch,fault):
    release=register.ROOT/'releases'/('a'*64);account=dict(uid=600,gid=600);python=release/'runtime/python'
    entries=register.jobs(release,account,python)
    pids={v['Label']:200+i for i,(_,v) in enumerate(entries)}
    owners={v['Label']:0 if v['Label'].endswith('.monitor') else 600 for _,v in entries}
    original=dict(pids);survivors=[];killed=[]
    if fault=='owner':owners[entries[4][1]['Label']]=501
    def command(args):
        if args[0]=='/bin/ps':
            rows=[]
            for label,pid in pids.items():
                rows.extend((f'{pid} 1 {owners[label]}',f'{pid+10000} {pid} {owners[label]}'))
            rows.extend(f'{pid} 1 600' for pid in survivors)
            return 0,'\n'.join(rows)
        assert args[:2] in (['/bin/launchctl','print'],['/bin/launchctl','kill'])
        label=args[-1].split('/',1)[1]
        assert label in pids
        if args[1]=='print':return 0,f'pid = {pids[label]}'
        assert args[2]=='SIGKILL'
        killed.append(label)
        old=pids[label]
        if fault=='surviving-child':survivors.append(old+10000)
        if fault!='same-pid':pids[label]=old+1000
        return 0,''
    initial=dict(origin='https://workshop.example.test',trusted_root_sha256='b'*64,catalog_version=1)
    monkeypatch.setattr(register,'private_ready',lambda *_:dict(initial,catalog_version=0 if fault=='rollback' else 2))
    clock=iter([0,200]).__next__ if fault in ('surviving-child','same-pid') else lambda:0
    if fault is None:
        results,accepted=register.lifecycle(release,account,python,entries,initial,command,clock=clock,sleep=lambda _:None)
        assert len(results)==7 and all(v['old_tree_exited'] and v['old_pid']!=v['new_pid'] for v in results)
        assert accepted['catalog_version']==2
        assert len(killed)==7 and len(set(killed))==7
        assert all(pids[label]!=original[label] for label in killed)
    else:
        with pytest.raises(ModError):register.lifecycle(release,account,python,entries,initial,command,clock=clock,sleep=lambda _:None)
        if fault=='owner':assert not killed


def test_private_readiness_runs_only_after_explicit_credential_drop(monkeypatch):
    release=register.ROOT/'releases'/('a'*64)
    value=dict(schema=1,phase='private-services-verified',uid=600,release=release.name,role_health_verified=True,
               catalog_version=1,release_count=0,transport='private-loopback-http',public_endpoint_verified=False)
    def run(args,**options):
        assert args[1:3]==['-I','-B'] and Path(args[-1]).name=='accept-private-host-services-mac.py'
        assert options['user']==options['group']==600 and options['extra_groups']==[]
        assert set(options['env'])=={'PATH','HOME','TMPDIR','LC_ALL','TZ'}
        return SimpleNamespace(returncode=0,stdout=json.dumps(value).encode(),stderr=b'')
    monkeypatch.setattr(register.subprocess,'run',run)
    assert register.private_ready(release,dict(uid=600,gid=600),Path('/python'))==value
    value['catalog_version']=True
    with pytest.raises(ModError):register.private_ready(release,dict(uid=600,gid=600),Path('/python'))
