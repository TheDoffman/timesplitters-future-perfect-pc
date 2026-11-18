"""Real admission/journal/files with simulated signed-readiness and fixed OS effects."""
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest
from test_upgrade_layout import prepared
from test_upgrade_flow import flow
from test_upgrade_activation import running,simulated_boot

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_upgrade_activation as activation
import mod_host_upgrade_process as process
import mod_host_cutover_journal as journal_code
import mod_host_routing as routes
from mod_package import ModError

pytestmark=pytest.mark.parametrize('prepared',['deferred'],indirect=True)


@pytest.fixture
def accepting(running,monkeypatch):
    case=running;case.start();kills=[];checks=[];mode=[None];version=[42];extra=[];ticks=[0.0];damage=[None]
    profile={key:case.flow.state.state()['private_baseline'][key] for key in ('origin','trusted_root_sha256')}
    def ready(current,uid,authority,*,floor):
        checks.append(floor)
        result=dict(schema=1,phase='private-services-verified',release=current['identity'],uid=uid,**authority,
            catalog_version=version[0],release_count=3,role_health_verified=True,transport='private-loopback-http',public_endpoint_verified=False)
        if damage[0]=='root':result['trusted_root_sha256']='0'*64
        if damage[0]=='health':result['role_health_verified']=False
        return result
    monkeypatch.setattr(process,'private_readiness',ready)
    def command(argv):
        if argv[:3]==['/bin/launchctl','kill','SIGKILL']:
            label=argv[-1].removeprefix('system/');pending=case.flow.state.state()['restart_pending']
            assert pending and pending['label']==label
            kills.append(label);old=case.pids[label]
            if mode[0]!='no-change':case.pids[label]+=1000
            if mode[0]=='orphan':extra.append(f'{old} 1 {old} 600\n')
            if mode[0]=='regress':version[0]=39
            if mode[0]=='timeout':raise subprocess.TimeoutExpired(argv,3)
            return (1 if mode[0]=='nonzero' else 0),'fixed utility result'
        code,output=case.command(argv)
        if argv[0]=='/bin/ps':
            output+=''.join(extra)
            if kills and mode[0] in ('wrong-owner','new-missing'):
                pid=case.pids[kills[-1]];lines=[]
                for line in output.splitlines():
                    fields=line.split()
                    if int(fields[0])==pid:
                        if mode[0]=='new-missing':continue
                        fields[-1]='0'
                    lines.append(' '.join(fields))
                output='\n'.join(lines)+'\n'
        return code,output
    def sleep(seconds):ticks[0]+=seconds
    def admit(*,resume=False):
        return activation.admit_private_services(case.flow.state,case.current,case.flow.case.exchange,case.flow.case.new,
            profile,command,case.flow.routing['invoke'],resume=resume,clock=lambda:ticks[0],sleep=sleep,timeout=1)
    return SimpleNamespace(case=case,profile=profile,kills=kills,checks=checks,mode=mode,version=version,extra=extra,
                           ticks=ticks,damage=damage,admit=admit,command=command)


def test_seven_restarts_and_signed_floor_admit_only_fresh_selective_restoration(accepting,public_upgrade_report):
    case=accepting;state=case.case.flow.state;result=case.admit()
    assert case.kills==journal_code.restart_order() and len(case.checks)==8
    assert case.checks==[40]+[42]*7 and result['catalog_version']==42
    assert state.state()['activation_ready'] and result['public_endpoint_started'] is False
    routes.transition(state.base['routing'],True,case.case.flow.routing['invoke'],state.routing_event)
    with pytest.raises(ModError):state.record('settled')
    state.record('public-readiness-verified',state.state()['config'],public_upgrade_report(state))
    state.record('settled');assert state.state()['settled']


@pytest.mark.parametrize('mode',['nonzero','timeout'])
def test_authoritative_new_pid_and_cleanup_can_confirm_nonzero_or_timeout(accepting,mode):
    case=accepting;case.mode[0]=mode;case.admit()
    assert len(case.kills)==7 and case.case.flow.state.state()['activation_ready']


@pytest.mark.parametrize('mode',['no-change','orphan','wrong-owner','new-missing'])
def test_pending_restart_preserves_old_tree_evidence_and_resume_never_repeats_kill(accepting,mode):
    case=accepting;case.mode[0]=mode;state=case.case.flow.state
    with pytest.raises(ModError):case.admit()
    pending=state.state()['restart_pending'];assert pending and case.kills==[pending['label']]
    with pytest.raises(ModError):case.admit()
    if mode=='no-change':case.case.pids[pending['label']]+=1000
    case.extra.clear();case.mode[0]=None;case.admit(resume=True)
    assert case.kills==journal_code.restart_order() and state.state()['activation_ready']


def test_lost_restart_observation_resumes_new_pid_without_another_signal(accepting,monkeypatch):
    case=accepting;state=case.case.flow.state;original=state.record
    def record(kind,*args,**kwargs):
        if kind=='restart-observed':raise OSError('lost cleanup observation')
        return original(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(state,'record',record)
        with pytest.raises(OSError):case.admit()
    assert state.state()['restart_pending'] is not None and len(case.kills)==1
    case.admit(resume=True)
    assert len(case.kills)==7 and state.state()['restart_pending'] is None


@pytest.mark.parametrize('damage',['root','health'])
def test_invalid_private_readiness_stops_before_any_restart(accepting,damage):
    case=accepting;case.damage[0]=damage
    with pytest.raises(ModError):case.admit()
    assert not case.kills and not case.case.flow.state.state()['activation_ready']


def test_catalog_regression_after_a_restart_stays_private_and_can_resume_from_known_floor(accepting):
    case=accepting;state=case.case.flow.state;case.mode[0]='regress'
    with pytest.raises(ModError):case.admit()
    assert state.state()['catalog_floor']==42 and len(case.kills)==1 and not state.state()['activation_ready']
    case.mode[0]=None;case.version[0]=43;case.admit(resume=True)
    assert state.state()['catalog_floor']==43 and len(case.kills)==7


def test_expired_admission_rechecks_controls_and_readiness_without_repeating_restarts(accepting):
    case=accepting;case.admit();state=case.case.flow.state;case.case.flow.now[0]+=301
    with pytest.raises(ModError):routes.transition(state.base['routing'],True,case.case.flow.routing['invoke'],state.routing_event)
    case.admit(resume=True)
    assert len(case.kills)==7 and len(case.case.scanner)==2
    routes.transition(state.base['routing'],True,case.case.flow.routing['invoke'],state.routing_event)
    assert state.state()['routing_restored']
