"""Private CLI dispatch; real protected metadata with simulated verified inputs/effects."""
import importlib.util
import json
from pathlib import Path
import sys
from contextlib import contextmanager

import pytest
from test_host_cutover import protected_jobs
from test_upgrade_context import saved_inputs

SOURCE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SOURCE/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_upgrade_context as contexts
import mod_host_upgrade_coordinator as coordinator
import mod_host_migration as migration
import mod_host_reboot_prepare as reboot_preparation
import mod_host_reboot_admission as reboot_admission
from mod_package import ModError


def entry_module():
    spec=importlib.util.spec_from_file_location('upgrade_command_entry',SOURCE/'tools/mods/hosting/upgrade-installed-host-mac.py')
    entry=importlib.util.module_from_spec(spec);spec.loader.exec_module(entry);return entry


@pytest.fixture
def command(saved_inputs,monkeypatch):
    item=saved_inputs;entry=entry_module();calls=[]
    def initial(*args):calls.append('initial');return item['context']
    def verified(*args):calls.append('verified');return dict(item['fresh'])
    def apply(journal,context,*,resume):
        assert item['path'].exists() and context['plan']==item['context']['plan']
        with pytest.raises(ModError):
            with storage.transaction(**item['arguments'],existing_only=True):pass
        if journal.state()['private_baseline'] is None:
            journal.record('baseline-private-verified',data=context['private_baseline'])
        calls.append(('apply',resume,context['report'].get('phase','initial')))
        return dict(phase='fixture-private-admission',public_endpoint_started=False)
    def rollback(journal,context,*,resume):
        assert context['operator'] is item['fresh']['operator'] and item['path'].exists()
        calls.append(('rollback',resume,context['report']['phase']))
        return dict(phase='fixture-private-rollback',public_endpoint_started=False)
    def restore(journal,context,*,resume):
        assert context['operator'] is item['fresh']['operator'] and item['path'].exists()
        calls.append(('restore',resume,context['report']['phase']))
        return dict(phase='fixture-public-restoration',public_endpoint_verified=True)
    monkeypatch.setattr(entry,'prepare_context',initial);monkeypatch.setattr(entry,'verified_inputs',verified)
    monkeypatch.setattr(coordinator,'apply_private',apply);monkeypatch.setattr(coordinator,'rollback_private',rollback)
    monkeypatch.setattr(coordinator,'restore_public',restore)
    def run(action,**kwargs):
        return entry.run_action('a'*64,'b'*64,600,'a'*64,action,'c'*64,**kwargs)
    return item,entry,calls,run


def test_fresh_apply_seals_under_lease_and_recovery_uses_historical_context(command):
    item,entry,calls,run=command
    result=run('apply',expected_preflight=item['arguments']['preflight_sha256'])
    assert result['nonce']=='c'*64 and result['public_endpoint_started'] is False
    assert calls==['initial',('apply',False,'initial')]
    calls.clear();run('resume')
    assert calls==['verified',('apply',True,'historical-upgrade-context-reconstructed')]
    calls.clear();run('rollback')
    assert calls==['verified',('rollback',False,'historical-upgrade-context-reconstructed')]
    calls.clear();run('rollback',resume_action=True)
    assert calls==['verified',('rollback',True,'historical-upgrade-context-reconstructed')]
    calls.clear();run('restore')
    assert calls==['verified',('restore',False,'historical-upgrade-context-reconstructed')]
    calls.clear();run('restore',resume_action=True)
    assert calls==['verified',('restore',True,'historical-upgrade-context-reconstructed')]


def test_stale_preflight_and_missing_recovery_do_not_create_history_or_run_effects(command):
    item,entry,calls,run=command
    with pytest.raises(ValueError,match='Fresh preflight differs'):run('apply',expected_preflight='0'*64)
    assert not (cutover.ROOT/'upgrades').exists() and not item['path'].exists() and calls==['initial']
    calls.clear()
    with pytest.raises(FileNotFoundError):run('resume')
    assert not (cutover.ROOT/'upgrades').exists() and calls==['verified']


def test_fresh_apply_cannot_reopen_an_existing_nonce(command):
    item,entry,calls,run=command;expected=item['arguments']['preflight_sha256']
    run('apply',expected_preflight=expected);calls.clear()
    path=cutover.ROOT/'upgrades'/('c'*64+'.json');original=path.read_bytes();context=item['path'].read_bytes()
    with pytest.raises(ModError):run('apply',expected_preflight=expected)
    assert calls==['initial'] and path.read_bytes()==original and item['path'].read_bytes()==context


@pytest.mark.parametrize('partial',[False,True])
def test_unused_transaction_requires_exact_context_recovery_and_preserves_partial_files(command,monkeypatch,partial):
    item,entry,calls,run=command;seal=contexts.seal
    def interrupted(journal,context,**kwargs):
        if partial:
            item['path'].parent.mkdir(mode=0o700);item['path'].write_bytes(b'{');item['path'].chmod(0o600)
        else:seal(journal,context,**kwargs)
        raise OSError('interrupted context admission')
    with monkeypatch.context() as patch:
        patch.setattr(contexts,'seal',interrupted)
        with pytest.raises(OSError):run('apply',expected_preflight=item['arguments']['preflight_sha256'])
    assert calls==['initial']
    path=cutover.ROOT/'upgrades'/('c'*64+'.json');assert json.loads(path.read_bytes())['events']==[]
    calls.clear()
    if partial:
        with pytest.raises(ModError):run('resume')
        assert item['path'].read_bytes()==b'{' and calls==['verified','initial']
    else:
        run('resume');assert calls==['verified','initial',('apply',True,'initial')]


@pytest.mark.parametrize('arguments',[
    ['--recover-boot','f'*64],
    ['--preflight','--from-release','a'*64,'--backup-release','8'*64,'--recover-boot','f'*64],
    ['--apply-private','--from-release','a'*64,'--backup-release','8'*64,'--nonce','c'*64,'--expected-preflight','e'*64,'--recover-boot','f'*64],
    ['--resume-private','--from-release','a'*64,'--backup-release','8'*64,'--nonce','c'*64,'--recover-boot','../bad'],
    ['--nonce','c'*64],['--apply-private','--from-release','a'*64,'--backup-release','a'*64,'--nonce','c'*64],
    ['--inspect','--from-release','a'*64,'--nonce','c'*64],
    ['--preflight','--from-release','a'*64,'--backup-release','a'*64,'--resume'],
    ['--resume-private','--from-release','a'*64,'--backup-release','a'*64,'--nonce','../bad'],
    ['--rollback-private','--from-release','a'*64,'--backup-release','a'*64,'--nonce','c'*64,'--expected-preflight','e'*64],
    ['--restore-public','--from-release','a'*64,'--backup-release','a'*64],
    ['--restore-public','--from-release','a'*64,'--backup-release','a'*64,'--nonce','c'*64,'--expected-preflight','e'*64],
])
def test_invalid_cli_options_refuse_before_root_admission(monkeypatch,arguments,capsys):
    entry=entry_module();monkeypatch.setattr(sys,'argv',['upgrade',*arguments])
    monkeypatch.setattr(entry,'root_context',lambda *_:pytest.fail('Authenticated before argument validation'))
    assert entry.main()==2 and 'Installed host upgrade:' in capsys.readouterr().err


@pytest.mark.parametrize('flag,extra,mode,resuming',[
    ('--apply-private',['--expected-preflight','e'*64],'apply',False),
    ('--resume-private',[],'resume',False),('--rollback-private',['--resume'],'rollback',True),
    ('--restore-public',[],'restore',False),('--restore-public',['--resume'],'restore',True),
])
def test_private_cli_authentication_precedes_action_and_valid_arguments_dispatch(monkeypatch,capsys,flag,extra,mode,resuming):
    entry=entry_module();calls=[]
    monkeypatch.setattr(sys,'argv',['upgrade',flag,'--from-release','a'*64,'--backup-release','8'*64,'--nonce','c'*64,*extra])
    def refuse(*args):calls.append('authentication');raise ValueError('fixture authentication refusal')
    monkeypatch.setattr(entry,'root_context',refuse)
    monkeypatch.setattr(entry,'run_action',lambda *args,**kwargs:pytest.fail('Ran before Root admission'))
    assert entry.main()==2 and calls==['authentication'];capsys.readouterr()
    monkeypatch.setattr(entry,'root_context',lambda before:('b'*64,600))
    def run(*args,**kwargs):calls.append((args,kwargs));return dict(phase='fixture',public_endpoint_started=False)
    monkeypatch.setattr(entry,'run_action',run)
    assert entry.main()==0 and json.loads(capsys.readouterr().out)['public_endpoint_started'] is False
    assert calls[-1]==(('a'*64,'b'*64,600,'8'*64,mode,'c'*64),
        dict(expected_preflight='e'*64 if mode=='apply' else None,resume_action=resuming))


@pytest.mark.parametrize('flag,mode',[('--resume-private','resume'),('--rollback-private','rollback'),('--restore-public','restore')])
def test_reboot_cli_authenticates_before_forwarding_exact_original_boot(monkeypatch,capsys,flag,mode):
    entry=entry_module();calls=[]
    monkeypatch.setattr(sys,'argv',['upgrade',flag,'--from-release','a'*64,'--backup-release','8'*64,
        '--nonce','c'*64,'--recover-boot','f'*64])
    monkeypatch.setattr(entry,'root_context',lambda *_:(_ for _ in ()).throw(ValueError('authentication refused')))
    monkeypatch.setattr(entry,'run_action',lambda *a,**k:pytest.fail('Dispatched before authentication'))
    assert entry.main()==2;capsys.readouterr()
    monkeypatch.setattr(entry,'root_context',lambda *_:('b'*64,600))
    def run(*args,**kwargs):calls.append((args,kwargs));return dict(phase='fixture-reboot')
    monkeypatch.setattr(entry,'run_action',run)
    assert entry.main()==0 and json.loads(capsys.readouterr().out)['phase']=='fixture-reboot'
    assert calls==[(('a'*64,'b'*64,600,'8'*64,mode,'c'*64),
        dict(expected_preflight=None,resume_action=False,recover_boot='f'*64))]


@pytest.mark.parametrize('action',['resume','rollback','restore'])
@pytest.mark.parametrize('admission_state',[None,'preparing','admitted','prior-admitted'])
def test_reboot_dispatch_uses_original_existing_exclusive_lease_without_initial_preflight(command,monkeypatch,action,admission_state):
    item,entry,calls,run=command
    already_admitted=admission_state=='admitted'
    run('apply',expected_preflight=item['arguments']['preflight_sha256']);calls.clear()
    journal_path=cutover.ROOT/'upgrades'/('c'*64+'.json');original=journal_path.read_bytes();context=item['path'].read_bytes()
    monkeypatch.setattr(storage,'boot_identity',lambda:'e'*64);monkeypatch.setattr(migration,'boot_identity',lambda:'e'*64)
    real_lease=storage.reboot_observation_lease
    @contextmanager
    def lease(*args,**kwargs):
        with real_lease(*args,**kwargs) as journal:
            assert journal.base['boot_id']=='f'*64 and journal.observation_boot=='e'*64 and journal.current==original
            if admission_state is not None:
                state=journal.state()
                monkeypatch.setattr(journal,'state',lambda:dict(state,reboot=dict(
                    boot_id=('9' if admission_state=='prior-admitted' else 'e')*64,
                    admitted=dict(time=1000000000) if admission_state in ('admitted','prior-admitted') else None)))
            yield journal
    monkeypatch.setattr(storage,'reboot_observation_lease',lease)
    def prepare(journal,fresh,*,resume):calls.append(('prepare-boot',resume))
    def admit(journal,fresh,*,resume):calls.append(('admit-boot',resume))
    def reconstruct(journal,fresh):
        calls.append('reconstruct')
        return dict(item['context'],report=dict(item['context']['report'],phase='historical-upgrade-context-reconstructed'))
    monkeypatch.setattr(reboot_preparation,'prepare',prepare);monkeypatch.setattr(reboot_admission,'admit',admit)
    monkeypatch.setattr(contexts,'reconstruct',reconstruct)
    monkeypatch.setattr(entry,'prepare_context',lambda *_:pytest.fail('Initial/private preflight during reboot recovery'))
    if action=='restore' and not already_admitted:
        with pytest.raises(ValueError,match='private resume or rollback'):run(action,recover_boot='f'*64)
        assert calls==['verified']
    else:
        result=run(action,recover_boot='f'*64)
        assert result['original_boot']=='f'*64 and result['recovered_boot']=='e'*64
        expected=['verified']+([] if already_admitted else [('prepare-boot',admission_state is not None),('admit-boot',True)])+[
            'reconstruct',(action if action!='resume' else 'apply',True,'historical-upgrade-context-reconstructed')]
        assert calls==expected
    assert journal_path.read_bytes()==original and item['path'].read_bytes()==context


@pytest.mark.parametrize('original_boot',['f'*64,'9'*64,'e'*64])
def test_reboot_dispatch_does_not_create_missing_or_adopt_wrong_history(command,monkeypatch,original_boot):
    item,entry,calls,run=command
    if original_boot!='f'*64:
        run('apply',expected_preflight=item['arguments']['preflight_sha256']);calls.clear()
    monkeypatch.setattr(storage,'boot_identity',lambda:'e'*64);monkeypatch.setattr(migration,'boot_identity',lambda:'e'*64)
    with pytest.raises((OSError,ValueError)):run('resume',recover_boot=original_boot)
    assert calls==['verified']
    if original_boot=='f'*64:assert not (cutover.ROOT/'upgrades').exists()


@pytest.mark.parametrize('action,boot',[('apply','f'*64),('resume','../bad'),('rollback',True),('restore',[])])
def test_invalid_direct_reboot_selection_refuses_before_preflight_or_input_verification(monkeypatch,action,boot):
    entry=entry_module()
    monkeypatch.setattr(entry,'prepare_context',lambda *_:pytest.fail('Unexpected initial preflight'))
    monkeypatch.setattr(entry,'verified_inputs',lambda *_:pytest.fail('Unexpected input verification'))
    with pytest.raises(ValueError):entry.run_action('a'*64,'b'*64,600,'8'*64,action,'c'*64,
        expected_preflight='e'*64 if action=='apply' else None,recover_boot=boot)
