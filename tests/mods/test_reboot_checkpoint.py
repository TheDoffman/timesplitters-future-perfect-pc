"""Real reboot evidence files/journal with simulated identity, boot and services."""
import copy
import json
import os
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest
from test_upgrade_layout import prepared
from test_upgrade_flow import flow
from test_upgrade_coordinator import machine
from test_upgrade_activation import simulated_boot

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_migration as migration
import mod_host_reboot_checkpoint as checkpoint
import mod_host_reboot_layout as reboot_layout
import mod_host_reboot_observation as observations
import mod_host_reboot_prepare as reboot_prepare
import mod_host_upgrade_context as contexts
import mod_host_upgrade_coordinator as coordinator
import mod_host_upgrade_layout as layouts
from mod_package import ModError

pytestmark=[pytest.mark.parametrize('prepared',['deferred'],indirect=True),
            pytest.mark.parametrize('flow',['fresh'],indirect=True)]


@pytest.fixture
def rebooted(machine,monkeypatch):
    item=machine;coordinator.apply_private(item.state,item.context)
    # The Root lease/process identity is simulated; all protected history and
    # monitor/layout files are the actual result of the composed private phase.
    for module in (storage,migration,observations,reboot_prepare,reboot_layout,checkpoint,layouts):
        monkeypatch.setattr(module,'boot_identity',lambda:'e'*64)
    monkeypatch.setattr(contexts,'read_reboot_context',lambda *_:dict(context_sha256='9'*64))
    item.state.observation_boot='e'*64
    item.loaded.clear()
    for index,(path,spec) in enumerate(item.context['candidate_entries']):item.loaded[spec['Label']]=(path,spec,900+index)
    original=item.context['command']
    def command(argv):
        if argv[:2]==['/bin/launchctl','bootout']:
            label=argv[-1].removeprefix('system/')
            assert item.state.state()['reboot']['pending']==label
            item.loaded.pop(label,None);return 0,''
        return original(argv)
    fresh=dict(item.context,command=command)
    reboot_prepare.prepare(item.state,fresh)
    return SimpleNamespace(item=item,journal=item.state,fresh=fresh,
        path=cutover.ROOT/'reboot-checkpoints'/('c'*64+'-'+'e'*64+'.json'))


def test_sealed_checkpoint_binds_original_and_current_boot_without_effect_authority(rebooted):
    case=rebooted;before=reboot_layout.read(case.journal,case.fresh)
    result=checkpoint.seal(case.journal,case.fresh)
    assert result['operational_admission'] is False and case.path.stat().st_mode&0o777==0o600
    assert case.path.parent.stat().st_mode&0o777==0o700 and case.journal.base['boot_id']=='f'*64
    assert checkpoint.load(case.journal,result['checkpoint_sha256'])==before
    assert case.journal.state()['reboot']['layout_checkpoint']['checkpoint_sha256']==result['checkpoint_sha256']
    original=case.journal.current
    with pytest.raises(ModError):case.journal.record('settled')
    with pytest.raises(ModError):coordinator.apply_private(case.journal,case.item.context,resume=True)
    with pytest.raises(ModError):checkpoint.seal(case.journal,case.fresh)
    assert checkpoint.seal(case.journal,case.fresh,resume=True)==result and case.journal.current==original


def test_complete_checkpoint_after_lost_journal_observation_requires_explicit_resume(rebooted,monkeypatch):
    case=rebooted;record=case.journal.record_reboot
    def interrupt(kind,*args,**kwargs):
        if kind=='reboot-layout-verified':raise OSError('lost checkpoint observation')
        return record(kind,*args,**kwargs)
    with monkeypatch.context() as patch:
        patch.setattr(case.journal,'record_reboot',interrupt)
        with pytest.raises(OSError):checkpoint.seal(case.journal,case.fresh)
    original=case.path.read_bytes();assert case.journal.state()['reboot']['layout_checkpoint'] is None
    with pytest.raises(ModError):checkpoint.seal(case.journal,case.fresh)
    result=checkpoint.seal(case.journal,case.fresh,resume=True)
    assert case.path.read_bytes()==original and checkpoint.load(case.journal,result['checkpoint_sha256'])['boot_id']=='e'*64


@pytest.mark.parametrize('fault',['partial','symlink','hardlink','mode','unknown-name','capacity','changed-layout'])
def test_unknown_existing_checkpoint_or_changed_layout_is_preserved(rebooted,fault):
    case=rebooted
    folder,fd,_=checkpoint.directory(case.journal,create=True);os.close(fd)
    if fault=='capacity':
        for number in range(checkpoint.MAX_CHECKPOINTS):
            path=folder/(format(number,'064x')+'-'+'e'*64+'.json');path.write_bytes(b'{}');path.chmod(0o600)
    elif fault=='unknown-name':(folder/'preserve-unknown').write_bytes(b'keep')
    elif fault=='changed-layout':
        path=cutover.ROOT/'monitor/api/monitor.json';path.write_bytes(b'{}');path.chmod(0o600)
    elif fault in ('symlink','hardlink'):
        target=cutover.ROOT/'preserved-evidence';target.write_bytes(b'{');target.chmod(0o600)
        if fault=='symlink':case.path.symlink_to(target)
        else:os.link(target,case.path)
    else:case.path.write_bytes(b'{');case.path.chmod(0o644 if fault=='mode' else 0o600)
    names=sorted(path.name for path in folder.iterdir())
    before=case.path.lstat() if case.path.exists() or case.path.is_symlink() else None
    with pytest.raises((ModError,OSError)):checkpoint.seal(case.journal,case.fresh,resume=True)
    assert sorted(path.name for path in folder.iterdir())==names
    assert case.journal.state()['reboot']['layout_checkpoint'] is None
    if before is not None:assert cutover.stamp(case.path.lstat())==cutover.stamp(before)


def test_pinned_checkpoint_hash_and_journal_binding_reject_changed_evidence(rebooted):
    case=rebooted;result=checkpoint.seal(case.journal,case.fresh);original=case.path.read_bytes()
    case.path.write_bytes(original+b'\n');case.path.chmod(0o600)
    with pytest.raises(ModError):checkpoint.load(case.journal,result['checkpoint_sha256'])
    value=json.loads(case.journal.current)
    index=next(i for i,e in enumerate(value['events']) if e['kind']=='reboot-layout-verified')
    for fault in ('boot_id','original_boot','nonce','layout_sha256','context_sha256','checkpoint_sha256','extra'):
        changed=copy.deepcopy(value);data=changed['events'][index]['data']
        if fault=='extra':data['operational_admission']=True
        else:data[fault]='z'*64 if fault=='checkpoint_sha256' else 'd'*64
        with pytest.raises(ModError):storage.checked(storage.wire(changed),case.journal.clock)
