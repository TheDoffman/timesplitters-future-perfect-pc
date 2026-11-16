"""New-only code deployment and known-artifact scheduled-job admission/rollback."""
import hashlib
import json
import os
from pathlib import Path
import plistlib
from types import SimpleNamespace

import pytest

import mod_backup_install as install
import mod_backup_release as release

from test_backup_service import descriptor


@pytest.fixture
def layout(private_posix_path,monkeypatch):
    root=private_posix_path/'host';root.mkdir(mode=0o755);root.chmod(0o755)
    launchd=private_posix_path/'LaunchDaemons';launchd.mkdir(mode=0o755);launchd.chmod(0o755)
    monkeypatch.setattr(release,'ROOT',root);monkeypatch.setattr(release,'ROOT_UID',os.getuid());monkeypatch.setattr(install,'LAUNCHD',launchd)
    value=descriptor();value['owner_uid']=os.getuid();bundle=private_posix_path/'bundle';bundle.mkdir()
    for name in release.FILES:(bundle/name).write_bytes(b'x')
    data=release.wire(value);(bundle/'release.json').write_bytes(data)
    identity=hashlib.sha256(data).hexdigest();plan=release.plan(bundle,identity)
    python=root/'releases'/value['base_release']/'runtime/python';python.parent.mkdir(parents=True);python.write_bytes(b'fixture')
    binding=dict(schema=1,producer=release.PRODUCER,identity=identity,base_release=value['base_release'],uid=600,gid=600,python=str(python))
    return root,launchd,plan,binding


def test_stage_seals_exact_bytes_and_reuses_only_matching_inventory(layout):
    root,_,plan,binding=layout;folder=install.stage(plan,binding)
    assert release.installed(folder)==(plan['descriptor'],binding)
    assert all(p.stat().st_mode&0o777==0o444 for p in folder.iterdir())
    assert install.stage(plan,binding)==folder
    foreign=folder/'foreign';foreign.write_bytes(b'preserve')
    with pytest.raises(ValueError,match='entries'):install.stage(plan,binding)
    assert foreign.read_bytes()==b'preserve'


def test_stage_revalidates_operator_payload_before_creating_root_output(layout):
    root,_,plan,binding=layout;(plan['bundle']/next(iter(release.FILES))).write_bytes(b'y')
    with pytest.raises(ValueError,match='bytes changed'):install.stage(plan,binding)
    assert not plan['target'].exists()


def test_stage_preserves_partial_existing_release(layout):
    root,_,plan,binding=layout;plan['target'].mkdir(parents=True);unknown=plan['target']/'unknown';unknown.write_bytes(b'preserve')
    with pytest.raises((ValueError,OSError)):install.stage(plan,binding)
    assert unknown.read_bytes()==b'preserve'


def test_base_binding_requires_complete_runtime_and_locked_identity(layout,monkeypatch):
    root,_,plan,binding=layout;base=root/'releases'/binding['base_release'];python=Path(binding['python']);python.chmod(0o555)
    marker=dict(producer='tsfp-installed-host-v1',phase='complete',descriptor_sha256=base.name,release=str(base),uid=600)
    (base/'installation.json').write_bytes(release.wire(marker));metadata=base/'runtime/runtime.json'
    metadata.write_bytes(release.wire(dict(python='python')))
    monkeypatch.setattr(install,'load_helper',lambda path:SimpleNamespace(existing=lambda:dict(uid=600,gid=600)))
    assert install.base_binding(plan)==binding
    metadata.write_bytes(release.wire(dict(python='../outside')))
    with pytest.raises(ValueError,match='path'):install.base_binding(plan)
    metadata.write_bytes(release.wire(dict(python='python')));marker['phase']='preparing'
    (base/'installation.json').write_bytes(release.wire(marker))
    with pytest.raises(ValueError,match='incomplete'):install.base_binding(plan)


def test_private_parent_creates_only_new_owned_directory_and_preserves_unknown(layout):
    root,_,_,_=layout;data=root/'data';data.mkdir(mode=0o700);data.chmod(0o700)
    install.private_parent(os.getuid(),os.getgid())
    parent=data/'backups';assert parent.stat().st_mode&0o777==0o700
    original=parent/'unknown';original.write_bytes(b'preserve');parent.chmod(0o755)
    with pytest.raises(ValueError,match='unknown private'):install.private_parent(os.getuid(),os.getgid())
    assert original.read_bytes()==b'preserve' and parent.stat().st_mode&0o777==0o755


class Jobs:
    def __init__(self,fail=None):self.loaded=None;self.runs=0;self.calls=[];self.fail=fail
    def __call__(self,args):
        self.calls.append(args);action=args[1]
        if action=='print':
            return (113,'service missing') if self.loaded is None else (0,'path = '+str(self.loaded)+'\nruns = '+str(self.runs)+'\nlast exit code = 0\n')
        if action=='bootstrap':
            if self.fail=='persistent' and Path(args[-1]).parent.name=='LaunchDaemons':return 5,'failed'
            self.loaded=Path(args[-1]);self.runs=1;return 0,''
        if action=='kickstart':
            if self.fail=='repeat':return 5,'failed'
            self.runs+=1;return 0,''
        if action=='bootout':self.loaded=None;return 0,''
        raise AssertionError(args)


def admission(layout,monkeypatch,fail=None):
    root,_,plan,binding=layout;folder=install.stage(plan,binding);jobs=Jobs(fail)
    monkeypatch.setattr(install,'private_parent',lambda uid,gid:None)
    def operation(folder,binding,mode):
        if mode==fail:raise ValueError('injected admission failure')
        result={'--probe':dict(status='probe',root_metadata_denied=True),'--initialize':dict(status='captured'),
                '--verify':dict(status='verified',backup='fixture'),'--status':dict(status='not-due')}[mode]
        return dict(result=result)
    return folder,binding,jobs,operation


def test_registration_admits_staging_repeats_then_persists_and_checks_copy(layout,monkeypatch):
    folder,binding,jobs,operation=admission(layout,monkeypatch)
    result=install.register(folder,binding,call=jobs,operation=operation)
    assert result['phase']=='complete' and result['persistent_run_verified']
    final=layout[1]/(release.LABEL+'.plist')
    assert jobs.loaded==final and plistlib.loads(final.read_bytes())==release.job(folder,binding)
    assert final.stat().st_mode&0o777==0o644
    assert [args[1] for args in jobs.calls].count('bootstrap')==2
    assert [args[1] for args in jobs.calls].count('kickstart')==1
    assert result['existing_host_jobs_changed'] is False and result['public_routing_changed'] is False


@pytest.mark.parametrize('failure',['--probe','--initialize','--verify','repeat','persistent'])
def test_failed_admission_unloads_only_owned_label_and_removes_only_new_boot_file(layout,monkeypatch,failure):
    folder,binding,jobs,operation=admission(layout,monkeypatch,failure)
    with pytest.raises(ValueError):install.register(folder,binding,call=jobs,operation=operation)
    assert jobs.loaded is None and not (layout[1]/(release.LABEL+'.plist')).exists()
    assert folder.exists()  # Preserve sealed code and private data for inspection.
    journals=list((layout[0]/'backup-jobs'/folder.name).glob('attempt-*.json'))
    assert len(journals)==1 and json.loads(journals[0].read_bytes())['rollback_complete']
    assert all(args[-1]=='system/'+release.LABEL for args in jobs.calls if args[1]=='bootout')


@pytest.mark.parametrize('collision',['file','label'])
def test_registration_preserves_preexisting_startup_files_and_labels(layout,monkeypatch,collision):
    folder,binding,jobs,operation=admission(layout,monkeypatch)
    target=layout[1]/(release.LABEL+'.plist')
    if collision=='file':target.write_bytes(b'foreign')
    else:jobs.loaded=Path('/foreign/job.plist')
    with pytest.raises(ValueError,match='Preserving existing'):install.register(folder,binding,call=jobs,operation=operation)
    assert not any(args[1] in ('bootstrap','bootout','kickstart') for args in jobs.calls)
    if collision=='file':assert target.read_bytes()==b'foreign'
    else:assert jobs.loaded==Path('/foreign/job.plist')


def test_failed_cleanup_preserves_replaced_startup_artifact(layout,monkeypatch):
    folder,binding,jobs,operation=admission(layout,monkeypatch)
    final=layout[1]/(release.LABEL+'.plist');original=operation
    def changed(folder,binding,mode):
        if jobs.loaded==final and mode=='--status':
            final.chmod(0o644);final.write_bytes(b'foreign replacement')
            raise ValueError('injected persistent acceptance failure')
        return original(folder,binding,mode)
    with pytest.raises(ValueError,match='incomplete cleanup'):install.register(folder,binding,call=jobs,operation=changed)
    assert final.read_bytes()==b'foreign replacement' and jobs.loaded is None


def test_job_state_rejects_ambiguous_observation_and_failed_exit(layout):
    assert install.job_state('runs = 2\nlast exit code = 0\n')==(2,0,None)
    with pytest.raises(ValueError):install.job_state('runs = 1\nruns = 2\n')
    with pytest.raises(ValueError,match='failed'):
        install.wait_job(Path('/own'),1,lambda args:(0,'path = /own\nruns = 1\nlast exit code = 2\n'))


def test_failed_startup_file_write_removes_its_known_partial_artifact(layout,monkeypatch):
    folder,binding,jobs,operation=admission(layout,monkeypatch)
    final=layout[1]/(release.LABEL+'.plist');original=install.os.fchmod;failed=False
    def fail_once(fd,mode):
        nonlocal failed
        if not failed and final.exists() and os.fstat(fd).st_ino==final.stat().st_ino:
            failed=True;raise OSError('injected startup file mode failure')
        return original(fd,mode)
    monkeypatch.setattr(install.os,'fchmod',fail_once)
    with pytest.raises(OSError):install.register(folder,binding,call=jobs,operation=operation)
    assert failed and not final.exists() and jobs.loaded is None
