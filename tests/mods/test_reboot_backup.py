"""Real stopped reboot journal/layout with a simulated locked backup child."""
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
import mod_host_reboot_backup as backup
import mod_host_reboot_checkpoint as checkpoints
import mod_host_upgrade_process as process
from mod_package import ModError

pytestmark=[pytest.mark.parametrize('prepared',['deferred'],indirect=True),
            pytest.mark.parametrize('flow',['fresh'],indirect=True)]


def test_reboot_capture_preserves_independent_binding_and_reuses_only_fresh_receipt(rebooted):
    case=rebooted;checkpoints.seal(case.journal,case.fresh)
    count=len(case.item.captures);original=case.journal.state()['backup']
    result=backup.capture(case.journal,case.fresh)
    assert result['operational_admission'] is False and result['reused'] is False and len(case.item.captures)==count+1
    assert result['receipt']['identity']==case.journal.base['backup_identity'] and result['receipt']['base_release']=='7'*64
    assert case.journal.state()['backup']==original  # Old frame authority is not upgraded by this phase.
    with pytest.raises(ModError):backup.capture(case.journal,case.fresh)
    committed=case.journal.current
    assert backup.capture(case.journal,case.fresh,resume=True)['reused'] is True
    assert len(case.item.captures)==count+1 and case.journal.current==committed
    case.item.flow.now[0]+=301
    renewed=backup.capture(case.journal,case.fresh,resume=True)
    assert renewed['reused'] is False and len(case.item.captures)==count+2
    assert renewed['receipt']['finished']==case.item.flow.now[0]
    with pytest.raises(ModError):case.journal.record('settled')


def test_uncertain_capture_requires_explicit_new_verification_after_quiescence(rebooted,monkeypatch):
    case=rebooted;checkpoints.seal(case.journal,case.fresh);record=case.journal.record_reboot
    def interrupt(kind,*args,**kwargs):
        if kind=='reboot-backup-verified':raise OSError('lost receipt observation')
        return record(kind,*args,**kwargs)
    count=len(case.item.captures)
    with monkeypatch.context() as patch:
        patch.setattr(case.journal,'record_reboot',interrupt)
        with pytest.raises(OSError):backup.capture(case.journal,case.fresh)
    assert case.journal.state()['reboot']['backup_pending'] is not None and len(case.item.captures)==count+1
    with pytest.raises(ModError):backup.capture(case.journal,case.fresh)
    result=backup.capture(case.journal,case.fresh,resume=True)
    assert len(case.item.captures)==count+2 and result['reused'] is False
    assert case.journal.state()['reboot']['backup_pending'] is None


@pytest.mark.parametrize('fault',['uid','runtime','unverified','future','zero-size','changed-checkpoint','orphan'])
def test_bad_receipts_or_changed_post_child_state_never_admit_reboot_backup(rebooted,monkeypatch,fault):
    case=rebooted;checkpoints.seal(case.journal,case.fresh);force=process.forced_backup
    def child(*args,**kwargs):
        report=force(*args,**kwargs)
        if fault=='uid':report['uid']=True
        elif fault=='runtime':report['base_release']='b'*64
        elif fault=='unverified':report['verified']=False
        elif fault=='future':report['finished']+=1
        elif fault=='zero-size':report['bytes']=0
        elif fault=='changed-checkpoint':case.path.write_bytes(case.path.read_bytes()+b'\n')
        elif fault=='orphan':case.item.loaded['foreign']=(Path('/unknown'),dict(Label='foreign'),999)
        return report
    monkeypatch.setattr(process,'forced_backup',child)
    with pytest.raises(ModError):backup.capture(case.journal,case.fresh)
    state=case.journal.state()['reboot']
    assert state['backup'] is None and state['backup_pending'] is not None
    with pytest.raises(ModError):case.journal.record('settled')


def test_missing_checkpoint_and_changed_runtime_binding_refuse_before_capture(rebooted):
    case=rebooted;count=len(case.item.captures)
    with pytest.raises(ModError):backup.capture(case.journal,case.fresh)
    assert len(case.item.captures)==count
    checkpoints.seal(case.journal,case.fresh)
    changed=dict(case.fresh,backup_binding=dict(case.fresh['backup_binding'],base_release='a'*64))
    with pytest.raises(ModError):backup.capture(case.journal,changed)
    assert len(case.item.captures)==count and case.journal.state()['reboot']['backup_pending'] is None


def test_replayed_backup_evidence_requires_intent_checkpoint_epoch_and_verified_receipt(rebooted):
    case=rebooted;checkpoints.seal(case.journal,case.fresh);backup.capture(case.journal,case.fresh)
    value=json.loads(case.journal.current)
    index=next(i for i,e in enumerate(value['events']) if e['kind']=='reboot-backup-verified')
    for fault in ('no-intent','checkpoint','uid','identity','base','verified','size','future','unknown'):
        changed=copy.deepcopy(value);event=changed['events'][index];report=event['data']['receipt']
        if fault=='no-intent':del changed['events'][index-1]
        elif fault=='checkpoint':event['data']['checkpoint_sha256']='d'*64
        elif fault=='uid':report['uid']=True
        elif fault=='identity':report['identity']='d'*64
        elif fault=='base':report['base_release']='d'*64
        elif fault=='verified':report['verified']=False
        elif fault=='size':report['bytes']=0
        elif fault=='future':report['finished']=event['time']+1
        elif fault=='unknown':report['private_payload']='canary'
        with pytest.raises(ModError):storage.checked(storage.wire(changed),case.journal.clock)
