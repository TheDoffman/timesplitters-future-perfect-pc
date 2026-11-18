"""CLI phases over sealed original context, real journal/files and simulated OS.

The original fixture already holds the real exclusive Root lease. Its lease
adapter yields that object; separate dispatch tests cover acquisition/reopen.
Closures, process control, scanner and locked children remain simulated.
"""
from contextlib import contextmanager
import json
from pathlib import Path
import sys

import pytest
from test_upgrade_layout import prepared
from test_upgrade_flow import flow
from test_upgrade_coordinator import machine
from test_upgrade_activation import simulated_boot
from test_reboot_checkpoint import rebooted
from test_upgrade_command import entry_module

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_upgrade_activation as activation
import mod_host_upgrade_shutdown as shutdown
from mod_package import ModError

pytestmark=[pytest.mark.parametrize('prepared',['deferred'],indirect=True),
            pytest.mark.parametrize('flow',['fresh-bound'],indirect=True)]


@pytest.mark.parametrize('rebooted',['early','applied','archive','migration','start','public'],indirect=True)
@pytest.mark.parametrize('reverse',[False,True])
def test_command_composes_real_sealed_context_admission_recovery_and_public_restoration(rebooted,monkeypatch,reverse):
    case=rebooted;entry=entry_module();journal=case.journal
    for module in (activation,shutdown):monkeypatch.setattr(module,'boot_identity',lambda:'e'*64)
    path=cutover.ROOT/'upgrade-contexts'/(journal.base['nonce']+'.json');original=path.read_bytes();base=dict(journal.base)
    def verified(*args):return case.fresh
    monkeypatch.setattr(entry,'verified_inputs',verified)
    monkeypatch.setattr(entry,'prepare_context',lambda *_:pytest.fail('Initial preflight against changed private data'))
    @contextmanager
    def held(nonce,before,after,uid,backup,boot,*,recover_pending):
        assert (nonce,before,after,uid,backup,boot,recover_pending)==('c'*64,'a'*64,'b'*64,600,'8'*64,'f'*64,True)
        with pytest.raises(ModError):
            with storage.transaction(**case.item.flow.arguments,existing_only=True):pass
        yield journal
    monkeypatch.setattr(storage,'reboot_observation_lease',held)
    unstaged=journal.state()['layout_sha256'] is None
    previous=len(json.loads(journal.current)['events'])
    action='rollback' if reverse else 'resume'
    result=entry.run_action('a'*64,'b'*64,600,'8'*64,action,'c'*64,recover_boot='f'*64)
    if reverse:
        assert {event['data'] for event in json.loads(journal.current)['events'][previous:] if event['kind']=='start-intent'}=={'before'}
        if unstaged:
            assert case.item.flow.calls==[] and not any(event['kind']=='migration-intent' for event in json.loads(journal.current)['events'])
    assert result['public_endpoint_started'] is False and journal.state()['activation_ready']
    result=entry.run_action('a'*64,'b'*64,600,'8'*64,'restore','c'*64,recover_boot='f'*64)
    assert result['public_endpoint_verified'] and result['original_boot']=='f'*64 and result['recovered_boot']=='e'*64
    assert journal.state()['settled'] and journal.state()['config']==('before' if reverse else 'after')
    assert journal.base==base and path.read_bytes()==original
