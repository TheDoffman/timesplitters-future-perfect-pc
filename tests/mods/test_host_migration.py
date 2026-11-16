"""Private configuration transaction/recovery; no real launchd or production data."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

import pytest
from cryptography.hazmat.primitives import serialization

SOURCE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SOURCE/'tools/mods'))
import mod_host_migration as migration
import mod_system_host as system
from mod_package import ModError
from mod_trust import make_root


@pytest.fixture
def configuration(tmp_path, monkeypatch, trust_keys):
    monkeypatch.setattr(migration, 'ROOT', tmp_path)
    monkeypatch.setattr(system, 'ROOT', tmp_path)
    host, scan = migration.helpers()
    monkeypatch.setattr(host, 'ROOT', tmp_path);monkeypatch.setattr(scan, 'ROOT', tmp_path)
    monkeypatch.setattr(migration, 'helpers', lambda: (host, scan))
    origin = 'https://workshop.example.test:8443'
    root = make_root(trust_keys[:2], [trust_keys[4]], 1, origin, int(time.time())-60)
    key = trust_keys[4].private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    before, after = (tmp_path/'releases'/('a'*64), tmp_path/'releases'/('b'*64))
    return before, after, root, key, origin, host, scan


def test_plan_matches_initializer_contracts_and_excludes_signing_material(configuration):
    before, after, root, key, origin, host, scan = configuration
    plan = migration.expected(before, after, root, key, origin)
    assert set(plan['before']) == set(plan['after']) == set(migration.FIELDS)
    assert len(plan['before']) == 6 and plan['sha256'] == hashlib.sha256(migration.wire(plan['manifest'])).hexdigest()
    assert key not in b''.join(plan['before'].values())+b''.join(plan['after'].values())
    for generation, side in ((before,'before'), (after,'after')):
        configs, marker = scan.plan(generation)
        assert plan[side]['scan/service.json'] == scan.wire(configs['service.json'])
        assert plan[side]['scan/initialization.json'] == scan.wire(dict(marker, phase='complete'))
        assert plan[side]['host/gateway.json'] == host.wire(host.configs(generation, origin)['gateway.json'])
    assert migration.expected(str(before), str(after), root, key, origin) == plan
    for old, new in ((before,before), (before,after.with_name('B'*64)), (before,after.parent.parent/'b')):
        with pytest.raises(ModError):migration.expected(old,new,root,key,origin)
    with pytest.raises(ModError):migration.expected(before,after,root,key,'https://different.example.test')


def test_quiescence_labels_match_actual_job_factories():
    spec = importlib.util.spec_from_file_location('migration_job_contract', SOURCE/'tools/mods/hosting/register-installed-host-mac.py')
    registrar = importlib.util.module_from_spec(spec);spec.loader.exec_module(registrar)
    generation = migration.ROOT/'releases'/('b'*64)
    python = generation/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12'
    labels = [value['Label'] for _, value in registrar.jobs(generation,dict(uid=600,gid=600),python)]
    assert migration.jobs() == sorted([*labels, 'org.timesplitters.workshop.backup'])


def test_cli_defaults_to_no_data_or_service_mutation():
    result = subprocess.run([sys.executable,'-B',str(SOURCE/'tools/mods/hosting/migrate-private-generation-mac.py')],
                            check=True,capture_output=True,timeout=10)
    assert json.loads(result.stdout) == dict(schema=1,phase='plan-only',files=6,services_changed=False,data_written=False)


@pytest.fixture
def private_tree(monkeypatch, trust_keys):
    if os.name != 'posix':pytest.skip('Actual held-FD/flock/private-mode contracts')
    # Production rejects writable ancestors; use a private HOME fixture, not /tmp.
    root = Path.home()/('.tsfp-generation-fixture-'+uuid.uuid4().hex);root.mkdir(mode=0o700)
    monkeypatch.setattr(migration,'ROOT',root);monkeypatch.setattr(migration,'ROOT_UID',os.geteuid());monkeypatch.setattr(system,'ROOT',root)
    host, scan = migration.helpers();monkeypatch.setattr(host,'ROOT',root);monkeypatch.setattr(scan,'ROOT',root)
    monkeypatch.setattr(migration,'helpers',lambda:(host,scan))
    origin = 'https://workshop.example.test:8443'
    pinned = make_root(trust_keys[:2],[trust_keys[4]],1,origin,int(time.time())-60)
    key = trust_keys[4].private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption())
    before, after = root/'releases'/('a'*64),root/'releases'/('b'*64)
    (root/'data').mkdir(mode=0o700);(root/'monitor').mkdir(mode=0o755)
    for directory, children in (('scan',scan.DIRECTORIES),('host',host.DIRECTORIES)):
        parent = root/'data'/directory;parent.mkdir(mode=0o700)
        for child in children:(parent/child).mkdir(mode=0o700)
    plan = migration.expected(before,after,pinned,key,origin)
    for generation in (before,after):
        profile=generation/'source/config/mods/public-workshop.json';profile.parent.mkdir(parents=True)
        profile.write_bytes(migration.wire(dict(schema=1,origin=origin,trusted_root_sha256=hashlib.sha256(pinned).hexdigest(),status='active')));profile.chmod(0o644)
    guards = {'host/api.json':host.wire(host.configs(before,origin)['api.json']), 'host/trusted-root.json':pinned,'host/catalog-1.pem':key}
    for name,value in dict(plan['before'],**guards).items():
        path = root/'data'/name;path.write_bytes(value);path.chmod(0o600)
    sentinel = root/'data/scan/repository/catalog.sqlite';sentinel.write_bytes(b'unchanged repository sentinel')
    nonce = 'c'*64
    def admission(**changes):
        now = int(time.time())
        value = dict(schema=1,producer='tsfp-upgrade-quiescence-v1',before=before.name,after=after.name,
                     uid=os.geteuid(),nonce=nonce,plan_sha256=plan['sha256'],backup_sha256='d'*64,
                     time=now,quiesced_at=now-10,backup_verified_at=now-5,stopped_jobs=migration.jobs())
        value.update(changes);path=root/'monitor/upgrade-quiescence.json';path.write_bytes(migration.wire(value));path.chmod(0o644)
    admission()
    try:yield root,before,after,nonce,plan,guards,sentinel,admission
    finally:
        assert root.parent == Path.home() and root.name.startswith('.tsfp-generation-fixture-')
        shutil.rmtree(root)


def members(root):
    return {name:(root/'data'/name).read_bytes() for name in migration.FIELDS}


def test_apply_idempotent_resume_and_rollback_preserve_keys_api_and_repository(private_tree):
    root,before,after,nonce,plan,guards,sentinel,admit = private_tree
    guard_metadata = {name:((root/'data'/name).stat().st_ino,(root/'data'/name).stat().st_mtime_ns) for name in guards}
    assert migration.inspect(before,after)['sha256'] == plan['sha256']
    for _ in range(2):assert migration.migrate(str(before),str(after),nonce)['phase'] == 'applied'
    assert members(root) == plan['after']
    assert migration.migrate(before,after,nonce,rollback=True)['phase'] == 'rolled-back'
    assert members(root) == plan['before']
    for name,value in guards.items():
        path=root/'data'/name;assert path.read_bytes()==value and (path.stat().st_ino,path.stat().st_mtime_ns)==guard_metadata[name]
    assert sentinel.read_bytes()==b'unchanged repository sentinel'
    journal = root/'data/host-migrations'/nonce
    assert all(value not in path.read_bytes() for path in journal.iterdir() for name,value in guards.items() if name.endswith('.pem'))


@pytest.mark.parametrize('change',[dict(time=0),dict(time=True),dict(backup_verified_at=0),dict(backup_sha256='bad'),dict(uid=-1),dict(nonce='e'*64),dict(plan_sha256='e'*64),dict(stopped_jobs=[]),dict(extra='unknown')])
def test_root_admission_denial_precedes_any_journal_or_config_change(private_tree,change):
    root,before,after,nonce,plan,_,_,admit=private_tree;admit(**change)
    with pytest.raises(ModError):migration.migrate(before,after,nonce)
    assert members(root)==plan['before'] and not (root/'data/host-migrations').exists()


@pytest.mark.parametrize('damage',['changed','symlink','hardlink','mode','unknown','state-link','root-update'])
def test_unknown_configuration_or_layout_is_preserved(private_tree,damage):
    root,before,after,nonce,plan,_,_,_=private_tree;path=root/'data/scan/service.json'
    if damage=='changed':path.write_bytes(b'preserve unknown config')
    elif damage=='symlink':path.rename(path.with_name('saved.json'));path.symlink_to('saved.json')
    elif damage=='hardlink':os.link(path,root/'linked-config')
    elif damage=='mode':path.chmod(0o644)
    elif damage=='unknown':(root/'data/host/unknown-file').write_text('preserve')
    elif damage=='state-link':
        state=root/'data/host/api-state';state.rmdir();state.symlink_to(root/'data/host/gateway-state',target_is_directory=True)
    else:(root/'data/host/root-updates/rotation.json').write_text('preserve rotation')
    before_bytes=path.read_bytes()
    with pytest.raises((OSError,ModError)):migration.migrate(before,after,nonce)
    assert path.read_bytes()==before_bytes and not (root/'data/host-migrations').exists()


@pytest.mark.parametrize('rollback',[False,True])
def test_interruption_recovers_only_known_file_generations(private_tree,monkeypatch,rollback):
    root,before,after,nonce,plan,_,sentinel,_=private_tree
    replace=migration._replace;writes=[]
    def interrupted(fd,name,current,stamp,target,temporary):
        replace(fd,name,current,stamp,target,temporary)
        if temporary.startswith('.migration-'):
            writes.append(name)
            if len(writes)==2:raise OSError('Injected interruption after durable generation replace')
    monkeypatch.setattr(migration,'_replace',interrupted)
    with pytest.raises(OSError):migration.migrate(before,after,nonce)
    current=members(root);assert any(current[name]==plan['after'][name] for name in migration.FIELDS)
    assert any(current[name]==plan['before'][name] for name in migration.FIELDS)
    monkeypatch.setattr(migration,'_replace',replace)
    assert migration.migrate(before,after,nonce,rollback=rollback)['phase']==('rolled-back' if rollback else 'applied')
    assert members(root)==plan['before' if rollback else 'after'] and sentinel.read_bytes()==b'unchanged repository sentinel'


def test_changed_state_after_interruption_is_never_overwritten(private_tree,monkeypatch):
    root,before,after,nonce,plan,_,_,_=private_tree
    replace=migration._replace
    def interrupted(*args):
        replace(*args)
        if args[-1].startswith('.migration-'):raise OSError('Interrupted')
    monkeypatch.setattr(migration,'_replace',interrupted)
    with pytest.raises(OSError):migration.migrate(before,after,nonce)
    monkeypatch.setattr(migration,'_replace',replace)
    path=root/'data/scan/definitions.json';path.write_bytes(b'preserve concurrent change')
    with pytest.raises(ModError):migration.migrate(before,after,nonce,rollback=True)
    assert path.read_bytes()==b'preserve concurrent change'


@pytest.mark.parametrize('rollback',[False,True])
def test_known_uncommitted_temporary_recovers_without_adopting_unknown_bytes(private_tree,monkeypatch,rollback):
    root,before,after,nonce,plan,_,_,_=private_tree
    replace=migration.os.replace
    def interrupted(source,destination,**kwargs):
        if str(source).startswith('.migration-'):raise OSError('Interrupted before configuration rename')
        return replace(source,destination,**kwargs)
    monkeypatch.setattr(migration.os,'replace',interrupted)
    with pytest.raises(OSError):migration.migrate(before,after,nonce)
    assert members(root)==plan['before']
    monkeypatch.setattr(migration.os,'replace',replace)
    assert migration.migrate(before,after,nonce,rollback=rollback)['phase']==('rolled-back' if rollback else 'applied')
    assert members(root)==plan['before' if rollback else 'after']


@pytest.mark.parametrize('damage',['mode','link','hardlink','foreign-authority'])
def test_admission_file_itself_must_have_root_authority(private_tree,monkeypatch,damage):
    root,before,after,nonce,plan,_,_,_=private_tree;path=root/'monitor/upgrade-quiescence.json'
    if damage=='mode':path.chmod(0o666)
    elif damage=='link':path.rename(path.with_name('saved-proof'));path.symlink_to('saved-proof')
    elif damage=='hardlink':os.link(path,root/'proof-alias')
    else:monkeypatch.setattr(migration,'ROOT_UID',os.geteuid()+1)
    with pytest.raises((OSError,ModError)):migration.migrate(before,after,nonce)
    assert members(root)==plan['before'] and not (root/'data/host-migrations').exists()


def test_partial_journal_and_missing_rollback_history_are_preserved(private_tree):
    root,before,after,nonce,plan,_,_,_=private_tree
    with pytest.raises(ModError):migration.migrate(before,after,nonce,rollback=True)
    journal=root/'data/host-migrations'/nonce;journal.mkdir(mode=0o700)
    marker=journal/'unknown-partial';marker.write_text('preserve')
    with pytest.raises(ModError):migration.migrate(before,after,nonce)
    assert marker.read_text()=='preserve' and members(root)==plan['before']


def test_immutable_origin_or_root_change_requires_explicit_separate_migration(private_tree):
    root,before,after,nonce,plan,_,_,_=private_tree
    profile=after/'source/config/mods/public-workshop.json';value=json.loads(profile.read_bytes());value['trusted_root_sha256']='f'*64
    profile.write_bytes(migration.wire(value))
    with pytest.raises(ModError,match='binding'):migration.migrate(before,after,nonce)
    assert members(root)==plan['before'] and not (root/'data/host-migrations').exists()
