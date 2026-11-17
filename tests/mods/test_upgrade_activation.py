"""Real proof retirement/journal/files; simulated scanner and launchd/process table."""
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
from types import SimpleNamespace

import pytest
from test_upgrade_layout import prepared
from test_upgrade_flow import flow

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover_journal as journal_code
import mod_host_upgrade_activation as activation
import mod_host_upgrade_process as process
from mod_package import ModError

pytestmark=pytest.mark.parametrize('prepared',['deferred'],indirect=True)


@pytest.fixture(autouse=True)
def simulated_boot(monkeypatch):
    monkeypatch.setattr(activation,'boot_identity',lambda:'f'*64)


@pytest.fixture
def running(flow,monkeypatch):
    item=flow;item.run('apply');loaded={};effects=[];scanner=[];mode=[None];foreign=[False]
    entries=item.case.new;by_path={str(path):(path,spec) for path,spec in entries}
    by_label={spec['Label']:(path,spec) for path,spec in entries}
    pids={label:100+index for index,label in enumerate(journal_code.start_order())}
    def command(argv):
        if argv[0]=='/bin/ps':
            output='1 0 1 0\n'
            for label in loaded:
                pid=pids[label];owner=0 if label.endswith('.monitor') else 600
                output+=f'{pid} 1 {pid} {owner}\n'
            if foreign[0]:output+='999 1 999 600\n'
            return 0,output
        if argv[1]=='bootstrap':
            path,spec=by_path[argv[-1]];label=spec['Label']
            assert item.state.state()['start_pending']==label
            assert not (item.case.root/'monitor/upgrade-quiescence.json').exists()
            effects.append(label)
            if mode[0] not in ('absent-error','timeout-absent'):loaded[label]=True
            if mode[0] in ('timeout-loaded','timeout-absent'):raise subprocess.TimeoutExpired(argv,3)
            return (1 if mode[0] in ('absent-error','loaded-error') else 0),'fixed utility output'
        label=argv[-1].removeprefix('system/')
        if label not in loaded:return 113,'Bad request.\nCould not find service "'+label+'" in domain for system\n'
        path,spec=by_label[label]
        output=('system/'+label+' = {\n\tpath = '+str(path)+'\n\ttype = LaunchDaemon\n'
                '\tprogram = '+spec['ProgramArguments'][0]+'\n\targuments = {\n'
                +''.join('\t\t'+argument+'\n' for argument in spec['ProgramArguments'])
                +'\t}\n\tpid = '+str(pids[label])+'\n}\n')
        return 0,output
    def scan(current,uid):
        scanner.append(current['identity'])
        return dict(schema=1,phase='scanner-admitted',uid=uid,release=current['identity'],version='ClamAV fixture',
            results=dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error'),
            temporary_state_removed=True,services_started=False,network_started=False)
    monkeypatch.setattr(process,'scanner_check',scan)
    current=dict(identity='b'*64,uid=600)
    def start(*,resume=False):
        return activation.start_private_services(item.state,current,item.case.exchange,entries,command,item.routing['invoke'],resume=resume)
    return SimpleNamespace(flow=item,loaded=loaded,effects=effects,scanner=scanner,mode=mode,foreign=foreign,
                           start=start,current=current,command=command,scan=scan)


def test_startup_loads_only_fixed_jobs_after_controls_and_blocks_public_restoration(running):
    case=running;item=case.flow;result=case.start()
    assert case.effects==journal_code.start_order() and case.scanner==['b'*64]
    assert result['readiness_verified'] is False and result['public_endpoint_started'] is False
    assert item.state.state()['started']==case.effects and not item.state.state()['quiescent']
    with pytest.raises(ModError):item.state.record('access-verified',data=dict(release='b'*64,sha256='9'*64))
    for kind in ('quiescent','quiescence-rechecked'):
        with pytest.raises(ModError):item.state.record(kind)
    count=len(case.effects);case.start(resume=True)
    assert len(case.effects)==count and case.scanner==['b'*64]


def test_readmission_renews_expired_scanner_controls_without_reloading_known_jobs(running):
    case=running;case.start();case.flow.now[0]+=301
    case.start(resume=True)
    assert case.scanner==['b'*64,'b'*64] and len(case.effects)==10


@pytest.mark.parametrize('mode',['loaded-error','timeout-loaded'])
def test_bootstrap_observation_governs_success_after_nonzero_or_timeout(running,mode):
    running.mode[0]=mode;running.start()
    assert len(running.loaded)==10 and running.flow.state.state()['start_pending'] is None


@pytest.mark.parametrize('mode',['absent-error','timeout-absent'])
def test_unloaded_pending_bootstrap_is_preserved_without_blind_retry(running,mode):
    case=running;case.mode[0]=mode
    with pytest.raises(ModError):case.start()
    assert case.flow.state.state()['start_pending']==journal_code.start_order()[0]
    with pytest.raises(ModError):case.start(resume=True)
    assert len(case.effects)==1 and not case.loaded


def test_lost_start_observation_resumes_known_loaded_effect_without_duplicate_bootstrap(running,monkeypatch):
    case=running;state=case.flow.state;original=state.record
    def record(kind,*args,**kwargs):
        if kind=='start-observed':raise OSError('lost observation commit')
        return original(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(state,'record',record)
        with pytest.raises(OSError):case.start()
    pending=state.state()['start_pending'];assert pending and case.effects==[pending]
    with pytest.raises(ModError):case.start()
    case.start(resume=True)
    assert len(case.effects)==len(set(case.effects))==10


@pytest.mark.parametrize('failure',['bad-result','leak'])
def test_scanner_rejection_or_leaked_descendant_cannot_start_jobs(running,monkeypatch,failure):
    case=running
    def leaked(*args):
        result=case.scan(*args)
        if failure=='leak':case.foreign[0]=True
        else:result['results']['blocked']='clean'
        return result
    monkeypatch.setattr(process,'scanner_check',leaked)
    with pytest.raises(ModError):case.start()
    assert case.effects==[] and case.flow.state.state()['scanner'] is None


def test_proof_reappearing_during_startup_preserves_pending_job_and_halts(running):
    case=running;original=case.command;item=case.flow
    def reappeared(argv):
        result=original(argv)
        if case.effects:
            proof=item.case.exchange.saved/'upgrade-quiescence.json';target=item.case.root/'monitor/upgrade-quiescence.json'
            target.write_bytes(proof.read_bytes());target.chmod(0o644)
        return result
    with pytest.raises(ValueError):activation.start_private_services(item.state,case.current,item.case.exchange,item.case.new,reappeared,item.routing['invoke'])
    assert len(case.effects)==1 and item.state.state()['start_pending']==case.effects[0]
    with pytest.raises(ValueError):case.start(resume=True)
    assert len(case.effects)==1


def test_private_health_updates_do_not_relax_fixed_policy_identity(running,monkeypatch):
    case=running;original=case.command;layout=case.flow.case.exchange
    def health_update(argv):
        result=original(argv)
        if len(case.effects)==1:
            path=case.flow.case.root/'monitor/api/health.json';path.write_bytes(b'{"new-health":true}');path.chmod(0o644)
        return result
    activation.start_private_services(case.flow.state,case.current,layout,case.flow.case.new,health_update,case.flow.routing['invoke'])
    path=case.flow.case.root/'monitor/api/monitor.json';data=path.read_bytes()
    path.rename(case.flow.case.root/'preserved-marker.json');path.write_bytes(data);path.chmod(0o600)
    with pytest.raises(ModError):case.start(resume=True)
    assert len(case.effects)==10


def test_retired_proof_is_archived_locked_and_known_missing_original_is_recognized(flow):
    item=flow;item.run('apply');item.run('rollback')
    path=item.case.root/'monitor/upgrade-quiescence.json';original=path.read_bytes()
    sha=activation.retire_proof(item.state,item.case.exchange,item.gate)
    archive=activation.archive_path(item.case.exchange.folder,'before',sha)
    assert archive.read_bytes()==original and stat.S_IMODE(archive.stat().st_mode)==0o600 and not path.exists()
    assert sha==hashlib.sha256(original).hexdigest() and item.case.exchange._locations()['active']=='before'


def test_foreign_retirement_destination_is_preserved_without_removing_active_authority(flow):
    item=flow;item.run('apply');item.run('rollback')
    path=item.case.root/'monitor/upgrade-quiescence.json';original=path.read_bytes();sha=hashlib.sha256(original).hexdigest()
    archive=activation.archive_path(item.case.exchange.folder,'before',sha);archive.write_bytes(b'foreign');archive.chmod(0o600)
    with pytest.raises(ValueError):activation.retire_proof(item.state,item.case.exchange,item.gate)
    assert path.read_bytes()==original and archive.read_bytes()==b'foreign'
    assert item.state.state()['retire_pending']==dict(generation='before',sha256=sha)


@pytest.mark.parametrize('target',['folder','store'])
def test_retirement_refuses_changed_private_storage_mode_without_consuming_proof(flow,target):
    item=flow;item.run('apply');item.run('rollback')
    path=item.case.root/'monitor/upgrade-quiescence.json';original=path.read_bytes()
    directory=item.case.exchange.folder if target=='folder' else item.case.exchange.folder.parent
    directory.chmod(0o755)
    with pytest.raises(ModError):activation.retire_proof(item.state,item.case.exchange,item.gate)
    assert path.read_bytes()==original and item.state.state()['retire_pending'] is None


@pytest.mark.parametrize('failure',['chmod','observation'])
def test_retirement_interruption_preserves_known_archive_and_requires_explicit_resume(flow,monkeypatch,failure):
    item=flow;item.run('apply');item.run('rollback');state=item.state
    path=item.case.root/'monitor/upgrade-quiescence.json';original=path.read_bytes();real_record=state.record;real_chmod=os.fchmod
    def chmod(fd,mode):
        if stat.S_IMODE(os.fstat(fd).st_mode)==0o644:raise OSError('interrupted tightening')
        return real_chmod(fd,mode)
    def record(kind,*args,**kwargs):
        if kind=='proof-retire-observed':raise OSError('lost observation')
        return real_record(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        if failure=='chmod':patch.setattr(activation.os,'fchmod',chmod)
        else:patch.setattr(state,'record',record)
        with pytest.raises(OSError):activation.retire_proof(state,item.case.exchange,item.gate)
    pending=state.state()['retire_pending'];assert pending and not path.exists()
    with pytest.raises(ModError):activation.retire_proof(state,item.case.exchange,item.gate)
    activation.retire_proof(state,item.case.exchange,item.gate,resume=True)
    archive=activation.archive_path(item.case.exchange.folder,'before',pending['sha256'])
    assert archive.read_bytes()==original and stat.S_IMODE(archive.stat().st_mode)==0o600
    assert state.state()['retire_pending'] is None
    assert sum(event['kind']=='proof-retire-intent' for event in json.loads(state.current)['events'])==1
