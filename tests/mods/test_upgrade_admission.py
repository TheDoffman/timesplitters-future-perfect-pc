"""Real protected proof/journal files; simulated job and backup child actions."""
import hashlib
import os
from pathlib import Path
import sys

import pytest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as journal
import mod_host_migration as migration
import mod_host_routing as routes
import mod_host_upgrade_admission as admission
from mod_package import ModError


def plan_report(origin):
    manifest=dict(schema=1,producer=migration.PRODUCER,before='a'*64,after='b'*64,origin=origin,
                  guards={name:'9'*64 for name in migration.GUARDS},
                  files={name:dict(before='1'*64,after='2'*64) for name in migration.FIELDS})
    return dict(manifest=manifest,sha256=hashlib.sha256(migration.wire(manifest)).hexdigest(),
                files=6,services_changed=False)


@pytest.fixture
def context(private_posix_path,monkeypatch,workshop_routing):
    root=private_posix_path/'workshop';root.mkdir(mode=0o755);(root/'monitor').mkdir(mode=0o755)
    monkeypatch.setattr(cutover,'ROOT',root);monkeypatch.setattr(migration,'ROOT',root)
    monkeypatch.setattr(cutover,'ROOT_UID',os.geteuid());monkeypatch.setattr(migration,'ROOT_UID',os.geteuid())
    monkeypatch.setattr(journal,'boot_identity',lambda:'f'*64);monkeypatch.setattr(migration,'boot_identity',lambda:'f'*64)
    profile=dict(origin=workshop_routing['baseline']['origin'],trusted_root_sha256='9'*64)
    plan=plan_report(profile['origin']);now=[1000000000];calls=[]
    args=dict(before='a'*64,after='b'*64,uid=600,nonce='c'*64,plan_sha256=plan['sha256'],
              preflight_sha256='e'*64,backup_identity='8'*64,backup_base_release='7'*64,
              tracked=dict(pids=[],groups=[]),routing=workshop_routing['baseline'],clock=lambda:now[0])
    report=dict(schema=1,producer='tsfp-upgrade-backup-v1',identity='8'*64,base_release='7'*64,uid=600,
                status='captured',backup='snapshot-'+('d'*32)+'-1000000000-00000001',
                verified_manifest_sha256='6'*64,created=now[0],started=now[0],finished=now[0],bytes=100,
                forced=True,verified=True)
    def quiesce(entries,uid,state,command,*,routing):
        calls.append('quiesce');state.require_suspended(routing)
        if not state.state()['quiescent']:
            for label in journal.stop_order():
                state.record('stop-intent',label);state.record('stop-observed',label)
            state.record('quiescent')
        else:state.record('quiescence-rechecked')
        return state.state()
    def capture(*values,**keywords):
        calls.append('backup');assert not (root/'monitor'/admission.NAME).exists()
        return report.copy()
    monkeypatch.setattr(cutover,'quiesce',quiesce);monkeypatch.setattr(admission.process,'forced_backup',capture)
    return dict(args=args,root=root,plan=plan,profile=profile,report=report,calls=calls,now=now,routing=workshop_routing)


def run(context,state):
    return admission.capture_and_admit(state,[],dict(identity='7'*64,uid=600),dict(identity='b'*64,uid=600),
        context['profile'],context['plan'],context['routing']['invoke'],None)


def suspend(context,state):
    routes.transition(state.base['routing'],False,context['routing']['invoke'],state.routing_event)


def test_proof_follows_backup_and_second_quiescence_and_matches_worker_reader(context,monkeypatch):
    with journal.transaction(**context['args']) as state:
        suspend(context,state);proof=run(context,state)
        assert context['calls']==['quiesce','backup','quiesce']
        path=context['root']/'monitor'/admission.NAME
        assert path.stat().st_mode & 0o777==0o644
        assert proof['backup_sha256']==context['report']['verified_manifest_sha256']
        assert state.state()['backup']==context['report']
        assert admission._existing(state)[0]==migration.wire(proof)
        # Exercise the actual worker reader; only service credentials are simulated.
        with monkeypatch.context() as patch:
            patch.setattr(migration.os,'geteuid',lambda:600)
            assert migration._admission(context['plan'],state.base['nonce'],state.clock)==migration.wire(proof)


@pytest.mark.parametrize('damage',['not-due','old','future','wrong-binding','unverified','plan','public','reappeared'])
def test_invalid_capture_or_recheck_never_issues_admission(context,monkeypatch,damage):
    with journal.transaction(**context['args']) as state:
        suspend(context,state)
        if damage=='not-due':context['report']['status']='not-due'
        elif damage=='old':context['report']['started']-=1
        elif damage=='future':context['report']['finished']+=1
        elif damage=='wrong-binding':context['report']['identity']='5'*64
        elif damage=='unverified':context['report']['verified']=False
        elif damage=='plan':context['plan']['manifest']['origin']='https://foreign.example'
        else:
            original=admission.process.forced_backup
            def capture(*args,**kwargs):
                result=original(*args,**kwargs)
                if damage=='public':context['routing']['invoke']('restore')
                else:
                    def reappeared(*args,**kwargs):raise ModError('worker reappeared')
                    monkeypatch.setattr(cutover,'quiesce',reappeared)
                return result
            monkeypatch.setattr(admission.process,'forced_backup',capture)
        with pytest.raises(ModError):run(context,state)
        assert not (context['root']/'monitor'/admission.NAME).exists()


@pytest.mark.parametrize('damage',['foreign-proof','pending'])
def test_unrecognized_evidence_is_preserved(context,damage):
    name=admission.NAME if damage=='foreign-proof' else '.upgrade-quiescence-'+context['args']['nonce']+'.pending'
    path=context['root']/'monitor'/name;path.write_bytes(b'{"foreign":true}');path.chmod(0o644)
    original=path.read_bytes()
    with journal.transaction(**context['args']) as state:
        suspend(context,state)
        with pytest.raises((ModError,OSError)):run(context,state)
        assert path.read_bytes()==original
        if damage=='foreign-proof':assert context['calls']==[]
        else:assert not (context['root']/'monitor'/admission.NAME).exists()


def test_refresh_requires_a_new_capture_and_keeps_original_quiescence(context,monkeypatch):
    with journal.transaction(**context['args']) as state:
        suspend(context,state);first=run(context,state)
        context['now'][0]+=40
        context['report'].update(started=context['now'][0],created=context['now'][0],finished=context['now'][0],
                                 verified_manifest_sha256='5'*64)
        monkeypatch.setattr(admission.process,'forced_backup',lambda *args,**kwargs:context['report'].copy())
        second=run(context,state)
        assert second['time']==first['time']+40 and second['quiesced_at']==first['quiesced_at']
        assert second['backup_sha256']=='5'*64 and second['backup_verified_at']==context['now'][0]
        assert admission._existing(state)[0]==migration.wire(second)
        assert len([event for event in migration.read_json(state.current)['events'] if event['kind']=='backup-verified'])==2


def test_interrupted_proof_commit_preserves_pending_and_refuses_implicit_retry(context,monkeypatch):
    with journal.transaction(**context['args']) as state:
        suspend(context,state)
        original=admission.os.replace
        def interrupted(source,target,**kwargs):
            if target==admission.NAME:raise OSError('proof commit interrupted')
            return original(source,target,**kwargs)
        with monkeypatch.context() as patch:
            patch.setattr(admission.os,'replace',interrupted)
            with pytest.raises(OSError):run(context,state)
        pending=context['root']/'monitor'/('.upgrade-quiescence-'+state.base['nonce']+'.pending')
        payload=pending.read_bytes()
        assert not (context['root']/'monitor'/admission.NAME).exists()
        with pytest.raises(FileExistsError):run(context,state)
        assert pending.read_bytes()==payload
