"""Composed current-boot admission and ordinary recovery over real journal/files."""
import copy
import json
from pathlib import Path
import sys

import pytest
from test_upgrade_layout import prepared
from test_upgrade_flow import flow
from test_upgrade_coordinator import machine
from test_upgrade_activation import simulated_boot
from test_reboot_checkpoint import rebooted

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover_journal as storage
import mod_host_reboot_admission as admission
import mod_host_reboot_checkpoint as checkpoints
import mod_host_upgrade_activation as activation
import mod_host_upgrade_admission as proofs
import mod_host_upgrade_coordinator as coordinator
import mod_host_upgrade_layout as layouts
import mod_host_upgrade_shutdown as shutdown
from mod_package import ModError

pytestmark=[pytest.mark.parametrize('prepared',['deferred'],indirect=True),
            pytest.mark.parametrize('flow',['fresh'],indirect=True)]


@pytest.fixture
def admitted(rebooted,monkeypatch):
    case=rebooted
    for module in (activation,shutdown):monkeypatch.setattr(module,'boot_identity',lambda:'e'*64)
    case.original=case.journal.current;case.original_base=copy.deepcopy(case.journal.base)
    admission.admit(case.journal,case.fresh)
    return case


@pytest.mark.parametrize('rebooted',['early','applied'],indirect=True)
@pytest.mark.parametrize('reverse',[False,True])
def test_new_boot_preserves_history_and_reestablishes_private_startup_before_public_restore(admitted,reverse):
    case=admitted;state=case.journal;frame=state.state()
    assert state.base==case.original_base and state.base['boot_id']=='f'*64
    assert frame['reboot']['admitted']['boot_id']=='e'*64 and frame['quiesced_at']==1000000010
    assert not frame['started'] and not frame['activation_ready'] and frame['scanner'] is None and frame['private_ready'] is None
    original=json.loads(case.original)['events'];assert json.loads(state.current)['events'][:len(original)]==original
    assert storage.operational_boot(state)=='e'*64
    assert storage.operational_tracking(state)==frame['reboot']['observation']['tracked']!=state.base['tracked']
    assert frame['catalog_floor']>=40 and frame['routing_suspended'] and not frame['routing_restored']
    with pytest.raises(ModError):state.record('settled')
    coordinator.apply_private(state,case.fresh,resume=True)
    if reverse:coordinator.rollback_private(state,case.fresh,resume=True)
    result=coordinator.restore_public(state,case.fresh,resume=True)
    assert result['generation']==('before' if reverse else 'after') and state.state()['settled']
    assert state.base==case.original_base and storage.operational_boot(state)=='e'*64
    if reverse and any(event['kind']=='layout-prepared' for event in original):
        assert 256<len(json.loads(state.current)['events'])<=storage.MAX_EVENTS
    assert len(state.current)<=storage.MAX_BYTES


def test_admission_retains_historical_checkpoint_after_layout_moves_and_expires_only_at_event(admitted):
    case=admitted;state=case.journal
    sha=state.state()['reboot']['layout_checkpoint']['checkpoint_sha256'];historical=checkpoints.load(state,sha)
    case.item.flow.now[0]+=301
    assert storage.operational_boot(state)=='e'*64  # Subsequent operations establish their own fresh gates.
    coordinator.rollback_private(state,case.fresh,resume=True)
    assert checkpoints.load(state,sha)==historical and historical['locations']['active']=='after'
    assert state.state()['layout_complete']=='before'


def test_replayed_admission_refuses_missing_prerequisites_or_changed_binding(admitted):
    case=admitted;value=json.loads(case.journal.current)
    index=next(i for i,e in enumerate(value['events']) if e['kind']=='reboot-admitted')
    for fault in ('no-backup','no-checkpoint','boot','checkpoint','backup','future','private','extra'):
        changed=copy.deepcopy(value);event=changed['events'][index]
        if fault=='no-backup':del changed['events'][index-1]
        elif fault=='no-checkpoint':changed['events']=[e for e in changed['events'] if e['kind']!='reboot-layout-verified']
        elif fault=='boot':event['data']['boot_id']='d'*64
        elif fault=='checkpoint':event['data']['checkpoint_sha256']='d'*64
        elif fault=='backup':event['data']['backup_sha256']='d'*64
        elif fault=='future':event['time']+=301
        elif fault=='private':event['data']['private_baseline']['release']='d'*64
        else:event['data']['trust_me']=True
        with pytest.raises(ModError):storage.checked(storage.wire(changed),lambda:case.journal.clock()+301)
    with pytest.raises(ModError):admission.admit(case.journal,case.fresh,resume=True)


def test_later_boot_or_missing_current_lease_cannot_use_admitted_history(admitted,monkeypatch):
    case=admitted
    with monkeypatch.context() as patch:
        patch.setattr(storage,'boot_identity',lambda:'9'*64)
        with pytest.raises(ModError):storage.operational_boot(case.journal)
        with pytest.raises(ModError):case.journal.record('settled')
    case.journal.observation_boot=None
    with pytest.raises(ModError):storage.operational_boot(case.journal)
    with pytest.raises(ModError):case.journal.record('settled')


def test_migration_proof_requires_capture_and_quiescence_from_its_own_boot(admitted):
    case=admitted;base=case.journal.base;frame=case.journal.state();report=frame['backup']
    value=dict(schema=1,producer=proofs.PRODUCER,**{key:base[key] for key in ('before','after','uid','nonce','plan_sha256')},
        backup_sha256=report['verified_manifest_sha256'],time=int(case.journal.clock()),stopped_jobs=proofs.migration.jobs(),
        quiesced_at=frame['quiesced_at'],backup_verified_at=report['finished'],boot_id='e'*64)
    assert proofs.checked_existing(case.journal,storage.wire(value))==value
    for boot in ('f'*64,'9'*64,[],True):
        with pytest.raises(ModError):proofs.checked_existing(case.journal,storage.wire(dict(value,boot_id=boot)))


def test_quiescence_ignores_an_unrelated_reused_preboot_pid(admitted):
    case=admitted;command=case.fresh['command']
    assert 100 in case.journal.base['tracked']['pids'] and 100 not in storage.operational_tracking(case.journal)['pids']
    def reused(argv):
        code,output=command(argv)
        if argv[0]=='/bin/ps':output+='100 1 100 501\n'
        return code,output
    layouts.quiescence_gate(case.journal,case.fresh['candidate_entries'],reused,case.fresh['operator'])


@pytest.mark.parametrize('rebooted',['archive','install','boot','migration','retire','start','restart','public'],indirect=True)
def test_known_preboot_pending_effects_reconcile_before_new_private_and_public_acceptance(admitted):
    case=admitted;original=json.loads(case.original)['events']
    pending=case.journal.state()['layout_pending']
    coordinator.apply_private(case.journal,case.fresh,resume=True)
    coordinator.restore_public(case.journal,case.fresh,resume=True)
    assert case.journal.state()['settled'] and case.journal.state()['config']=='after'
    if pending is not None:
        assert sum(event['kind']=='layout-intent' and event['label']==pending for event in json.loads(case.journal.current)['events'])==1
    assert json.loads(case.journal.current)['events'][:len(original)]==original
