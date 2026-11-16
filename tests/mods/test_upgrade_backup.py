"""Fresh forced captures versus normal cadence; no production/root admission."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest

from test_host_backup import host
import mod_backup
import mod_backup_pool as pool

SOURCE=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('force_upgrade_backup_test',SOURCE/'tools/mods/hosting/force-upgrade-backup-mac.py')
force=importlib.util.module_from_spec(spec);spec.loader.exec_module(force)


def test_force_backup_default_is_plan_without_private_or_installed_reads():
    result=subprocess.run([sys.executable,'-B',str(SOURCE/'tools/mods/hosting/force-upgrade-backup-mac.py')],
                          capture_output=True,check=True,timeout=10)
    assert json.loads(result.stdout)==dict(schema=1,phase='plan-only',forced_capture=False,data_written=False)


def test_active_guard_precedes_any_protected_or_private_reads(monkeypatch):
    monkeypatch.setattr(force.sys,'platform','darwin')
    monkeypatch.setattr(force.os,'geteuid',lambda:501,raising=False)
    monkeypatch.setattr(force,'protected_program',lambda _:pytest.fail('Read before locked identity admission'))
    with pytest.raises(ValueError,match='isolated locked'):force.active('a'*64)
    with pytest.raises(ValueError,match='exact'):force.active('../escape')


@pytest.fixture
def private_pool(private_posix_path,host,monkeypatch):
    repository,*_=host
    root=private_posix_path/'service';root.mkdir(mode=0o700)
    for name in ('data','data/scan','data/backups'):(root/name).mkdir(mode=0o700)
    target=root/'data/scan/repository';shutil.copytree(repository.root,target)
    parent=root/'data/backups';managed=parent/'managed';pool.initialize(target,managed)
    monkeypatch.setattr(force,'ROOT',root)
    return managed,dict(identity='c'*64,base_release='a'*64),target


def test_force_captures_another_verified_snapshot_when_normal_run_is_not_due(private_pool):
    managed,binding,target=private_pool
    first=pool.run(managed,force=True)
    assert pool.run(managed)['status']=='not-due'
    report=force.force_capture(SOURCE/'tools/mods',binding,os.geteuid())
    assert report['forced'] and report['verified'] and report['status']=='captured'
    assert report['backup']!=first['backup'] and report['identity']==binding['identity']
    assert report['started']<=report['created']<=report['finished']
    assert mod_backup.verify_snapshot(managed/report['backup'])['manifest_sha256']==report['verified_manifest_sha256']
    assert (managed/first['backup']).is_dir() and target.is_dir()
    assert len(pool.entries(managed,pool.policy(managed)[1])[0])==2


def test_changed_policy_refuses_before_fresh_capture_and_preserves_previous_backup(private_pool):
    managed,binding,_=private_pool
    first=pool.run(managed,force=True)
    path=managed/pool.POLICY;value=json.loads(path.read_bytes());value['interval']=3600
    path.write_text(json.dumps(value))
    with pytest.raises(ValueError,match='policy differs'):force.force_capture(SOURCE/'tools/mods',binding,os.geteuid())
    assert len(pool.entries(managed,pool.policy(managed)[1])[0])==1
    assert mod_backup.verify_snapshot(managed/first['backup'])


def test_mixed_module_closure_is_refused_before_any_capture(private_pool,monkeypatch):
    managed,binding,_=private_pool
    monkeypatch.setitem(sys.modules,'mod_foreign_fixture',SimpleNamespace(__file__=str(managed/'foreign.py')))
    with pytest.raises(ValueError,match='closure differs'):force.force_capture(SOURCE/'tools/mods',binding,os.geteuid())
    assert not pool.entries(managed,pool.policy(managed)[1])[0]


def test_failed_independent_verification_keeps_both_snapshots_for_inspection(private_pool,monkeypatch):
    managed,binding,_=private_pool
    first=pool.run(managed,force=True)
    with monkeypatch.context() as patch:
        patch.setattr(mod_backup,'verify_snapshot',lambda _:(_ for _ in ()).throw(ValueError('injected verification failure')))
        with pytest.raises(ValueError,match='injected'):force.force_capture(SOURCE/'tools/mods',binding,os.geteuid())
    snapshots,_=pool.entries(managed,pool.policy(managed)[1])
    assert len(snapshots)==2 and (managed/first['backup']).is_dir()
    for snapshot in snapshots:assert mod_backup.verify_snapshot(snapshot['path'])
