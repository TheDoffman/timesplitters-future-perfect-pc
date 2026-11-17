"""Real Root journal/layout/proof reader; simulated credentials, OS and private child."""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

from test_upgrade_layout import prepared

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover_journal as journal
import mod_host_migration as migration
import mod_host_routing as routes
import mod_host_upgrade_admission as admission
import mod_host_upgrade_layout as layout
import mod_host_upgrade_process as process
from mod_package import ModError

pytestmark=pytest.mark.parametrize('prepared',['deferred'],indirect=True)


@pytest.fixture
def flow(prepared,workshop_routing,monkeypatch):
    case=prepared;now=[1000000000];calls=[];private=['before']
    monkeypatch.setattr(journal,'boot_identity',lambda:'f'*64)
    monkeypatch.setattr(migration,'boot_identity',lambda:'f'*64)
    monkeypatch.setattr(migration,'ROOT_UID',os.geteuid())
    arguments=dict(before='a'*64,after='b'*64,uid=600,nonce='c'*64,plan_sha256='d'*64,preflight_sha256='e'*64,
        tracked=dict(pids=[],groups=[]),routing=workshop_routing['baseline'],backup_identity='8'*64,
        backup_base_release='7'*64,clock=lambda:now[0])
    def command(argv):
        if argv[0]=='/bin/ps':return 0,'1 0 1 0\n'
        name=argv[-1].removeprefix('system/')
        return 113,'Bad request.\nCould not find service "'+name+'" in domain for system\n'
    with journal.transaction(**arguments) as state:
        state.record('baseline-private-verified',data=dict(schema=1,phase='private-services-verified',release='a'*64,uid=600,
            origin=workshop_routing['baseline']['origin'],trusted_root_sha256='9'*64,catalog_version=40,release_count=3,
            role_health_verified=True,transport='private-loopback-http',public_endpoint_verified=False))
        routes.transition(state.base['routing'],False,workshop_routing['invoke'],state.routing_event)
        for label in journal.stop_order():state.record('stop-intent',label);state.record('stop-observed',label)
        state.record('quiescent')
        def capture_and_prove():
            receipt=dict(schema=1,producer='tsfp-upgrade-backup-v1',identity='8'*64,base_release='7'*64,uid=600,
                status='captured',backup='snapshot-'+('d'*32)+'-'+str(now[0])+'-00000001',
                verified_manifest_sha256=format(now[0],'064x'),created=now[0],started=now[0],finished=now[0],
                bytes=100,forced=True,verified=True)
            state.record('backup-verified',data=receipt)
            proof=dict(schema=1,producer=admission.PRODUCER,before='a'*64,after='b'*64,uid=600,nonce='c'*64,
                plan_sha256='d'*64,backup_sha256=receipt['verified_manifest_sha256'],time=now[0],
                stopped_jobs=migration.jobs(),quiesced_at=state.state()['quiesced_at'],backup_verified_at=now[0],boot_id='f'*64)
            previous,identity=admission._existing(state)
            admission._publish(state,proof,previous,identity)
            return proof
        capture_and_prove();case.exchange.journal=state
        case.fingerprint=case.exchange.prepare()
        gate=lambda:layout.quiescence_gate(state,case.old,command,workshop_routing['invoke'])
        current=dict(identity='b'*64,uid=600)
        def child(current,uid,before,nonce,plan_sha256,*,rollback):
            action='rollback' if rollback else 'apply';calls.append(action)
            # Actual proof reader under simulated child credentials; other code
            # runs under the actual fixture owner, and no service process starts.
            with monkeypatch.context() as patch:
                patch.setattr(migration.os,'geteuid',lambda:600)
                patch.setattr(migration,'ROOT',case.root)
                patch.setattr(migration,'jobs',lambda:sorted(spec['Label'] for _,spec in case.old))
                migration._admission(dict(manifest=dict(before=before,after=current['identity']),sha256=plan_sha256),nonce,state.clock)
            if rollback:
                assert json.loads((case.root/'monitor/monitor.json').read_bytes())['release']==str(case.root/'releases'/before)
            private[0]='before' if rollback else 'after'
            return dict(schema=1,phase='rolled-back' if rollback else 'applied',plan_sha256=plan_sha256,
                before=before,after=current['identity'],files=6,services_changed=False,repository_modified=False,keys_modified=False)
        monkeypatch.setattr(process,'migration_step',child)
        def run(direction,*,resume=False):
            return layout.configuration_and_exchange(state,current,case.exchange,direction,gate,resume=resume)
        yield SimpleNamespace(case=case,state=state,now=now,calls=calls,private=private,run=run,gate=gate,
            capture=capture_and_prove,child=child,routing=workshop_routing,arguments=arguments)


def test_full_journaled_apply_reverse_and_restoration_requires_matching_generation(flow):
    item=flow;state=item.state
    result=item.run('apply')
    assert result['services_started'] is False and state.state()['layout_complete']=='after' and item.private==['after']
    with pytest.raises(ModError):state.record('access-verified',data=dict(release='a'*64,sha256='9'*64))
    item.run('rollback')
    assert item.private==['before'] and state.state()['config']=='before' and item.calls==['apply','rollback']
    state.record('access-verified',data=dict(release='a'*64,sha256='9'*64))
    routes.transition(state.base['routing'],True,item.routing['invoke'],state.routing_event)
    state.record('settled')
    assert state.state()['settled'] and 64<len(json.loads(state.current)['events'])<=journal.MAX_EVENTS


@pytest.mark.parametrize('operation',['archive','install','boot'])
@pytest.mark.parametrize('direction',['apply','rollback'])
def test_real_journal_pending_effect_recovers_without_repeating_migration(flow,monkeypatch,operation,direction):
    item=flow;state=item.state
    if direction=='rollback':item.run('apply')
    original=state.layout_event
    def record(kind,name,fingerprint):
        if kind=='observed' and ((operation=='boot' and '.workshop.' in name) or name==direction+'-'+operation):
            raise OSError('observation interrupted')
        return original(kind,name,fingerprint)
    with monkeypatch.context() as patch:
        patch.setattr(state,'layout_event',record)
        with pytest.raises(OSError):item.run(direction)
    pending=state.state()['layout_pending'];assert pending
    with pytest.raises(ModError):item.run(direction)
    for release in ('a'*64,'b'*64):
        with pytest.raises(ModError):state.record('access-verified',data=dict(release=release,sha256='9'*64))
    item.run(direction,resume=True)
    assert state.state()['layout_pending'] is None
    assert item.calls==(['apply'] if direction=='apply' else ['apply','rollback'])
    events=json.loads(state.current)['events']
    assert sum(event['kind']=='layout-intent' and event['label']==pending for event in events)==1


def test_lost_private_child_result_keeps_intent_and_requires_explicit_resume(flow,monkeypatch):
    item=flow
    def lost(*args,**kwargs):item.child(*args,**kwargs);raise OSError('lost child result')
    with monkeypatch.context() as patch:
        patch.setattr(process,'migration_step',lost)
        with pytest.raises(OSError):item.run('apply')
    assert item.private==['after'] and item.state.state()['config_pending']=='apply'
    assert item.case.exchange._locations()['active']=='before'
    with pytest.raises(ModError):item.run('apply')
    item.run('apply',resume=True)
    assert item.state.state()['config_pending'] is None and item.calls==['apply','apply']


def test_expired_proof_and_fresh_recapture_can_resume_after_layout_preparation(flow):
    item=flow;item.now[0]+=40
    with pytest.raises(ModError):item.run('apply')
    assert item.state.state()['config_pending']=='apply' and item.private==['before']
    item.capture();item.run('apply',resume=True)
    assert item.private==['after'] and item.state.state()['layout_complete']=='after'


def test_rollback_refreshes_proof_after_old_monitor_restoration_and_preserves_new_proof(flow):
    item=flow;item.run('apply');item.now[0]+=40
    item.capture()  # Fresh proof in the new active monitor; archive it on reverse.
    with pytest.raises(ModError):item.run('rollback')  # Restored old proof has expired.
    assert item.state.state()['layout_complete']=='before' and item.state.state()['config_pending']=='rollback'
    item.capture();item.run('rollback',resume=True)
    assert item.private==['before'] and item.state.state()['config_pending'] is None
    assert (item.case.exchange.staged/'upgrade-quiescence.json').exists()


def test_old_backup_cannot_authorize_migration_even_with_prepared_layout(flow):
    item=flow;item.now[0]+=301
    with pytest.raises(ModError):item.run('apply')
    assert item.state.state()['config_pending'] is None and item.calls==[]
    assert item.case.exchange._locations()['active']=='before'


def test_cancelling_prepared_layout_does_not_migrate_or_exchange_any_files(flow):
    item=flow;result=item.run('rollback')
    assert result['phase']=='configuration-and-layout-rolled-back' and item.calls==[]
    assert item.private==['before'] and not item.case.exchange.saved.exists()
    assert item.state.state()['layout_complete']=='before'


@pytest.mark.parametrize('field',['backup_sha256','nonce','boot_id','uid'])
def test_unknown_proof_is_refused_before_private_migration(flow,field):
    item=flow;path=item.case.root/'monitor/upgrade-quiescence.json'
    value=json.loads(path.read_bytes());value[field]=601 if field=='uid' else '0'*64
    path.write_bytes(migration.wire(value))
    with pytest.raises(ModError):item.run('apply')
    assert item.calls==[] and item.private==['before'] and not item.case.exchange.saved.exists()


def test_missing_original_proof_is_preserved_and_refused_before_private_migration(flow):
    item=flow;path=item.case.root/'monitor/upgrade-quiescence.json'
    path.rename(item.case.root/'preserved-proof.json')
    with pytest.raises(ModError):item.run('apply')
    assert item.calls==[] and item.private==['before'] and not path.exists()


def test_typed_journal_refuses_forged_out_of_order_or_foreign_effects(flow):
    item=flow;state=item.state
    state.record('layout-prepared',data=item.case.fingerprint)
    for kind,label,data in [
        ('migration-intent','apply','0'*64),('migration-observed','apply',{}),
        ('layout-intent','apply-archive',item.case.fingerprint),('layout-complete','apply',{}),
    ]:
        with pytest.raises(ModError):state.record(kind,label,data)
    item.run('apply')
    for kind,label,data in [
        ('layout-intent','apply-archive',item.case.fingerprint),
        ('layout-intent','rollback-other.job',item.case.fingerprint),
        ('layout-observed','rollback-archive',item.case.fingerprint),('migration-intent','rollback','d'*64),
    ]:
        with pytest.raises(ModError):state.record(kind,label,data)
    with pytest.raises(ModError):state.layout_event('intent','rollback-archive','0'*64)


def test_journal_event_capacity_is_bounded_and_refusal_keeps_committed_bytes(flow):
    state=flow.state
    while len(json.loads(state.current)['events'])<journal.MAX_EVENTS:state.record('quiescence-rechecked')
    previous=state.current
    with pytest.raises(ModError):state.record('quiescence-rechecked')
    assert state.current==previous and len(previous)<=journal.MAX_BYTES
