"""Original Root metadata may be leased after reboot, never used as OS authority."""
import json
from pathlib import Path
import sys

import pytest
from test_host_cutover_journal import transaction

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_migration as migration
from mod_package import ModError


@pytest.fixture
def rebooted(transaction,monkeypatch):
    arguments=transaction
    with storage.transaction(**arguments):pass
    path=cutover.ROOT/'upgrades'/('c'*64+'.json');original=path.read_bytes();boot=['e'*64]
    monkeypatch.setattr(storage,'boot_identity',lambda:boot[0])
    monkeypatch.setattr(migration,'boot_identity',lambda:boot[0])
    def lease(**kwargs):
        return storage.reboot_observation_lease('c'*64,'a'*64,'b'*64,600,'8'*64,'f'*64,clock=lambda:1000,**kwargs)
    return arguments,path,original,boot,lease


def test_old_metadata_lease_preserves_original_boot_and_disallows_upgrade_events(rebooted):
    arguments,path,original,boot,lease=rebooted
    with pytest.raises(ModError):storage.recovery_arguments('c'*64,'a'*64,'b'*64,600,'8'*64,clock=lambda:1000)
    with lease() as journal:
        assert journal.base['boot_id']=='f'*64 and journal.observation_boot=='e'*64
        assert journal.base['tracked']==dict(pids=[123],groups=[123])
        assert not journal.state()['settled']
        with pytest.raises(ModError):journal.record('settled')
        with pytest.raises(ModError):
            with storage.transaction(**arguments,existing_only=True):pass
        with pytest.raises(ModError):
            cutover.quiesce([],600,journal,lambda *_:pytest.fail('Ran OS command using old tracking'),routing=lambda *_:b'')
    assert path.read_bytes()==original


@pytest.mark.parametrize('fault',['same-boot','foreign-boot','boolean-uid','foreign-release','unknown-nonce','changed-current-boot'])
def test_reboot_lease_refuses_foreign_or_unstable_selection_without_rewriting_history(rebooted,fault):
    _,path,original,boot,_=rebooted
    args=['c'*64,'a'*64,'b'*64,600,'8'*64,'f'*64]
    if fault=='same-boot':boot[0]='f'*64
    elif fault=='foreign-boot':args[-1]='d'*64
    elif fault=='boolean-uid':args[3]=True
    elif fault=='foreign-release':args[2]='d'*64
    elif fault=='unknown-nonce':args[0]='d'*64
    with pytest.raises((ModError,OSError)):
        with storage.reboot_observation_lease(*args,clock=lambda:1000) as journal:
            assert fault=='changed-current-boot'
            boot[0]='d'*64;journal.state()
    assert path.read_bytes()==original


def test_known_pending_metadata_requires_explicit_promotion_and_grants_no_effect_authority(rebooted):
    _,path,original,boot,lease=rebooted
    value=json.loads(original)
    value['events']=[dict(kind='routing-intent',label=None,time=1000,data=dict(value['routing'],expose=False))]
    pending=path.with_name(path.name+'.pending');wire=storage.wire(value);pending.write_bytes(wire);pending.chmod(0o600)
    with pytest.raises(ModError):
        with lease():pass
    assert pending.read_bytes()==wire and path.read_bytes()==original
    with lease(recover_pending=True) as journal:
        assert journal.current==wire and journal.state()['routing_pending'] is False
        with pytest.raises(ModError):journal.record('quiescent')
    assert path.read_bytes()==wire and not pending.exists()


def test_changed_history_between_initial_read_and_lease_is_not_adopted(rebooted,monkeypatch):
    _,path,original,_,lease=rebooted;original_factory=storage._transaction
    def changed(*args,**kwargs):
        value=json.loads(original);value['tracked']=dict(pids=[456],groups=[456])
        path.write_bytes(storage.wire(value));path.chmod(0o600)
        return original_factory(*args,**kwargs)
    monkeypatch.setattr(storage,'_transaction',changed)
    with pytest.raises(ModError):
        with lease():pass
    assert json.loads(path.read_bytes())['tracked']==dict(pids=[456],groups=[456])


def test_missing_recovery_history_is_not_recreated_after_directory_inventory(rebooted,monkeypatch):
    arguments,path,_,boot,_=rebooted;boot[0]='f'*64;read=storage.Journal._read
    def vanished(journal):
        path.rename(path.with_suffix('.preserved'))
        return read(journal)
    monkeypatch.setattr(storage.Journal,'_read',vanished)
    with pytest.raises(FileNotFoundError):
        with storage.transaction(**arguments,existing_only=True):pass
    assert not path.exists() and path.with_suffix('.preserved').exists()


def test_new_nonce_cannot_adopt_history_created_after_directory_inventory(rebooted,monkeypatch):
    arguments,path,_,boot,_=rebooted;boot[0]='f'*64
    with storage.transaction(**arguments,existing_only=True) as journal:journal.record('settled')
    target=path.with_name('d'*64+'.json');read=storage.Journal._read
    def appeared(journal):
        if journal.name==target.name and not target.exists():
            target.write_bytes(storage.wire(dict(journal.base,events=[])));target.chmod(0o600)
        return read(journal)
    monkeypatch.setattr(storage.Journal,'_read',appeared)
    with pytest.raises(ModError,match='cannot adopt'):
        with storage.transaction(**dict(arguments,nonce='d'*64),new_only=True):pass
    assert json.loads(target.read_bytes())['events']==[]
