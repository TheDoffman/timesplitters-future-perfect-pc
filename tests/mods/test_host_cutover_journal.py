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
from mod_package import ModError


@pytest.fixture
def transaction(private_posix_path, monkeypatch):
    root = private_posix_path/'workshop';root.mkdir(mode=0o755)
    monkeypatch.setattr(cutover, 'ROOT', root)
    monkeypatch.setattr(migration, 'ROOT', root)
    monkeypatch.setattr(cutover, 'ROOT_UID', os.geteuid())
    monkeypatch.setattr(journal, 'boot_identity', lambda:'f'*64)
    return dict(before='a'*64, after='b'*64, uid=600, nonce='c'*64,
                plan_sha256='d'*64, preflight_sha256='e'*64,
                tracked=dict(pids=[123],groups=[123]),clock=lambda:1000)


def test_stop_intent_survives_reopen_and_blocks_new_transaction(transaction):
    label = journal.stop_order()[0]
    with journal.transaction(**transaction) as state:
        assert not state.state()['stopped']
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
        state.record('settled')
    with journal.transaction(**dict(transaction, nonce='9'*64)) as state:
        state.record('settled')


def test_monitors_precede_backup_and_workers_and_quiescence_requires_all_observations(transaction):
    order = journal.stop_order()
    assert len(order) == 10 and all(v.endswith('.monitor') for v in order[:4])
    assert order[4] == 'org.timesplitters.workshop.backup'
    with journal.transaction(**transaction) as state:
        for kind,label in [('quiescent',None), ('stop-observed',order[0]), ('stop-intent',order[-1])]:
            with pytest.raises(ModError):state.record(kind,label)
        for label in order:
            state.record('stop-intent',label)
            with pytest.raises(ModError):state.record('quiescent')
            state.record('stop-observed',label)
        state.record('quiescent')
        assert state.state()['quiescent'] and not state.state()['settled']
        with pytest.raises(ModError):state.record('stop-intent',order[0])
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
        elif damage == 'future':value['events']=[dict(kind='settled',label=None,time=1001)]
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


def test_pending_write_after_interruption_is_preserved_and_refuses_resume(transaction, monkeypatch):
    with journal.transaction(**transaction) as state:
        def interrupted(*args,**kwargs):raise OSError('injected rename interruption')
        with monkeypatch.context() as patch:
            patch.setattr(journal.os,'replace',interrupted)
            with pytest.raises(OSError):state.record('stop-intent',journal.stop_order()[0])
        assert not state.state()['pending']
        pending = state.directory/(state.name+'.pending')
        assert pending.exists() and json.loads(pending.read_bytes())['events'][0]['kind']=='stop-intent'
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
def test_invalid_event_sequences_are_refused_without_filesystem_access(event):
    with pytest.raises(ModError):journal.replay([event],lambda:1000)


def test_root_journal_encoding_and_replay_contract_without_filesystem_access():
    assert journal.SHA.fullmatch('a'*64) and not journal.SHA.fullmatch('a'*64+'\n')
    events=[]
    for label in journal.stop_order():
        events.extend([dict(kind='stop-intent',label=label,time=1),dict(kind='stop-observed',label=label,time=1)])
    events.append(dict(kind='quiescent',label=None,time=2))
    assert journal.replay(events,lambda:1000)['quiescent']
    with pytest.raises(ModError):journal.replay(events+[events[-1]],lambda:1000)
