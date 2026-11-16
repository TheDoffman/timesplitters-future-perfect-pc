"""Real private captures, due-time/retention and interrupted managed rotation."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest

import mod_backup
import mod_backup_pool as pool
from mod_package import ModError
from test_host_backup import host

pytestmark=pytest.mark.skipif(os.name!='posix',reason='Actual private POSIX host backup operations')
ROOT=Path(__file__).resolve().parents[2]


def files(folder):
    return {str(p.relative_to(folder)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest())
            for p in folder.rglob('*') if p.is_file()}


def clocked(monkeypatch,now):
    monkeypatch.setattr(mod_backup.time,'time',lambda:now)
    return lambda:now


def test_readonly_verification_preserves_snapshot_and_live_authentication(host,tmp_path):
    repository,accounts,_,token,_,_,_=host
    target=tmp_path/'manual';captured=mod_backup.snapshot(repository.root,target)
    before=files(target)
    report=mod_backup.verify_snapshot(target)
    assert report['files']==captured['files'] and report['bytes']==captured['bytes']
    assert files(target)==before and not list(target.glob('catalog.sqlite-*'))
    assert accounts.authenticate(token)
    result=subprocess.run([sys.executable,str(ROOT/'tools/mods/mod_backup.py'),'verify',str(target)],capture_output=True,text=True,timeout=20)
    assert result.returncode==0,result.stderr
    assert json.loads(result.stdout)['manifest_sha256']==report['manifest_sha256']


def test_operator_cli_creates_pool_captures_once_and_reports_next_due(host,tmp_path):
    repository,*_=host;folder=tmp_path/'managed'
    tool=[sys.executable,str(ROOT/'tools/mods/mod_backup_pool.py')]
    for arguments,status in [(['init',str(repository.root),str(folder)],None),
                             (['run',str(folder)],'captured'),(['run',str(folder)],'not-due')]:
        result=subprocess.run(tool+arguments,capture_output=True,text=True,timeout=20)
        assert result.returncode==0,result.stderr
        report=json.loads(result.stdout)
        if status:assert report['status']==status and report['next_due']>0
    assert len(list(folder.glob('snapshot-*')))==1


def test_due_ticks_capture_and_rotate_only_owned_backups_after_verification(host,tmp_path,monkeypatch):
    repository,_,_,_,_,_,_=host
    manual=tmp_path/'manual';mod_backup.snapshot(repository.root,manual);before=files(manual)
    folder=tmp_path/'managed';pool.initialize(repository.root,folder,keep=2,interval=3600)
    now=int(time.time());clock=clocked(monkeypatch,now)
    first=pool.run(folder,clock=clock)
    assert first['status']=='captured' and first['snapshots']==1 and first['removed']==[]
    skipped=pool.run(folder,clock=clock)
    assert skipped['status']=='not-due' and skipped['next_due']==now+3600
    second=pool.run(folder,clock=clocked(monkeypatch,now+3600))
    third=pool.run(folder,clock=clocked(monkeypatch,now+7200))
    assert third['snapshots']==2 and third['removed']==[first['backup']]
    assert not (folder/first['backup']).exists()
    for name in (second['backup'],third['backup']):mod_backup.verify_snapshot(folder/name)
    assert files(manual)==before


def test_same_second_forced_capture_preserves_the_newest_sequence(host,tmp_path,monkeypatch):
    repository,*_=host;folder=tmp_path/'managed';pool.initialize(repository.root,folder,keep=1)
    clock=clocked(monkeypatch,int(time.time()))
    first=pool.run(folder,clock=clock);second=pool.run(folder,force=True,clock=clock)
    assert second['backup'].endswith('-00000002') and second['removed']==[first['backup']]
    assert (folder/second['backup']).is_dir()


def test_full_pool_preserves_last_good_backup_instead_of_predeleting(host,tmp_path,monkeypatch):
    repository,*_=host
    probe=tmp_path/'probe';report=mod_backup.snapshot(repository.root,probe)
    total=report['bytes']+(probe/mod_backup.MANIFEST).stat().st_size
    folder=tmp_path/'managed'
    pool.initialize(repository.root,folder,keep=1,maximum_bytes=pool.MANIFEST_RESERVE+total+1)
    now=int(time.time());first=pool.run(folder,clock=clocked(monkeypatch,now))
    before=files(folder/first['backup'])
    with pytest.raises(ModError,match='headroom'):
        pool.run(folder,force=True,clock=clocked(monkeypatch,now+3600))
    assert files(folder/first['backup'])==before
    assert len(list(folder.glob('snapshot-*')))==1


def test_corrupt_new_capture_never_removes_a_previous_good_snapshot(host,tmp_path,monkeypatch):
    repository,*_=host;folder=tmp_path/'managed';pool.initialize(repository.root,folder,keep=1)
    now=int(time.time());first=pool.run(folder,clock=clocked(monkeypatch,now));before=files(folder/first['backup'])
    original=pool.snapshot
    def corrupt(*args,**kwargs):
        result=original(*args,**kwargs)
        database=Path(args[1])/'catalog.sqlite'
        with database.open('r+b') as stream:stream.write(b'corrupt')
        return result
    monkeypatch.setattr(pool,'snapshot',corrupt)
    with pytest.raises(ModError,match='integrity'):
        pool.run(folder,force=True,clock=clocked(monkeypatch,now+3600))
    assert files(folder/first['backup'])==before and len(list(folder.glob('snapshot-*')))==2


def test_complete_snapshot_recovers_interrupted_rotation_without_another_capture(host,tmp_path,monkeypatch):
    repository,*_=host;folder=tmp_path/'managed';pool.initialize(repository.root,folder,keep=1)
    now=int(time.time());first=pool.run(folder,clock=clocked(monkeypatch,now))
    original=pool.retain
    def interrupted(*args,**kwargs):raise OSError('Simulated rotation interruption')
    monkeypatch.setattr(pool,'retain',interrupted)
    with pytest.raises(OSError,match='interruption'):
        pool.run(folder,force=True,clock=clocked(monkeypatch,now+3600))
    assert len(list(folder.glob('snapshot-*')))==2
    monkeypatch.setattr(pool,'retain',original)
    def forbidden(*args,**kwargs):raise AssertionError('Must finish existing capture, not make another')
    monkeypatch.setattr(pool,'snapshot',forbidden)
    result=pool.run(folder,clock=clocked(monkeypatch,now+3601))
    assert result['status']=='not-due' and result['removed']==[first['backup']] and result['snapshots']==1


def test_partial_capture_is_preserved_and_prevents_further_output(host,tmp_path,monkeypatch):
    repository,*_=host;folder=tmp_path/'managed';pool.initialize(repository.root,folder,keep=1)
    now=int(time.time());first=pool.run(folder,clock=clocked(monkeypatch,now));before=files(folder/first['backup'])
    original=pool.snapshot
    def interrupted(*args,**kwargs):
        result=original(*args,**kwargs);(Path(args[1])/mod_backup.MANIFEST).unlink()
        raise OSError('Simulated capture interruption')
    monkeypatch.setattr(pool,'snapshot',interrupted)
    with pytest.raises(OSError):pool.run(folder,force=True,clock=clocked(monkeypatch,now+3600))
    partials=list(folder.glob('snapshot-*'));assert len(partials)==2
    with pytest.raises(OSError):pool.run(folder,force=True,clock=clocked(monkeypatch,now+7200))
    assert list(folder.glob('snapshot-*'))==partials and files(folder/first['backup'])==before


def test_pool_lock_unknown_entries_and_clock_rollback_fail_without_capture(host,tmp_path,monkeypatch):
    repository,*_=host;folder=tmp_path/'managed';pool.initialize(repository.root,folder)
    now=int(time.time());clock=clocked(monkeypatch,now)
    with pool.locked(folder):
        with pytest.raises(ModError,match='busy'):pool.run(folder,clock=clock)
    first=pool.run(folder,clock=clock)
    with pytest.raises(ModError,match='backwards'):pool.run(folder,force=True,clock=clocked(monkeypatch,now-1))
    unknown=folder/'operator-note';unknown.write_bytes(b'preserve')
    with pytest.raises(ModError,match='unexpected'):pool.run(folder,force=True,clock=clock)
    assert unknown.read_bytes()==b'preserve' and (folder/first['backup']).is_dir()


@pytest.mark.parametrize('failure',['payload','extra','link','hardlink'])
def test_verification_refuses_changed_or_aliased_snapshot_without_altering_it(host,tmp_path,failure):
    repository,*_=host;target=tmp_path/'manual';mod_backup.snapshot(repository.root,target)
    member=next((target/'blobs').iterdir())
    if failure=='payload':member.write_bytes(b'changed')
    if failure=='extra':(target/'unknown').write_bytes(b'preserve')
    if failure in ('link','hardlink'):
        outside=tmp_path/'outside';outside.write_bytes(member.read_bytes());member.unlink()
        if failure=='link':member.symlink_to(outside)
        else:os.link(outside,member)
    before=files(target)
    with pytest.raises((ModError,OSError)):mod_backup.verify_snapshot(target)
    assert files(target)==before


def test_capture_budget_is_exact_and_initialization_never_adopts_existing_data(host,tmp_path):
    repository,*_=host
    with pytest.raises(ModError,match='headroom'):mod_backup.snapshot(repository.root,tmp_path/'tiny',maximum_total=1)
    assert not (tmp_path/'tiny').exists()
    with pytest.raises(ModError,match='Invalid'):mod_backup.snapshot(repository.root,tmp_path/'boolean',maximum_total=True)
    existing=tmp_path/'existing';existing.mkdir(mode=0o700);(existing/'sentinel').write_bytes(b'private')
    with pytest.raises(ModError,match='Preserving'):pool.initialize(repository.root,existing)
    assert (existing/'sentinel').read_bytes()==b'private'
