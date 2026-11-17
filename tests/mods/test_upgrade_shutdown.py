"""Real protected stop/checkpoint/reverse journal; simulated launchd, AV and readiness."""
import json
from pathlib import Path
import plistlib
import subprocess
import sys
from types import SimpleNamespace

import pytest
from test_upgrade_layout import prepared
from test_upgrade_flow import flow
from test_upgrade_activation import running,simulated_boot

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover_journal as journal_code
import mod_host_upgrade_activation as activation
import mod_host_upgrade_shutdown as shutdown
import mod_host_upgrade_layout as layout
import mod_host_upgrade_process as process
import mod_host_routing as routes
from mod_package import ModError

pytestmark=pytest.mark.parametrize('prepared',['deferred'],indirect=True)


@pytest.fixture
def stopping(running,monkeypatch):
    case=running;case.start();item=case.flow;stops=[];mode=[None];ticks=[0.0]
    monkeypatch.setattr(shutdown,'boot_identity',lambda:'f'*64)
    policies={spec['Label']:(path,spec) for path,spec in item.case.new}
    def command(argv):
        if argv[0]=='/bin/ps':return case.command(argv)
        label=argv[-1].removeprefix('system/')
        if argv[1]=='bootout':
            assert item.state.state()['shutdown_pending']==label;stops.append(label)
            if mode[0]!='not-unloaded':case.loaded.pop(label,None)
            if mode[0]=='timeout':raise subprocess.TimeoutExpired(argv,3)
            return (1 if mode[0] in ('error','not-unloaded') else 0),'fixed utility outcome'
        if argv[1]=='bootstrap':
            path=Path(argv[-1]);spec=plistlib.loads(path.read_bytes());label=spec['Label']
            assert item.state.state()['start_pending']==label and not (item.case.root/'monitor/upgrade-quiescence.json').exists()
            policies[label]=(path,spec);case.loaded[label]=True;case.effects.append(label);return 0,''
        if argv[1]=='kill':case.pids[label]+=1000;return 0,''
        if label not in case.loaded:return 113,'Bad request.\nCould not find service "'+label+'" in domain for system\n'
        path,spec=policies[label]
        return 0,('system/'+label+' = {\n\tpath = '+str(path)+'\n\ttype = LaunchDaemon\n\tprogram = '+spec['ProgramArguments'][0]
            +'\n\targuments = {\n'+''.join('\t\t'+value+'\n' for value in spec['ProgramArguments'])
            +'\t}\n\tpid = '+str(case.pids[label])+'\n}\n')
    def sleep(seconds):ticks[0]+=seconds
    def entries():return item.case.old if item.state.state()['monitor']=='before' else item.case.new
    def stop(*,resume=False):
        return shutdown.stop_private_services(item.state,item.case.exchange,entries(),command,item.routing['invoke'],
            resume=resume,clock=lambda:ticks[0],sleep=sleep,timeout=1)
    def gate():return layout.quiescence_gate(item.state,entries(),command,item.routing['invoke'])
    def checkpoint(*,resume=False):return shutdown.checkpoint_monitor(item.state,item.case.exchange,entries(),gate,resume=resume)
    profile={key:item.state.state()['private_baseline'][key] for key in ('origin','trusted_root_sha256')}
    def ready(current,uid,authority,*,floor):
        return dict(schema=1,phase='private-services-verified',release=current['identity'],uid=uid,**authority,
            catalog_version=max(42,floor),release_count=3,role_health_verified=True,transport='private-loopback-http',public_endpoint_verified=False)
    monkeypatch.setattr(process,'private_readiness',ready)
    def admit():
        generation=item.state.state()['config'];current=dict(identity=item.state.base[generation],uid=600)
        return activation.admit_private_services(item.state,current,item.case.exchange,entries(),profile,command,item.routing['invoke'])
    def health():
        for path,spec in entries():
            if not spec['Label'].endswith('.monitor'):continue
            role=spec['Label'].split('.')[3]
            value=dict(schema=1,uid=600,service=spec['Label'].removesuffix('.monitor'),time=item.now[0],status='idle',
                policy='root-system-bootstrap-v2' if role=='scan' else 'root-system-host-v1')
            if role!='scan':value.update(role=role,release=str(item.case.root/'releases'/item.state.base[item.state.state()['config']]))
            target=path.with_name('scan.json' if role=='scan' else 'health.json')
            target.write_bytes(json.dumps(value).encode());target.chmod(0o644)
    return SimpleNamespace(case=case,flow=item,stops=stops,mode=mode,command=command,stop=stop,gate=gate,checkpoint=checkpoint,admit=admit,health=health)


def test_full_started_upgrade_shutdown_checkpoint_reverse_and_old_private_admission(stopping):
    case=stopping;item=case.flow;case.admit();case.health();item.now[0]+=60
    case.stop();assert case.stops==journal_code.stop_order() and item.state.state()['quiescent'] and not case.case.loaded
    temporary=item.case.root/'monitor'/('.scan-'+'1'*32);temporary.write_bytes(b'{');temporary.chmod(0o600)
    checkpoint=case.checkpoint();assert item.state.state()['monitor_checkpoints']['after']==checkpoint
    item.capture()
    with pytest.raises(ModError):item.run('rollback') # Archived old authority expired before restoration.
    assert item.state.state()['config_pending']=='rollback'
    item.capture();item.run('rollback',resume=True)
    assert item.private==['before'] and (item.case.exchange.staged/temporary.name).read_bytes()==b'{'
    current=dict(identity='a'*64,uid=600)
    activation.start_private_services(item.state,current,item.case.exchange,item.case.old,case.command,item.routing['invoke'])
    case.admit()
    routes.transition(item.state.base['routing'],True,item.routing['invoke'],item.state.routing_event)
    item.state.record('settled')
    assert item.state.state()['catalog_floor']==42 and item.state.state()['settled']
    assert 128<len(json.loads(item.state.current)['events'])<=journal_code.MAX_EVENTS


@pytest.mark.parametrize('mode',['error','timeout'])
def test_stop_absence_confirms_nonzero_or_timeout_without_retry(stopping,mode):
    case=stopping;case.mode[0]=mode;case.stop()
    assert len(case.stops)==10 and case.flow.state.state()['shutdown_complete']


def test_pending_stop_resumes_only_observation_without_repeating_bootout(stopping):
    case=stopping;case.mode[0]='not-unloaded'
    with pytest.raises(ModError):case.stop()
    label=case.flow.state.state()['shutdown_pending'];assert label and case.stops==[label]
    case.case.loaded.pop(label);case.mode[0]=None
    with pytest.raises(ModError):case.stop()
    case.stop(resume=True)
    assert case.stops==journal_code.stop_order() and case.flow.state.state()['shutdown_complete']


def test_lost_stop_observation_recovers_without_repeating_effect(stopping,monkeypatch):
    case=stopping;state=case.flow.state;original=state.record
    def record(kind,*args,**kwargs):
        if kind=='shutdown-stop-observed':raise OSError('lost stop observation')
        return original(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(state,'record',record)
        with pytest.raises(OSError):case.stop()
    assert len(case.stops)==1 and state.state()['shutdown_pending']
    case.stop(resume=True)
    assert len(case.stops)==10 and state.state()['shutdown_complete']


def test_escaped_uid_process_blocks_quiescence_after_known_labels_stop(stopping):
    case=stopping;case.case.foreign[0]=True
    with pytest.raises(ModError):case.stop()
    assert len(case.stops)==10 and not case.flow.state.state()['quiescent']
    case.case.foreign[0]=False;case.stop(resume=True)
    assert len(case.stops)==10 and case.flow.state.state()['quiescent']


@pytest.mark.parametrize('damage',['unknown','bad-health','health-link'])
def test_checkpoint_preserves_unknown_or_invalid_history_and_refuses_adoption(stopping,damage):
    case=stopping;case.health();case.stop();root=case.flow.case.root/'monitor'
    if damage=='unknown':path=root/'foreign';path.write_bytes(b'preserve');path.chmod(0o600)
    elif damage=='bad-health':
        path=root/'api/health.json';value=json.loads(path.read_bytes());value['release']='/foreign';path.write_bytes(json.dumps(value).encode())
    else:
        path=root/'api/health.json';path.rename(case.flow.case.root/'saved-health');path.symlink_to(case.flow.case.root/'saved-health')
    identity=path.lstat()
    with pytest.raises((ModError,OSError)):case.checkpoint()
    assert path.lstat()==identity and not case.flow.state.state()['monitor_checkpoints']


def test_pending_absent_startup_can_be_cancelled_by_observed_shutdown(running,monkeypatch):
    case=running;item=case.flow;case.mode[0]='absent-error'
    monkeypatch.setattr(shutdown,'boot_identity',lambda:'f'*64)
    with pytest.raises(ModError):case.start()
    result=shutdown.stop_private_services(item.state,item.case.exchange,item.case.new,case.command,item.routing['invoke'])
    assert result['shutdown_complete'] and result['quiescent'] and result['start_pending'] is None and case.effects==[journal_code.start_order()[0]]
