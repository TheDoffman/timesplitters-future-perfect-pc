"""Relocated native image discovery, private cache, verified contained execution."""
import contextlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import queue
import threading
import time
import uuid

import pytest
from test_mod_match import image_pins
from test_script_process import process_tools

SUBDIR='ModRuntime/'+'a'*64


@pytest.fixture
def runtime(process_tools,tmp_path):
    worker=process_tools['worker'];app=tmp_path/'Player \u03a9 app';app.mkdir()
    driver=app/('native-resources'+worker.suffix)
    shutil.copy2(worker.with_name('tsfp-mod-resources-driver'+worker.suffix),driver)
    bundle=app/SUBDIR;bundle.mkdir(parents=True)
    shutil.copy2(worker,bundle/worker.name)
    guardian=worker.with_name('tsfp-mod-guardian')
    if guardian.is_file():shutil.copy2(guardian,bundle/guardian.name)
    parent=tmp_path/'Private \u03a9 cache';parent.mkdir()
    return driver,parent,image_pins(worker),bundle,worker.name


def command(runtime,mode='run',subdir=SUBDIR,pins=None,parent=None):
    driver,cache,expected,_,_=runtime
    return [str(driver),mode,str(parent or cache),subdir,*(pins or expected)]


def call(runtime,environment=None,**kwargs):
    p=subprocess.run(command(runtime,**kwargs),capture_output=True,timeout=12,cwd=runtime[1],env=environment)
    assert p.returncode==0 and not p.stderr,(p.returncode,p.stdout,p.stderr)
    return [json.loads(line) for line in p.stdout.splitlines()]


@contextlib.contextmanager
def held(runtime,mode='hold',parent=None):
    p=subprocess.Popen(command(runtime,mode,parent=parent),stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        first=json.loads(p.stdout.readline());assert first['accepted'],first
        root=Path(bytes.fromhex(first['root_hex']).decode())
        yield p,root
    finally:
        if p.poll() is None:p.communicate(b'\n',timeout=10)


def test_relocated_os_image_uses_bundle_beside_executable_and_cleans_cache(runtime):
    records=call(runtime)
    assert records[0]['accepted'] and records[1]['ran']
    assert not list(runtime[1].iterdir())
    assert (runtime[3]/runtime[4]).is_file()


def test_os_cache_parent_resolution_is_read_only(runtime):
    record=call(runtime,mode='parent')[0]
    parent=Path(bytes.fromhex(record['root_hex']).decode())
    assert record['accepted'] and parent.is_absolute() and parent.is_dir()
    assert not list(runtime[1].iterdir())


def test_cwd_and_environment_cannot_redirect_helper_discovery(runtime):
    driver,parent,pins,bundle,name=runtime
    decoy=parent/SUBDIR;decoy.mkdir(parents=True);(decoy/name).write_bytes(b'wrong image in cwd')
    environment=dict(os.environ,TSFP_MOD_WORKER=str(decoy/name),TSFP_MOD_RUNTIME_DIR=str(decoy),RECOMP_GAME_DIR=str(parent))
    records=call(runtime,environment=environment)
    assert records[0]['accepted'] and records[1]['ran']
    assert (decoy/name).read_bytes()==b'wrong image in cwd'
    assert list(parent.iterdir())==[parent/'ModRuntime']


@pytest.mark.parametrize('subdir',['','../ModRuntime/'+'a'*64,'/ModRuntime/'+'a'*64,'ModRuntime/'+'A'*64,'ModRuntime/'+'a'*63,'Other/'+'a'*64])
def test_only_generated_bundle_shape_is_accepted(runtime,subdir):
    assert not call(runtime,subdir=subdir)[0]['accepted']
    assert not list(runtime[1].iterdir())


@pytest.mark.parametrize('kind',['missing','changed','pin','file-link','parent-link'])
def test_missing_changed_or_aliased_helpers_never_start(runtime,kind):
    driver,parent,pins,bundle,name=runtime;worker=bundle/name
    if kind=='missing':worker.unlink()
    if kind=='changed':worker.write_bytes(b'corrupt packaged image')
    if kind=='pin':pins=['0'*64,pins[1]]
    if kind.endswith('link'):
        target=worker if kind=='file-link' else bundle
        renamed=target.with_name(target.name+'-original');target.rename(renamed)
        try:target.symlink_to(renamed,target_is_directory=kind=='parent-link')
        except OSError as error:pytest.skip('Host cannot create links: '+str(error))
    assert not call(runtime,pins=pins)[0]['accepted']
    assert not list(parent.iterdir())


def test_fresh_private_directory_preserves_unknown_content(runtime):
    with held(runtime,'private-hold') as (p,root):
        (root/'keep.txt').write_bytes(b'unknown user file, preserve')
        out,err=p.communicate(b'\n',timeout=10)
        assert p.returncode==0 and not err and json.loads(out)['rechecked']
    assert (root/'keep.txt').read_bytes()==b'unknown user file, preserve'


def test_two_cache_leases_are_fresh_and_independent(runtime):
    with held(runtime,'private-hold') as (p,first),held(runtime,'private-hold') as (q,second):
        assert first!=second and first.parent==second.parent==runtime[1]
    assert not list(runtime[1].iterdir())


def test_private_cache_has_bound_ownership_and_exclusive_lifetime_lease(runtime):
    with held(runtime,'private-hold') as (p,root):
        assert (root/'.tsfp-runtime-owner').read_text()=='TSFP mod runtime cache v2\n'+root.name+'\n'
        assert (root/'.runtime-lock').stat().st_size==0
        assert {path.name for path in root.iterdir()}=={'.tsfp-runtime-owner','.runtime-lock'}
        if os.name!='nt':
            import fcntl
            with (root/'.runtime-lock').open('r+b') as incoming:
                with pytest.raises(BlockingIOError):fcntl.flock(incoming,fcntl.LOCK_EX|fcntl.LOCK_NB)
        else:
            import ctypes
            kernel=ctypes.WinDLL('kernel32',use_last_error=True)
            kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
            kernel.CreateFileW.restype=ctypes.c_void_p
            handle=kernel.CreateFileW(str(root/'.runtime-lock'),0x80000000,7,None,3,0x00200000,None)
            assert handle==ctypes.c_void_p(-1).value and ctypes.get_last_error()==32
    assert not root.exists()


def test_changed_cache_marker_refuses_recheck_and_preserves_all_contents(runtime):
    with held(runtime,'private-hold') as (p,root):
        marker=root/'.tsfp-runtime-owner';marker.write_text('unknown ownership\n')
        out,err=p.communicate(b'\n',timeout=10)
        assert p.returncode==0 and not err and not json.loads(out)['rechecked']
    assert marker.read_text()=='unknown ownership\n' and (root/'.runtime-lock').exists()


def test_abrupt_parent_death_preserves_marked_cache_for_later_recovery(runtime):
    with held(runtime,'private-hold') as (p,root):
        p.kill();p.communicate(timeout=10)
    assert root.is_dir() and (root/'.tsfp-runtime-owner').read_text().endswith(root.name+'\n')
    if os.name!='nt':
        import fcntl
        with (root/'.runtime-lock').open('r+b') as incoming:fcntl.flock(incoming,fcntl.LOCK_EX|fcntl.LOCK_NB)
    # Recovery is not a blanket sweep: a new allocation preserves this old cache
    # until closed-layout/process-identity recovery admission is implemented.
    assert call(runtime,mode='private')[0]['accepted']
    assert root.exists() and list(runtime[1].iterdir())==[root]


@pytest.mark.parametrize('kind',['relative','missing','file','parent-link'])
def test_unsafe_cache_parents_are_refused(runtime,kind):
    parent=runtime[1]
    if kind=='relative':parent='relative'
    if kind=='missing':parent=parent/'absent'
    if kind=='file':parent=parent/'file';parent.write_bytes(b'preserve')
    if kind=='parent-link':
        alias=parent.with_name('alias')
        try:alias.symlink_to(parent,target_is_directory=True)
        except OSError as error:pytest.skip('Host cannot create links: '+str(error))
        parent=alias
    assert not call(runtime,mode='private',parent=parent)[0]['accepted']


@pytest.mark.skipif(os.name=='nt',reason='POSIX writer permissions')
def test_broad_posix_parent_and_replaced_owned_directory_are_refused(runtime):
    parent=runtime[1];parent.chmod(0o777)
    try:assert not call(runtime,mode='private')[0]['accepted']
    finally:parent.chmod(0o700)
    with held(runtime,'private-hold') as (p,root):
        moved=root.with_name(root.name+'-moved');root.rename(moved);root.mkdir()
        out,err=p.communicate(b'\n',timeout=10)
        assert p.returncode==0 and not err and not json.loads(out)['rechecked']
    assert root.is_dir() and moved.is_dir(),'Never remove a replacement or a renamed directory'


@pytest.mark.skipif(os.name!='nt',reason='Windows native ACLs')
def test_broad_windows_parent_is_refused_before_cache_creation(runtime):
    result=subprocess.run(['icacls',str(runtime[1]),'/grant','*S-1-1-0:(OI)(CI)M'],capture_output=True,timeout=10)
    assert result.returncode==0,(result.stdout,result.stderr)
    assert not call(runtime,mode='private')[0]['accepted'] and not list(runtime[1].iterdir())


def next_record(process):
    result=queue.Queue()
    threading.Thread(target=lambda:result.put(process.stdout.readline()),daemon=True).start()
    line=result.get(timeout=6)
    assert line,process.poll()
    return json.loads(line)


def journal_bytes(path):
    if os.name!='nt':return path.read_bytes()
    # The owned broker handle holds DELETE/write access. Observers explicitly
    # share those existing rights while requesting read-only access themselves.
    import ctypes
    api=ctypes.WinDLL('kernel32',use_last_error=True)
    api.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p]
    api.CreateFileW.restype=ctypes.c_void_p
    api.ReadFile.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint,ctypes.POINTER(ctypes.c_uint),ctypes.c_void_p]
    api.CloseHandle.argtypes=[ctypes.c_void_p]
    handle=api.CreateFileW(str(path),0x80000000,7,None,3,0x00200000,None)
    assert handle not in (None,ctypes.c_void_p(-1).value),ctypes.get_last_error()
    try:
        data=ctypes.create_string_buffer(4097);size=ctypes.c_uint()
        assert api.ReadFile(handle,data,4097,ctypes.byref(size),None) and size.value<=4096
        return data.raw[:size.value]
    finally:api.CloseHandle(handle)


def directory_identity(root):
    if os.name!='nt':
        stat=root.stat();return stat.st_dev,stat.st_ino
    import ctypes
    api=ctypes.WinDLL('kernel32',use_last_error=True)
    api.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p]
    api.CreateFileW.restype=ctypes.c_void_p
    api.GetFileInformationByHandle.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
    api.CloseHandle.argtypes=[ctypes.c_void_p]
    handle=api.CreateFileW(str(root),0x80,7,None,3,0x02200000,None)
    assert handle not in (None,ctypes.c_void_p(-1).value),ctypes.get_last_error()
    try:
        # BY_HANDLE_FILE_INFORMATION: attributes, three FILETIMEs, volume,
        # size high/low, link count, and file index high/low (13 DWORDs).
        info=(ctypes.c_uint32*13)();assert api.GetFileInformationByHandle(handle,ctypes.byref(info))
        return info[7],(info[11]<<32)|info[12]
    finally:api.CloseHandle(handle)


def journal_records(root,pins):
    intents=list(root.glob('.spawn-*.intent'));commits=list(root.glob('.spawn-*.commit'))
    assert len(intents)==len(commits)==1
    intent=journal_bytes(intents[0]).decode('ascii').splitlines();commit=journal_bytes(commits[0]).decode('ascii').splitlines()
    assert intent[:3]==['TSFP mod spawn intent v1',root.name,intents[0].name.removesuffix('.intent')]
    assert tuple(map(int,intent[3].split()))==directory_identity(root)
    assert intent[5:]==[pins[0],'' if pins[1]=='-' else pins[1]]
    assert len(Path(intent[4]).parts)==2 and Path(intent[4]).parts[0].startswith('images-')
    assert (root/intent[4]).is_file()
    assert commit[:3]==['TSFP mod spawn commit v1',root.name,intents[0].name]
    count=int(commit[3]);assert count==(2 if os.sys.platform=='darwin' else 1)
    stamps=[tuple(map(int,line.split())) for line in commit[4:4+count]]
    assert all(len(s)==4 and s[0]>0 and s[1]>0 and s[2]>0 for s in stamps)
    if os.name=='nt':
        leaf,profile,sid,identity=commit[4+count:]
        assert str(uuid.UUID(leaf)).upper()==leaf.upper() and profile=='TSFP.Mod.'+leaf
        assert sid.startswith('S-1-15-2-') and len(identity.split())==2 and int(identity.split()[1])>0
    else:assert len(commit)==4+count
    return intents[0],commits[0],stamps,commit


def stamp_state(tools,stamp):
    driver=tools['worker'].with_name('tsfp-mod-process-identity-driver'+tools['worker'].suffix)
    result=subprocess.run([str(driver),'state',*map(str,stamp)],capture_output=True,timeout=5)
    assert result.returncode==0 and not result.stderr
    return json.loads(result.stdout)['state']


def test_owned_cache_cannot_start_without_its_journal(runtime):
    records=call(runtime,mode='no-journal')
    assert records[1]['refused'] and not list(runtime[1].iterdir())


def test_intent_precedes_native_start_and_commit_contains_owned_births(runtime,process_tools):
    with held(runtime,'intent-hold') as (p,root):
        assert next_record(p)['intent']
        assert len(list(root.glob('.spawn-*.intent')))==1 and not list(root.glob('.spawn-*.commit'))
        p.stdin.write(b's');p.stdin.flush();assert next_record(p)['committed']
        _,_,stamps,_=journal_records(root,runtime[2])
        assert all(stamp_state(process_tools,s)=='alive' for s in stamps)
        out,err=p.communicate(b'x',timeout=10);assert p.returncode==0 and not err and not out
    assert not root.exists() and not list(runtime[1].iterdir())
    assert all(stamp_state(process_tools,s)=='exited' for s in stamps)


def test_actual_session_keeps_committed_journal_until_worker_cleanup(runtime):
    with held(runtime,'journal-hold') as (p,root):
        assert next_record(p)['ready'];journal_records(root,runtime[2])
        out,err=p.communicate(b'x',timeout=10)
        assert p.returncode==0 and not err and json.loads(out)['ran']
    assert not root.exists()


def test_journal_finish_preserves_live_creation_evidence(runtime,process_tools):
    with held(runtime,'intent-hold') as (p,root):
        assert next_record(p)['intent'];p.stdin.write(b's');p.stdin.flush();assert next_record(p)['committed']
        intent,commit,stamps,_=journal_records(root,runtime[2]);before=(journal_bytes(intent),journal_bytes(commit))
        out,err=p.communicate(b'l',timeout=10);assert p.returncode==0 and not err and not out
    assert (intent.read_bytes(),commit.read_bytes())==before
    assert all(stamp_state(process_tools,s)=='exited' for s in stamps)


@pytest.mark.skipif(os.name=='nt',reason='Windows denies replacement/writes while the broker holds journal handles')
@pytest.mark.parametrize('change',['contents','replacement','link'])
def test_changed_intent_refuses_process_creation_and_preserves_evidence(runtime,change):
    with held(runtime,'intent-hold') as (p,root):
        assert next_record(p)['intent'];intent=next(root.glob('.spawn-*.intent'))
        if change=='contents':intent.write_bytes(b'foreign intent')
        else:
            original=intent.with_suffix('.original');intent.rename(original)
            if change=='replacement':intent.write_bytes(original.read_bytes())
            else:intent.symlink_to(original)
        out,err=p.communicate(b's',timeout=10)
        assert p.returncode==0 and not err and json.loads(out)['refused']
    assert intent.exists() and not list(root.glob('.spawn-*.commit'))


@pytest.mark.skipif(os.name!='nt',reason='Windows journal sharing control')
def test_windows_live_journal_denies_writes_and_replacement(runtime):
    with held(runtime,'intent-hold') as (p,root):
        assert next_record(p)['intent'];intent=next(root.glob('.spawn-*.intent'))
        with pytest.raises(PermissionError):intent.write_bytes(b'foreign intent')
        with pytest.raises(PermissionError):intent.rename(intent.with_suffix('.foreign'))
        p.stdin.write(b's');p.stdin.flush();assert next_record(p)['committed']
        out,err=p.communicate(b'x',timeout=10);assert p.returncode==0 and not err and not out
    assert not root.exists()


@pytest.mark.skipif(os.name=='nt',reason='Windows prevents live journal mutation with sharing modes')
@pytest.mark.parametrize('change',['contents','replacement','hardlink'])
def test_changed_committed_record_preserves_both_records_at_shutdown(runtime,change):
    with held(runtime,'journal-hold') as (p,root):
        assert next_record(p)['ready'];intent,commit,_,_=journal_records(root,runtime[2])
        before=intent.read_bytes()
        if change=='contents':commit.write_bytes(b'foreign committed record')
        elif change=='replacement':
            original=commit.with_suffix('.original');commit.rename(original);commit.write_bytes(original.read_bytes())
        else:os.link(commit,commit.with_suffix('.linked'))
        changed=commit.read_bytes();out,err=p.communicate(b'x',timeout=10)
        assert p.returncode==0 and not err and json.loads(out)['ran']
    assert intent.read_bytes()==before and commit.read_bytes()==changed


def test_foreign_commit_collision_is_preserved_and_no_mod_code_runs(runtime):
    with held(runtime,'intent-hold') as (p,root):
        assert next_record(p)['intent'];intent=next(root.glob('.spawn-*.intent'));commit=intent.with_suffix('.commit')
        commit.write_bytes(b'foreign commit')
        out,err=p.communicate(b's',timeout=10)
        assert p.returncode==3 and not err and not json.loads(out)['committed']
    assert intent.is_file() and commit.read_bytes()==b'foreign commit'


def test_abrupt_broker_death_retains_committed_records_and_child_exits(runtime,process_tools):
    with held(runtime,'journal-hold') as (p,root):
        assert next_record(p)['ready'];intent,commit,stamps,fields=journal_records(root,runtime[2])
        before=(journal_bytes(intent),journal_bytes(commit));p.kill();p.communicate(timeout=8)
    deadline=time.monotonic()+5
    while any(stamp_state(process_tools,s)!='exited' for s in stamps):
        assert time.monotonic()<deadline,'Owned child survived abrupt broker exit'
        time.sleep(.02)
    assert (intent.read_bytes(),commit.read_bytes())==before
    if os.name=='nt':
        # Remove only this fixture's newly-created GUID profile after all kernel
        # creation identities are proven dead; leave cache/journal evidence.
        import ctypes
        leaf,profile,_,_=fields[4+int(fields[3]):]
        assert profile=='TSFP.Mod.'+str(uuid.UUID(leaf)).upper()
        api=ctypes.WinDLL('userenv');api.DeleteAppContainerProfile.argtypes=[ctypes.c_wchar_p]
        api.DeleteAppContainerProfile.restype=ctypes.c_long
        assert api.DeleteAppContainerProfile(profile)==0


def delete_fixture_profile(fields):
    if os.name!='nt':return
    import ctypes
    leaf,profile,_,_=fields[4+int(fields[3]):]
    assert profile=='TSFP.Mod.'+str(uuid.UUID(leaf)).upper()
    api=ctypes.WinDLL('userenv');api.DeleteAppContainerProfile.argtypes=[ctypes.c_wchar_p]
    api.DeleteAppContainerProfile.restype=ctypes.c_long
    assert api.DeleteAppContainerProfile(profile)==0


def fixture_profile_scope(fields):
    import ctypes
    leaf,profile,sid,identity=fields[4+int(fields[3]):]
    assert profile=='TSFP.Mod.'+str(uuid.UUID(leaf)).upper()
    api=ctypes.WinDLL('userenv');api.GetAppContainerFolderPath.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_void_p)]
    api.GetAppContainerFolderPath.restype=ctypes.c_long;pointer=ctypes.c_void_p()
    assert api.GetAppContainerFolderPath(sid,ctypes.byref(pointer))==0 and pointer.value
    try:path=Path(ctypes.wstring_at(pointer))
    finally:
        ole=ctypes.WinDLL('ole32');ole.CoTaskMemFree.argtypes=[ctypes.c_void_p];ole.CoTaskMemFree(pointer)
    if path.name.lower()=='ac' and path.parent.name.casefold()==profile.casefold():path=path.parent
    assert directory_identity(path)==tuple(map(int,identity.split()))
    return path


@contextlib.contextmanager
def crashed_runtime(runtime,tools):
    with held(runtime,'journal-hold') as (p,root):
        assert next_record(p)['ready'];_,_,stamps,fields=journal_records(root,runtime[2]);p.kill();p.communicate(timeout=8)
    deadline=time.monotonic()+5
    while any(stamp_state(tools,s)!='exited' for s in stamps):
        assert time.monotonic()<deadline,'Fixture children must be dead before recovery'
        time.sleep(.02)
    try:yield root,fields
    finally:delete_fixture_profile(fields)


def cache_snapshot(root):
    result={}
    for path in root.rglob('*'):
        key=path.relative_to(root).as_posix()
        if path.is_symlink():result[key]=('link',os.readlink(path))
        elif path.is_file():result[key]=('file',path.read_bytes())
        else:result[key]=('directory',None)
    return result


@pytest.mark.parametrize('mode',['recover','run'])
def test_verified_dead_cache_is_recovered_explicitly_and_before_fresh_session(runtime,process_tools,mode):
    with crashed_runtime(runtime,process_tools) as (root,_):
        records=call(runtime,mode=mode)
        if mode=='recover':assert records==[dict(examined=1,removed=1,preserved=0)]
        else:assert records[0]['accepted'] and records[1]['ran']
        assert not root.exists() and not list(runtime[1].iterdir())


def test_recovery_preserves_an_actual_live_leased_session(runtime):
    with held(runtime,'journal-hold') as (p,root):
        assert next_record(p)['ready'];before=[x.name for x in root.iterdir()]
        assert call(runtime,mode='recover')==[dict(examined=1,removed=0,preserved=1)]
        assert p.poll() is None and sorted(x.name for x in root.iterdir())==sorted(before)
        out,err=p.communicate(b'x',timeout=10);assert p.returncode==0 and not err and json.loads(out)['ran']
    assert not root.exists()


@pytest.mark.parametrize('change',['unknown','nested-unknown','hash','hardlink','helper-hardlink','helper-link','missing-commit','bad-count','root-id','alive','bad-kind','pin','old-marker','oversize','profile-id'])
def test_recovery_refuses_uncertain_roots_without_mutating_any_cache_file(runtime,process_tools,change):
    if change=='profile-id' and os.name!='nt':pytest.skip('Windows AppContainer profile identity')
    with crashed_runtime(runtime,process_tools) as (root,original_fields):
        intent=next(root.glob('.spawn-*.intent'));commit=next(root.glob('.spawn-*.commit'));pins=None
        if os.name=='nt':profile_scope=fixture_profile_scope(original_fields);profile_identity=directory_identity(profile_scope)
        if change=='unknown':(root/'foreign.txt').write_bytes(b'preserve this unknown content')
        elif change=='nested-unknown':next(root.glob('images-*')).joinpath('foreign.txt').write_bytes(b'unknown nested file')
        elif change in ('helper-hardlink','helper-link'):
            worker=next(root.glob('images-*/tsfp-mod-worker*'));outside=runtime[1]/'outside-helper'
            if change=='helper-hardlink':os.link(worker,outside)
            else:
                worker.rename(outside)
                try:worker.symlink_to(outside)
                except OSError as error:pytest.skip('Host cannot create links: '+str(error))
        elif change=='hash':
            worker=next(root.glob('images-*/tsfp-mod-worker*'));worker.chmod(0o600);worker.write_bytes(b'foreign executable bytes')
        elif change=='hardlink':os.link(commit,runtime[1]/'outside-hardlink')
        elif change=='missing-commit':commit.unlink()
        elif change=='root-id':
            lines=intent.read_text().splitlines();device,inode=map(int,lines[3].split());lines[3]=f'{device} {inode+1}';intent.write_bytes(('\n'.join(lines)+'\n').encode('ascii'))
        elif change=='pin':pins=['0'*64,runtime[2][1]]
        elif change=='old-marker':(root/'.tsfp-runtime-owner').write_bytes(('TSFP mod runtime cache v1\n'+root.name+'\n').encode('ascii'))
        elif change=='oversize':commit.write_bytes(b'x'*4097)
        else:
            lines=commit.read_text().splitlines()
            if change=='bad-count':lines[3]='3'
            elif change=='profile-id':
                volume,inode=map(int,lines[-1].split());lines[-1]=f'{volume} {inode+1}'
            elif change=='bad-kind':lines[4]='0 '+lines[4].split(' ',1)[1]
            else:
                driver=process_tools['worker'].with_name('tsfp-mod-process-identity-driver'+process_tools['worker'].suffix)
                result=subprocess.run([str(driver),'read',str(os.getpid())],capture_output=True,timeout=5)
                assert result.returncode==0 and not result.stderr;record=json.loads(result.stdout);assert record['state']=='alive'
                lines[4]=' '.join(str(record[k]) for k in ('kind','pid','start_a','start_b'))
            commit.write_bytes(('\n'.join(lines)+'\n').encode('ascii'))
        before=cache_snapshot(root)
        assert call(runtime,mode='recover',pins=pins)==[dict(examined=1,removed=0,preserved=1)]
        assert cache_snapshot(root)==before
        if os.name=='nt':assert directory_identity(profile_scope)==profile_identity
