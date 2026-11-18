"""Root-journal storage and recovery fixtures; no live job changes or root proof."""
import json
import os
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as journal
import mod_host_migration as migration
import mod_host_routing as routes
from mod_package import ModError


@pytest.fixture
def transaction(private_posix_path, monkeypatch, workshop_routing):
    root = private_posix_path/'workshop';root.mkdir(mode=0o755)
    monkeypatch.setattr(cutover, 'ROOT', root)
    monkeypatch.setattr(migration, 'ROOT', root)
    monkeypatch.setattr(cutover, 'ROOT_UID', os.geteuid())
    monkeypatch.setattr(journal, 'boot_identity', lambda:'f'*64)
    return dict(before='a'*64, after='b'*64, uid=600, nonce='c'*64,
                plan_sha256='d'*64, preflight_sha256='e'*64,
                backup_identity='8'*64,backup_base_release='7'*64,
                tracked=dict(pids=[123],groups=[123]),routing=workshop_routing['baseline'],clock=lambda:1000)


def suspend(state,fixture):
    return routes.transition(state.base['routing'],False,fixture['invoke'],state.routing_event)


def test_recovery_only_does_not_create_missing_history_and_new_only_refuses_existing_nonce(transaction):
    with pytest.raises(FileNotFoundError):
        with journal.transaction(**transaction,existing_only=True):pass
    assert not (cutover.ROOT/'upgrades').exists()
    with journal.transaction(**transaction,new_only=True) as state:original=state.current
    with pytest.raises(ModError):
        with journal.transaction(**transaction,new_only=True):pass
    with journal.transaction(**transaction,existing_only=True) as state:assert state.current==original
    with pytest.raises(ModError):
        with journal.transaction(**transaction,new_only=True,existing_only=True):pass


def test_recovery_arguments_reopen_the_original_transaction_without_creating_a_new_one(transaction):
    with pytest.raises(FileNotFoundError):journal.recovery_arguments(transaction['nonce'],'a'*64,'b'*64,600,'8'*64,clock=lambda:1000)
    assert not (cutover.ROOT/'upgrades').exists()
    with journal.transaction(**transaction,new_only=True) as state:original=state.current
    arguments=journal.recovery_arguments(transaction['nonce'],'a'*64,'b'*64,600,'8'*64,clock=lambda:1000)
    assert arguments=={key:value for key,value in transaction.items() if key!='clock'}
    with journal.transaction(**arguments,clock=lambda:1000,existing_only=True,recover_pending=True) as state:
        assert state.current==original


@pytest.mark.parametrize('damage',['nonce','before','after','uid','backup','boot'])
def test_recovery_arguments_refuse_other_identity_or_boot_without_mutating_history(transaction,monkeypatch,damage):
    with journal.transaction(**transaction,new_only=True) as state:original=state.current
    values=[transaction['nonce'],'a'*64,'b'*64,600,'8'*64]
    if damage=='boot':monkeypatch.setattr(journal,'boot_identity',lambda:'0'*64)
    elif damage=='nonce':values[0]='../escape'
    elif damage=='uid':values[3]=600.0
    else:values[{'before':1,'after':2,'backup':4}[damage]]='0'*64
    with pytest.raises(ModError):journal.recovery_arguments(*values,clock=lambda:1000)
    assert (cutover.ROOT/'upgrades'/(transaction['nonce']+'.json')).read_bytes()==original


def test_changed_journal_base_between_argument_read_and_lease_is_not_adopted(transaction):
    with journal.transaction(**transaction,new_only=True):pass
    arguments=journal.recovery_arguments(transaction['nonce'],'a'*64,'b'*64,600,'8'*64,clock=lambda:1000)
    path=cutover.ROOT/'upgrades'/(transaction['nonce']+'.json')
    value=json.loads(path.read_bytes());value['plan_sha256']='6'*64;changed=journal.wire(value);path.write_bytes(changed)
    with pytest.raises(ModError):
        with journal.transaction(**arguments,clock=lambda:1000,existing_only=True):pass
    assert path.read_bytes()==changed


def restore(state,fixture,*,after=False):
    state.record('access-verified',data=dict(release=state.base['after' if after else 'before'],sha256='9'*64))
    return routes.transition(state.base['routing'],True,fixture['invoke'],state.routing_event)


def test_stop_intent_survives_reopen_and_blocks_new_transaction(transaction,workshop_routing):
    label = journal.stop_order()[0]
    with journal.transaction(**transaction) as state:
        assert not state.state()['stopped']
        with pytest.raises(ModError):state.record('stop-intent',label)
        suspend(state,workshop_routing)
        state.record('stop-intent', label)
        assert state.state()['pending'] == label
        with pytest.raises(ModError):
            with journal.transaction(**transaction):pass
    with pytest.raises(ModError):
        with journal.transaction(**dict(transaction, nonce='9'*64)):pass
    with journal.transaction(**transaction) as state:
        assert state.state()['pending'] == label
        with pytest.raises(ModError):state.record('settled')
        with pytest.raises(ModError):state.record('stop-intent', label)
        state.record('stop-observed', label)
        # A real coordinator may settle this only after verifying restoration.
        restore(state,workshop_routing)
        state.record('settled')
    with journal.transaction(**dict(transaction, nonce='9'*64)) as state:
        state.record('settled')


def test_monitors_precede_backup_and_workers_and_quiescence_requires_all_observations(transaction,workshop_routing):
    order = journal.stop_order()
    assert len(order) == 10 and all(v.endswith('.monitor') for v in order[:4])
    assert order[4] == 'org.timesplitters.workshop.backup'
    with journal.transaction(**transaction) as state:
        suspend(state,workshop_routing)
        for kind,label in [('quiescent',None), ('stop-observed',order[0]), ('stop-intent',order[-1])]:
            with pytest.raises(ModError):state.record(kind,label)
        for label in order:
            state.record('stop-intent',label)
            with pytest.raises(ModError):state.record('quiescent')
            state.record('stop-observed',label)
        state.record('quiescent')
        assert state.state()['quiescent'] and not state.state()['settled']
        with pytest.raises(ModError):state.record('stop-intent',order[0])
        with pytest.raises(ModError):state.record('settled')
        restore(state,workshop_routing,after=True)
        state.record('settled')
        with pytest.raises(ModError):state.record('settled')


@pytest.mark.parametrize('damage', ['foreign', 'mode', 'link', 'hardlink', 'partial', 'boolean', 'duplicate', 'future', 'unknown-field'])
def test_unknown_history_is_preserved(transaction, damage):
    with journal.transaction(**transaction):pass
    path = cutover.ROOT/'upgrades'/(transaction['nonce']+'.json')
    if damage == 'mode':path.chmod(0o644)
    elif damage in ('link','hardlink'):
        saved=path.with_name('saved');path.rename(saved)
        if damage == 'link':path.symlink_to(saved.name)
        else:os.link(saved,path)
    elif damage == 'partial':path.write_bytes(b'{')
    elif damage == 'duplicate':path.write_bytes(b'{"schema":1,"schema":1}')
    else:
        value=json.loads(path.read_bytes())
        if damage == 'foreign':value['plan_sha256']='8'*64
        elif damage == 'boolean':value['schema']=True
        elif damage == 'future':value['events']=[dict(kind='settled',label=None,time=1001,data=None)]
        else:value['unknown']='preserve'
        path.write_bytes(journal.wire(value))
    stamp = cutover.stamp(path.lstat())
    with pytest.raises((ModError,OSError)):
        with journal.transaction(**transaction):pass
    assert cutover.stamp(path.lstat()) == stamp


@pytest.mark.parametrize('target', ['journal','lease'])
def test_replaced_lease_and_journal_are_not_silently_adopted(transaction, target):
    with journal.transaction(**transaction) as state:
        path = cutover.ROOT/'upgrades'/(state.name if target == 'journal' else '.cutover.lock')
        value=path.read_bytes();path.rename(path.with_suffix('.old'))
        path.write_bytes(value+b' ' if target == 'journal' else value);path.chmod(0o600)
        with pytest.raises(ModError):state.record('settled')
    # A different fixture owns this deliberate partial evidence; no cleanup/adoption.


def test_pending_write_after_interruption_is_preserved_and_refuses_resume(transaction, monkeypatch,workshop_routing):
    with journal.transaction(**transaction) as state:
        suspend(state,workshop_routing)
        def interrupted(*args,**kwargs):raise OSError('injected rename interruption')
        with monkeypatch.context() as patch:
            patch.setattr(journal.os,'replace',interrupted)
            with pytest.raises(OSError):state.record('stop-intent',journal.stop_order()[0])
        assert not state.state()['pending']
        pending = state.directory/(state.name+'.pending')
        assert pending.exists() and json.loads(pending.read_bytes())['events'][-1]['kind']=='stop-intent'
    saved=pending.read_bytes()
    with pytest.raises(ModError):
        with journal.transaction(**transaction):pass
    assert pending.read_bytes()==saved


def test_journal_capacity_allows_same_transaction_recovery_but_not_a_new_nonce(transaction):
    for number in range(journal.MAX_JOURNALS):
        with journal.transaction(**dict(transaction,nonce=format(number,'064x'))) as state:state.record('settled')
    with journal.transaction(**dict(transaction,nonce=format(0,'064x'))) as state:assert state.state()['settled']
    with pytest.raises(ModError):
        with journal.transaction(**transaction):pass


def test_changed_boot_refuses_existing_transaction_and_unprivileged_use_writes_nothing(transaction, monkeypatch):
    with journal.transaction(**transaction):pass
    with monkeypatch.context() as patch:
        patch.setattr(journal,'boot_identity',lambda:'1'*64)
        with pytest.raises(ModError):
            with journal.transaction(**transaction):pass
    with monkeypatch.context() as patch:
        patch.setattr(cutover,'ROOT_UID',os.geteuid()+1)
        with pytest.raises(ModError):
            with journal.transaction(**transaction):pass


@pytest.mark.parametrize('event', [dict(kind='quiescent',label=None,time=1),
    dict(kind='stop-observed',label='foreign',time=1),dict(kind='settled',label=None,time=True),
    dict(kind='settled',label=None,time=1001),dict(kind='unknown',label=None,time=1)])
def test_invalid_event_sequences_are_refused_without_filesystem_access(event,workshop_routing):
    with pytest.raises(ModError):journal.replay([dict(event,data=None)],lambda:1000,workshop_routing['baseline'],'a'*64,'b'*64,600,'8'*64,'7'*64,'d'*64)


def test_root_journal_encoding_and_replay_contract_without_filesystem_access(workshop_routing):
    assert journal.SHA.fullmatch('a'*64) and not journal.SHA.fullmatch('a'*64+'\n')
    baseline=workshop_routing['baseline']
    workshop_routing['invoke']('suspend')
    off=routes.snapshot(workshop_routing['invoke']('status'),baseline['origin'])
    events=[dict(kind='routing-intent',label=None,time=1,data=dict(baseline,expose=False)),
            dict(kind='routing-observed',label=None,time=1,data=dict(off,expose=False))]
    for label in journal.stop_order():
        events.extend([dict(kind='stop-intent',label=label,time=1,data=None),dict(kind='stop-observed',label=label,time=1,data=None)])
    events.append(dict(kind='quiescent',label=None,time=2,data=None))
    assert journal.replay(events,lambda:1000,baseline,'a'*64,'b'*64,600,'8'*64,'7'*64,'d'*64)['quiescent']
    with pytest.raises(ModError):journal.replay(events+[events[-1]],lambda:1000,baseline,'a'*64,'b'*64,600,'8'*64,'7'*64,'d'*64)


def test_real_routing_adapter_commits_intent_before_action_and_rechecks_live_suspension(transaction,workshop_routing):
    with journal.transaction(**transaction) as state:
        actions=[]
        def invoke(action):
            if action=='suspend':assert state.state()['routing_pending'] is False
            actions.append(action);return workshop_routing['invoke'](action)
        routes.transition(state.base['routing'],False,invoke,state.routing_event)
        assert actions==['status','suspend','status'] and state.state()['routing_suspended']
        state.require_suspended(invoke)
        count=len(json.loads(state.current)['events'])
        routes.transition(state.base['routing'],False,invoke,state.routing_event,resume=True)
        assert len(json.loads(state.current)['events'])==count
        workshop_routing['invoke']('restore')
        with pytest.raises(ModError):state.require_suspended(invoke)
        routes.transition(state.base['routing'],False,invoke,state.routing_event)
        assert state.state()['routing_suspended']


def test_public_restore_requires_fresh_private_verification_and_pending_restore_can_refresh(transaction,workshop_routing):
    now=[1000];transaction=dict(transaction,clock=lambda:now[0])
    with journal.transaction(**transaction) as state:
        suspend(state,workshop_routing)
        with pytest.raises(ModError):routes.transition(state.base['routing'],True,workshop_routing['invoke'],state.routing_event)
        assert not state.state()['routing_restored']
        with pytest.raises(ModError):state.record('access-verified',data=dict(release=state.base['after'],sha256='9'*64))
        state.record('access-verified',data=dict(release=state.base['before'],sha256='9'*64))
        now[0]=1301
        with pytest.raises(ModError):routes.transition(state.base['routing'],True,workshop_routing['invoke'],state.routing_event)
        state.record('access-verified',data=dict(release=state.base['before'],sha256='8'*64))
        off=routes.snapshot(workshop_routing['invoke']('status'),state.base['routing']['origin'])
        state.routing_event('routing-intent',dict(off,expose=True))
        now[0]=1602
        with pytest.raises(ModError):routes.transition(state.base['routing'],True,workshop_routing['invoke'],state.routing_event,resume=True)
        assert state.state()['routing_pending'] is True
        state.record('access-verified',data=dict(release=state.base['before'],sha256='7'*64))
        routes.transition(state.base['routing'],True,workshop_routing['invoke'],state.routing_event,resume=True)
        state.record('settled')


def test_cancel_unapplied_suspension_requires_exact_original_public_state(transaction):
    with journal.transaction(**transaction) as state:
        public=dict(state.base['routing'],expose=False)
        state.routing_event('routing-intent',public)
        with pytest.raises(ModError):state.record('settled')
        bad=dict(public,config_sha256='0'*64)
        with pytest.raises(ModError):state.record('routing-unchanged',data=bad)
        state.record('routing-unchanged',data=public)
        state.record('settled')


@pytest.mark.parametrize('damage', [None,'truncated','foreign','two-events','duplicate','legacy'])
def test_explicit_pending_recovery_only_promotes_one_complete_known_append(transaction,monkeypatch,damage):
    with journal.transaction(**transaction) as state:
        with monkeypatch.context() as patch:
            patch.setattr(journal.os,'replace',lambda *args,**kwargs:(_ for _ in ()).throw(OSError('rename interrupted')))
            with pytest.raises(OSError):state.routing_event('routing-intent',dict(state.base['routing'],expose=False))
        pending=state.directory/(state.name+'.pending')
        old=state.current;value=json.loads(pending.read_bytes())
    if damage=='truncated':pending.write_bytes(b'{')
    elif damage=='foreign':value['nonce']='9'*64;pending.write_bytes(journal.wire(value))
    elif damage=='two-events':
        value['events'].append(dict(kind='routing-unchanged',label=None,time=1000,data=dict(transaction['routing'],expose=False)))
        pending.write_bytes(journal.wire(value))
    elif damage=='duplicate':pending.write_bytes(old)
    elif damage=='legacy':value['schema']=1;value['producer']='tsfp-root-cutover-stop-v1';pending.write_bytes(journal.wire(value))
    saved=pending.read_bytes()
    with pytest.raises(ModError):
        with journal.transaction(**transaction):pass
    if damage:
        with pytest.raises((ModError,OSError)):
            with journal.transaction(**transaction,recover_pending=True):pass
        assert pending.read_bytes()==saved and (pending.parent/(transaction['nonce']+'.json')).read_bytes()==old
    else:
        with journal.transaction(**transaction,recover_pending=True) as state:
            assert state.state()['routing_pending'] is False and not state.state()['routing_suspended']
            assert not pending.exists()
            with pytest.raises(ModError):state.record('stop-intent',journal.stop_order()[0])
        assert (pending.parent/(transaction['nonce']+'.json')).read_bytes()==saved
