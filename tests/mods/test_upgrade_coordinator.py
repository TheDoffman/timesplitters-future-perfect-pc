"""Composed real journal/files/proofs; credentials, launchd and children simulated."""
import copy
import json
import plistlib
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest
from test_upgrade_layout import prepared
from test_upgrade_flow import flow
from test_upgrade_activation import simulated_boot

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_routing as routes
import mod_host_upgrade_coordinator as coordinator
import mod_host_upgrade_process as process
import mod_host_upgrade_shutdown as shutdown
from mod_package import ModError

pytestmark=[pytest.mark.parametrize('prepared',['deferred'],indirect=True),
            pytest.mark.parametrize('flow',['fresh'],indirect=True)]


@pytest.fixture
def machine(flow,monkeypatch,public_upgrade_report):
    item=flow;state=item.state;old=item.case.old;new=item.case.new
    monkeypatch.setattr(shutdown,'boot_identity',lambda:'f'*64)
    loaded={spec['Label']:(path,spec,100+index) for index,(path,spec) in enumerate(old)}
    stops=[];starts=[];kills=[];captures=[];scans=[]
    def command(argv):
        if argv[0]=='/bin/ps':
            return 0,'1 0 1 0\n'+''.join(f'{pid} 1 {pid} {0 if label.endswith(".monitor") else 600}\n'
                for label,(_,_,pid) in loaded.items())
        if argv[1]=='bootstrap':
            path=Path(argv[-1]);spec=plistlib.loads(path.read_bytes());label=spec['Label']
            assert state.state()['start_pending']==label
            assert not (item.case.root/'monitor/upgrade-quiescence.json').exists()
            starts.append(label);loaded[label]=(path,spec,200+len(starts))
            return 0,''
        label=argv[-1].removeprefix('system/')
        if argv[1]=='bootout':
            assert label in (state.state()['pending'],state.state()['shutdown_pending'])
            stops.append(label);del loaded[label];return 0,''
        if argv[1]=='kill':
            assert state.state()['restart_pending']['label']==label
            kills.append(label);path,spec,pid=loaded[label];loaded[label]=(path,spec,pid+1000)
            return 0,''
        if label not in loaded:return 113,'Bad request.\nCould not find service "'+label+'" in domain for system\n'
        path,spec,pid=loaded[label]
        return 0,('system/'+label+' = {\n\tpath = '+str(path)+'\n\ttype = LaunchDaemon\n'
            '\tprogram = '+spec['ProgramArguments'][0]+'\n\targuments = {\n'
            +''.join('\t\t'+argument+'\n' for argument in spec['ProgramArguments'])
            +'\t}\n\tpid = '+str(pid)+'\n}\n')
    def backup(*args,**kwargs):
        assert not loaded and kwargs['quiesced_at']==state.state()['quiesced_at']
        captures.append(True);now=item.now[0]
        return dict(schema=1,producer='tsfp-upgrade-backup-v1',identity='8'*64,base_release='7'*64,uid=600,
            status='captured',backup='snapshot-'+('d'*32)+'-'+str(now)+'-00000001',
            verified_manifest_sha256=format(len(captures),'064x'),created=now,started=now,finished=now,
            bytes=100,forced=True,verified=True)
    def scanner(current,uid):
        assert not loaded or state.state()['activation_ready']
        scans.append(current['identity'])
        return dict(schema=1,phase='scanner-admitted',uid=uid,release=current['identity'],version='ClamAV fixture',
            results=dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error'),
            temporary_state_removed=True,services_started=False,network_started=False)
    profile={key:item.baseline[key] for key in ('origin','trusted_root_sha256')}
    def ready(current,uid,authority,*,floor):
        assert len(loaded)==10 and floor<=42
        return dict(item.baseline,release=current['identity'],catalog_version=42)
    monkeypatch.setattr(process,'forced_backup',backup)
    monkeypatch.setattr(process,'scanner_check',scanner)
    monkeypatch.setattr(process,'private_readiness',ready)
    def public(current,uid,authority,*,floor,host_release):
        assert current['identity']=='b'*64 and host_release==state.base[state.state()['config']]
        assert uid==600 and authority==profile and floor==state.state()['catalog_floor']
        assert routes.snapshot(item.routing['invoke']('status'),profile['origin'])['phase']=='public'
        return public_upgrade_report(state)
    monkeypatch.setattr(process,'public_readiness',public)
    root=item.case.root
    def release(identity):return dict(identity=identity,uid=600,root=root/'releases'/identity,python=root/identity/'python')
    old_release,new_release,runtime=release('a'*64),release('b'*64),release('7'*64)
    binding=dict(uid=600,gid=600,base_release='7'*64,python=str(runtime['python']))
    report={key:state.base[key] for key in ('before','after','uid','backup_identity','backup_base_release','plan_sha256','preflight_sha256')}
    context=dict(old=old_release,new=new_release,profile=profile,backup_runtime=runtime,backup_binding=binding,
        operator=item.routing['invoke'],command=command,entries=old,candidate_entries=new,plan=item.plan,
        private_baseline=item.baseline,observed=dict(observation=dict(tracked=state.base['tracked'])),
        routing=state.base['routing'],report=report)
    return SimpleNamespace(flow=item,state=state,context=context,loaded=loaded,stops=stops,starts=starts,
        kills=kills,captures=captures,scans=scans)


def test_composed_upgrade_captures_before_migration_and_stays_private_until_separate_restoration(machine):
    item=machine;result=coordinator.apply_private(item.state,item.context)
    assert result['generation']=='after' and result['catalog_version']==42 and result['restarts']==7
    assert result['public_endpoint_started'] is False
    assert item.stops==storage.stop_order() and item.starts==storage.start_order() and item.kills==storage.restart_order()
    assert len(item.captures)==1 and item.scans==['b'*64] and item.flow.private==['after']
    item.state.require_suspended(item.context['operator'])
    assert (item.flow.case.root/'monitor/deployments').exists() is False
    assert (item.flow.case.root/'upgrade-layouts'/('c'*64)/'monitor-before/deployments/old-receipt.json').exists()
    with pytest.raises(ModError):coordinator.apply_private(item.state,item.context)
    # Fresh admission can be resumed without another capture, stop/start or kill.
    coordinator.apply_private(item.state,item.context,resume=True)
    assert len(item.captures)==1 and len(item.stops)==len(item.starts)==10 and len(item.kills)==7
    result=coordinator.restore_public(item.state,item.context)
    assert result['generation']=='after' and result['public_endpoint_verified'] and item.state.state()['settled']


@pytest.mark.parametrize('lost',['routing-observed','public-readiness-verified','settled'])
@pytest.mark.parametrize('reverse',[False,True])
def test_expired_interrupted_restoration_is_reobserved_without_repeating_routes_or_jobs(machine,monkeypatch,lost,reverse):
    item=machine;coordinator.apply_private(item.state,item.context)
    if reverse:coordinator.rollback_private(item.state,item.context)
    actions=[];original=item.context['operator'];record=item.state.record;failed=[]
    def invoke(action):
        if action!='status':actions.append(action)
        return original(action)
    item.context['operator']=invoke
    def interrupt(event,*args,**kwargs):
        if event==lost and not failed:failed.append(True);raise OSError('lost observation')
        return record(event,*args,**kwargs)
    counts=(len(item.stops),len(item.starts),len(item.kills),len(item.captures))
    with monkeypatch.context() as patch:
        patch.setattr(item.state,'record',interrupt)
        with pytest.raises(OSError):coordinator.restore_public(item.state,item.context)
    assert actions==['restore'] and failed and not item.state.state()['settled']
    assert item.state.state()['routing_pending'] is (True if lost=='routing-observed' else None)
    with pytest.raises(ModError):coordinator.restore_public(item.state,item.context)
    item.flow.now[0]+=1000
    result=coordinator.restore_public(item.state,item.context,resume=True)
    assert result['generation']==('before' if reverse else 'after') and item.state.state()['settled']
    assert actions==['restore'] and counts==(len(item.stops),len(item.starts),len(item.kills),len(item.captures))


@pytest.mark.parametrize('fault',['before-activation','unrecorded-public','unrelated-route','bad-public-report','stale-scan'])
def test_restoration_preserves_unadmitted_or_changed_state(machine,monkeypatch,fault):
    item=machine
    if fault!='before-activation':coordinator.apply_private(item.state,item.context)
    if fault=='unrecorded-public':item.context['operator']('restore')
    elif fault=='unrelated-route':item.flow.routing['value']['TCP']['1234']={'HTTPS':True}
    elif fault=='bad-public-report':monkeypatch.setattr(process,'public_readiness',lambda *a,**k:{})
    elif fault=='stale-scan':
        original=process.private_readiness
        def private(*args,**kwargs):
            result=original(*args,**kwargs);item.flow.now[0]+=301;return result
        monkeypatch.setattr(process,'private_readiness',private)
    with pytest.raises((ModError,ValueError)):coordinator.restore_public(item.state,item.context)
    assert not item.state.state()['settled']


@pytest.mark.parametrize('reverse',[False,True])
def test_replayed_restoration_requires_exact_fresh_private_and_public_evidence(machine,reverse):
    item=machine;coordinator.apply_private(item.state,item.context)
    if reverse:coordinator.rollback_private(item.state,item.context)
    coordinator.restore_public(item.state,item.context)
    events=json.loads(item.state.current)['events'];base=item.state.base
    refresh=next(i for i,event in enumerate(events) if event['kind']=='restoration-readiness-verified')
    public=next(i for i,event in enumerate(events) if event['kind']=='public-readiness-verified')
    def replay(value):
        return storage.replay(value,lambda:item.flow.now[0]+1000,base['routing'],base['before'],base['after'],base['uid'],
            base['backup_identity'],base['backup_base_release'],base['plan_sha256'])
    assert replay(events)['settled']
    for fault in ('scanner-uid','scanner-time','private-time','future','order','route','extra',
                  'public-origin','public-label','public-floor','public-health','no-public','stale-public'):
        changed=copy.deepcopy(events);data=changed[refresh]['data'];evidence=changed[public]
        if fault=='scanner-uid':data['scanner']['uid']=True
        elif fault=='scanner-time':data['scanner_checked_at']=True
        elif fault=='private-time':data['private_checked_at']=True
        elif fault=='future':data['private_checked_at']=changed[refresh]['time']+1
        elif fault=='order':data['private_checked_at']-=1
        elif fault=='route':
            data['routing']['phase']='public';data['routing']['config_sha256']=base['routing']['config_sha256']
        elif fault=='extra':data['private_config']='canary'
        elif fault=='public-origin':evidence['data']['origin']='https://foreign.example'
        elif fault=='public-label':evidence['label']='after' if reverse else 'before'
        elif fault=='public-floor':evidence['data']['loopback']['catalog_version']=1
        elif fault=='public-health':evidence['data']['role_health_verified']=False
        elif fault=='no-public':del changed[public]
        elif fault=='stale-public':changed[-1]['time']+=301
        with pytest.raises(ModError,match='.'):
            replay(changed)


@pytest.mark.parametrize('kind',['migration-observed','layout-observed','start-observed','restart-observed'])
def test_lost_phase_observation_requires_resume_and_never_repeats_os_effect(machine,monkeypatch,kind):
    item=machine;record=item.state.record;failed=[]
    def interrupt(event,*args,**kwargs):
        if event==kind and not failed:
            failed.append(True);raise OSError('lost durable observation')
        return record(event,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(item.state,'record',interrupt)
        with pytest.raises(OSError):coordinator.apply_private(item.state,item.context)
    assert failed and not item.state.state()['settled']
    with pytest.raises(ModError):coordinator.apply_private(item.state,item.context)
    # Force the proof to expire before resuming a lost private migration result.
    item.flow.now[0]+=40
    coordinator.apply_private(item.state,item.context,resume=True)
    assert item.state.state()['activation_ready'] and item.flow.private==['after']
    assert len(item.stops)==len(set(item.stops))==10
    assert len(item.starts)==len(set(item.starts))==10
    assert len(item.kills)==len(set(item.kills))==7
    assert len(item.captures)==(2 if kind=='migration-observed' else 1)


@pytest.mark.parametrize('damage',['boot','plan','tracked','backup','authority'])
def test_foreign_context_is_refused_before_routing_or_service_effects(machine,damage,monkeypatch):
    item=machine
    if damage=='boot':monkeypatch.setattr(coordinator.migration,'boot_identity',lambda:'0'*64)
    elif damage=='plan':item.context['plan']['sha256']='0'*64
    elif damage=='tracked':item.context['observed']=dict(observation=dict(tracked=dict(pids=[],groups=[])))
    elif damage=='backup':item.context['backup_binding']['base_release']='0'*64
    else:item.context['profile']['trusted_root_sha256']='0'*64
    with pytest.raises(ModError):coordinator.apply_private(item.state,item.context)
    assert not item.stops and not item.starts and not item.kills and not item.captures
    assert json.loads(item.state.current)['events']==[]
    assert routes.snapshot(item.context['operator']('status'),item.context['routing']['origin'])==item.context['routing']


def test_composed_reversal_preserves_candidate_history_and_readmits_old_services_at_current_floor(machine):
    item=machine;coordinator.apply_private(item.state,item.context);item.flow.now[0]+=60
    temporary=item.flow.case.root/'monitor'/('.scan-'+'1'*32)
    temporary.write_bytes(b'{');temporary.chmod(0o600)
    result=coordinator.rollback_private(item.state,item.context)
    assert result['generation']=='before' and result['catalog_version']==42
    assert item.flow.private==['before'] and item.flow.calls==['apply','rollback']
    assert len(item.captures)==2 and item.scans==['b'*64,'a'*64]
    assert item.stops==storage.stop_order()*2 and item.starts==storage.start_order()*2
    assert item.kills==storage.restart_order()*2
    folder=item.flow.case.root/'upgrade-layouts'/('c'*64)
    assert (folder/'monitor-after'/temporary.name).read_bytes()==b'{'
    assert (item.flow.case.root/'monitor/deployments/old-receipt.json').exists()
    item.state.require_suspended(item.context['operator'])
    with pytest.raises(ModError):coordinator.apply_private(item.state,item.context,resume=True)
    with pytest.raises(ModError):coordinator.rollback_private(item.state,item.context)
    coordinator.rollback_private(item.state,item.context,resume=True)
    assert len(item.starts)==20 and len(item.kills)==14 and len(item.captures)==2
    result=coordinator.restore_public(item.state,item.context)
    assert result['generation']=='before' and result['public_endpoint_verified'] and item.state.state()['settled']
    assert 128<len(json.loads(item.state.current)['events'])<=storage.MAX_EVENTS


@pytest.mark.parametrize('kind',['shutdown-stop-observed','monitor-checkpoint','layout-observed',
    'migration-observed','start-observed','restart-observed'])
def test_reversal_observation_loss_recovers_without_duplicate_stops_starts_or_kills(machine,monkeypatch,kind):
    item=machine;coordinator.apply_private(item.state,item.context);item.flow.now[0]+=60
    record=item.state.record;failed=[]
    def interrupt(event,*args,**kwargs):
        if event==kind and not failed:
            failed.append(True);raise OSError('lost reversal observation')
        return record(event,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(item.state,'record',interrupt)
        with pytest.raises(OSError):coordinator.rollback_private(item.state,item.context)
    assert failed and not item.state.state()['settled']
    with pytest.raises(ModError):coordinator.rollback_private(item.state,item.context)
    item.flow.now[0]+=40
    coordinator.rollback_private(item.state,item.context,resume=True)
    assert item.flow.private==['before'] and item.state.state()['catalog_floor']==42
    assert item.stops==storage.stop_order()*2 and item.starts==storage.start_order()*2
    assert item.kills==storage.restart_order()*2
    assert len(item.captures)==(3 if kind=='migration-observed' else 2)


def test_reversal_of_partial_candidate_startup_observes_known_jobs_without_starting_remaining_candidate_jobs(machine,monkeypatch):
    item=machine;record=item.state.record
    def interrupt(kind,*args,**kwargs):
        if kind=='start-observed':raise OSError('lost first candidate startup observation')
        return record(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(item.state,'record',interrupt)
        with pytest.raises(OSError):coordinator.apply_private(item.state,item.context)
    assert len(item.starts)==1 and item.state.state()['start_pending']
    result=coordinator.rollback_private(item.state,item.context)
    assert result['generation']=='before' and len(item.starts)==11
    assert item.flow.private==['before'] and len(item.kills)==7


@pytest.mark.parametrize('failure',['scanner','catalog'])
def test_old_generation_failure_stays_private_until_fresh_acceptance_passes(machine,monkeypatch,failure):
    item=machine;coordinator.apply_private(item.state,item.context)
    scanner,ready=process.scanner_check,process.private_readiness
    def scan(current,uid):
        result=scanner(current,uid)
        if current['identity']=='a'*64:result['results']['blocked']='clean'
        return result
    def regressed(current,uid,profile,*,floor):
        result=ready(current,uid,profile,floor=floor)
        if current['identity']=='a'*64:result['catalog_version']=39
        return result
    with monkeypatch.context() as patch:
        patch.setattr(process,'scanner_check',scan if failure=='scanner' else scanner)
        patch.setattr(process,'private_readiness',regressed if failure=='catalog' else ready)
        with pytest.raises(ModError):coordinator.rollback_private(item.state,item.context)
    assert item.flow.private==['before'] and item.state.state()['catalog_floor']==42
    assert not item.state.state()['activation_ready']
    item.state.require_suspended(item.context['operator'])
    with pytest.raises(ModError):routes.transition(item.state.base['routing'],True,item.context['operator'],item.state.routing_event)
    assert len(item.starts)==(10 if failure=='scanner' else 20) and len(item.kills)==7
    coordinator.rollback_private(item.state,item.context,resume=True)
    assert len(item.starts)==20 and len(item.kills)==14 and len(item.captures)==2


@pytest.mark.parametrize('fail_old_scanner',[False,True])
def test_cancel_prepared_layout_before_migration_readmits_original_services_without_private_writes(machine,monkeypatch,fail_old_scanner):
    item=machine;record=item.state.record
    def interrupt(kind,*args,**kwargs):
        if kind=='migration-intent':raise OSError('stopped before private migration intent')
        return record(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(item.state,'record',interrupt)
        with pytest.raises(OSError):coordinator.apply_private(item.state,item.context)
    assert item.state.state()['layout_complete']=='before' and item.flow.calls==[]
    if fail_old_scanner:
        scan=process.scanner_check
        def reject(current,uid):
            result=scan(current,uid);result['results']['blocked']='clean';return result
        with monkeypatch.context() as patch:
            patch.setattr(process,'scanner_check',reject)
            with pytest.raises(ModError):coordinator.rollback_private(item.state,item.context)
        with pytest.raises(ModError):coordinator.rollback_private(item.state,item.context)
        result=coordinator.rollback_private(item.state,item.context,resume=True)
    else:result=coordinator.rollback_private(item.state,item.context)
    assert result['generation']=='before' and item.flow.private==['before'] and item.flow.calls==[]
    assert len(item.starts)==10 and len(item.kills)==7 and len(item.captures)==1
    assert item.scans==['a'*64]*(2 if fail_old_scanner else 1)
    assert (item.flow.case.root/'monitor/deployments/old-receipt.json').exists()


def test_pending_forward_migration_is_not_reinterpreted_as_rollback(machine,monkeypatch):
    item=machine;record=item.state.record
    def interrupt(kind,*args,**kwargs):
        if kind=='migration-observed':raise OSError('lost forward migration result')
        return record(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(item.state,'record',interrupt)
        with pytest.raises(OSError):coordinator.apply_private(item.state,item.context)
    committed=item.state.current
    with pytest.raises(ModError):coordinator.rollback_private(item.state,item.context,resume=True)
    assert item.state.current==committed and not item.starts and len(item.captures)==1
