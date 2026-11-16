"""Host provisioning is private, bounded, pinned and never adopts unknown state."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest
from cryptography.hazmat.primitives import serialization

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/mods'))
import mod_system_host as host
from mod_package import ModError
from mod_trust import make_root

SOURCE = ROOT/'tools/mods/hosting/initialize-private-host-mac.py'
spec = importlib.util.spec_from_file_location('private_host_initializer',SOURCE)
init = importlib.util.module_from_spec(spec);spec.loader.exec_module(init)


def test_default_plan_never_accesses_private_files_or_starts_services():
    result=subprocess.run([sys.executable,'-B',str(SOURCE)],check=True,capture_output=True,timeout=10)
    value=json.loads(result.stdout)
    assert value['phase']=='plan-only' and value['account']=='_tsfpworkshop'
    assert value['services_started'] is value['network_started'] is False


@pytest.fixture
def inputs(tmp_path,monkeypatch,trust_keys):
    if os.name!='posix':pytest.skip('Actual POSIX owner/mode/FD and linked-file checks')
    monkeypatch.setattr(init,'ROOT',tmp_path);monkeypatch.setattr(host,'ROOT',tmp_path)
    tmp_path.chmod(0o700)
    (tmp_path/'data').mkdir(mode=0o700)
    incoming=tmp_path/'data/host-input';incoming.mkdir(mode=0o700)
    origin='https://workshop.example.test:8443'
    pinned=make_root(trust_keys[:2],[trust_keys[4]],1,origin,int(time.time())-60)
    values={'origin.json':init.wire(dict(schema=1,origin=origin,trusted_root_sha256=hashlib.sha256(pinned).hexdigest())),
            'trusted-root.json':pinned,
            'catalog-1.pem':trust_keys[4].private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption())}
    for name,value in values.items():
        path=incoming/name;path.write_bytes(value);path.chmod(0o600)
    return tmp_path/'releases'/('a'*64),tmp_path/'data/host',incoming,values


def test_host_initialization_is_exact_and_idempotent_without_input_reuse(inputs):
    release,root,incoming,values=inputs
    result=init.initialize(release)
    assert result['phase']=='ready' and result['trusted_root_sha256']==hashlib.sha256(values['trusted-root.json']).hexdigest()
    assert result['services_started'] is result['network_started'] is False
    assert set(p.name for p in root.iterdir())=={*init.PAYLOADS,*init.DIRECTORIES,'initialization.json'}
    before={p.name:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in root.iterdir() if p.is_file()}
    assert all(p.stat().st_mode&0o777==(0o700 if p.is_dir() else 0o600) for p in root.iterdir())
    sentinel=root/'api-state/preserve-live-state';sentinel.write_bytes(b'owned fixture state')
    incoming.rename(incoming.with_name('host-input-retired'))
    assert init.initialize(release)==result
    assert before=={p.name:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in root.iterdir() if p.is_file()}
    assert sentinel.read_bytes()==b'owned fixture state'
    configs={name:json.loads((root/name).read_bytes()) for name in init.CONFIGS}
    assert configs['api.json']['repository']==configs['publication.json']['repository']==str(release.parents[1]/'data/scan/repository')
    assert configs['api.json']['origin']==configs['gateway.json']['origin']==result['origin']
    assert (root/'catalog-1.pem').read_bytes()==values['catalog-1.pem']


@pytest.mark.parametrize('damage',['unknown','pin','origin','foreign-key','root-key','encrypted-key','key-mode','key-link','key-fifo','oversized-root','duplicate'])
def test_invalid_inputs_are_preserved_before_host_directory_creation(inputs,damage,trust_keys):
    release,root,incoming,values=inputs
    if damage=='unknown':(incoming/'root-private.pem').write_bytes(b'preserve unknown file')
    if damage in ('pin','origin'):
        value=json.loads(values['origin.json']);value['trusted_root_sha256' if damage=='pin' else 'origin']='0'*64 if damage=='pin' else 'https://other.example.test'
        (incoming/'origin.json').write_bytes(init.wire(value))
    if damage in ('foreign-key','root-key','encrypted-key'):
        key=trust_keys[3] if damage=='foreign-key' else trust_keys[0] if damage=='root-key' else trust_keys[4]
        encryption=serialization.BestAvailableEncryption(b'disposable fixture passphrase') if damage=='encrypted-key' else serialization.NoEncryption()
        (incoming/'catalog-1.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,encryption))
    if damage=='key-mode':(incoming/'catalog-1.pem').chmod(0o644)
    if damage=='key-link':os.link(incoming/'catalog-1.pem',incoming.parent/'linked-key')
    if damage=='key-fifo':(incoming/'catalog-1.pem').unlink();os.mkfifo(incoming/'catalog-1.pem',0o600)
    if damage=='oversized-root':(incoming/'trusted-root.json').write_bytes(b'x'*65537)
    if damage=='duplicate':(incoming/'origin.json').write_bytes(b'{"schema":1,"schema":1}')
    before={p.name:p.lstat().st_ino for p in incoming.iterdir()}
    with pytest.raises((OSError,ModError)):init.initialize(release)
    assert not root.exists()
    assert before=={p.name:p.lstat().st_ino for p in incoming.iterdir()}


@pytest.mark.parametrize('damage',['unknown','partial','changed','generation','hardlink','symlink','writable','fifo','updates','state-link'])
def test_existing_foreign_or_changed_host_state_is_never_overwritten(inputs,damage):
    release,root,incoming,_=inputs
    init.initialize(release)
    target=root/'api.json'
    if damage=='unknown':(root/'personal-file').write_bytes(b'preserve')
    if damage=='partial':
        value=json.loads((root/'initialization.json').read_bytes());value['phase']='preparing';(root/'initialization.json').write_bytes(init.wire(value))
    if damage=='changed':target.write_bytes(b'personal configuration')
    if damage=='generation':release=release.with_name('b'*64)
    if damage=='hardlink':os.link(target,root.parent/'linked-config')
    if damage=='symlink':target.rename(root.parent/'old-api');target.symlink_to(root.parent/'old-api')
    if damage=='writable':target.chmod(0o644)
    if damage=='fifo':target.unlink();os.mkfifo(target,0o600)
    if damage=='updates':(root/'root-updates/new-root').write_bytes(b'preserve')
    if damage=='state-link':(root/'api-state').rename(root.parent/'old-state');(root/'api-state').symlink_to(root.parent/'old-state')
    before={p.name:p.lstat().st_ino for p in root.iterdir()}
    with pytest.raises((OSError,ModError)):init.initialize(release)
    assert before=={p.name:p.lstat().st_ino for p in root.iterdir()}
    if damage=='changed':assert target.read_bytes()==b'personal configuration'


def test_interrupted_host_initialization_is_withheld_and_not_retried(inputs,monkeypatch):
    release,root,incoming,_=inputs
    original=init.new_member
    def interrupted(fd,name,value):
        if name=='catalog-1.pem':raise OSError('injected disk failure')
        return original(fd,name,value)
    monkeypatch.setattr(init,'new_member',interrupted)
    with pytest.raises(OSError):init.initialize(release)
    assert json.loads((root/'initialization.json').read_bytes())['phase']=='preparing'
    assert not (root/'catalog-1.pem').exists()
    with pytest.raises(ModError):init.initialize(release)


def test_worker_requires_completed_host_initialization_before_config(monkeypatch):
    spec=importlib.util.spec_from_file_location('host_worker_gate',SOURCE.with_name('system-host-service-mac.py'))
    worker=importlib.util.module_from_spec(spec);spec.loader.exec_module(worker)
    from types import SimpleNamespace
    release=Path.cwd()/'fixture-host/releases'/('a'*64)
    calls=[]
    def helper(name):
        if name=='initialize-private-scan-mac.py':return SimpleNamespace(installed_release=lambda:release,check=lambda r:calls.append('scan'))
        assert name=='initialize-private-host-mac.py'
        return SimpleNamespace(check=lambda r:(_ for _ in ()).throw(ModError('host incomplete')))
    monkeypatch.setattr(worker.sys,'platform','darwin')
    monkeypatch.setattr(worker,'require_locked_process',lambda:calls.append('identity'))
    monkeypatch.setattr(worker,'helper',helper)
    # Only the installed path comparison is substituted; no files are opened.
    monkeypatch.setattr(worker,'__file__',str(release/'source/tools/mods/hosting/system-host-service-mac.py'))
    with pytest.raises(ModError,match='host incomplete'):worker.installed_release()
    assert calls==['identity','scan']
