"""Relocated native image discovery, private cache, verified contained execution."""
import contextlib
import json
import os
from pathlib import Path
import shutil
import subprocess

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
        assert (root/'.tsfp-runtime-owner').read_text()=='TSFP mod runtime cache v1\n'+root.name+'\n'
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
