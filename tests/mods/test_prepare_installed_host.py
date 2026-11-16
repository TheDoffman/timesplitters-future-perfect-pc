"""Protected setup has no generic exec and never generates jobs before admission."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'tools/mods/hosting/prepare-installed-host-mac.py'
spec=importlib.util.spec_from_file_location('installed_host_preparer',SOURCE)
prepare=importlib.util.module_from_spec(spec);spec.loader.exec_module(prepare)
from mod_package import ModError


def test_default_plan_does_not_require_root_or_touch_service_state():
    result=subprocess.run([sys.executable,'-B',str(SOURCE)],check=True,capture_output=True,timeout=10)
    value=json.loads(result.stdout)
    assert value['phase']=='plan-only' and value['official_definition_network_required'] is True
    assert value['services_started'] is value['public_endpoint_started'] is False


@pytest.mark.parametrize('failure',[None,'scan-init','host-init','definitions','scanner','scan-generation'])
def test_preparation_requires_admission_before_any_root_job_generation(monkeypatch,failure):
    release=prepare.ROOT/'releases'/('a'*64);calls=[]
    monkeypatch.setattr(prepare,'read_inputs',lambda *_:{})
    monkeypatch.setattr(prepare,'verify_public_inputs',lambda *_:dict(origin='https://workshop.example.test',trusted_root_sha256='b'*64))
    monkeypatch.setattr(prepare,'copy_inputs',lambda *_:calls.append('copy'))
    records=[]
    monkeypatch.setattr(prepare,'admission_record',lambda *_:records.append('protected-admission'))
    def run(release,python,account,name,args=(),root=False,**options):
        calls.append((name,root,tuple(args)))
        if name=='initialize-private-scan-mac.py':return dict(phase='bad' if failure=='scan-init' else 'ready',release=release.name)
        if name=='initialize-private-host-mac.py':return dict(phase='bad' if failure=='host-init' else 'ready',release=release.name,origin='https://workshop.example.test',trusted_root_sha256='b'*64)
        if name=='definitions-service-mac.py':
            if failure=='definitions':raise ModError('update failed')
            return None
        if name=='accept-installed-scanner-mac.py':return dict(phase='bad' if failure=='scanner' else 'scanner-admitted',release=release.name,uid=600,temporary_state_removed=True,
            results=dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error'))
        return dict(phase='generated-only',services_started=failure=='scan-generation')
    monkeypatch.setattr(prepare,'command',run)
    if failure is None:
        value=prepare.prepare(release,dict(uid=600,gid=600),Path('/python'),Path('/inputs'),501)
        assert value['phase']=='configured-and-admitted' and value['services_started'] is value['public_endpoint_started'] is False
        assert [call[2] for call in calls[-3:]]==[('--role',role,'--generate') for role in prepare.ROLES]
        assert records==['protected-admission']
    else:
        with pytest.raises(ModError):prepare.prepare(release,dict(uid=600,gid=600),Path('/python'),Path('/inputs'),501)
        if failure!='scan-generation':assert not any(c[1] for c in calls[1:])
        assert records==[]


@pytest.mark.parametrize('name,args,root',[
    ('shell',(),True),('initialize-private-host-mac.py',('--apply',),True),
    ('definitions-service-mac.py',('--config','/anything'),False),
    ('system-host-monitor-mac.py',('--role','shell','--generate'),True),
    ('system-host-monitor-mac.py',('--role','gateway','--watchdog'),True),
    ('system-scan-monitor-mac.py',('--generate',),False)])
def test_preparation_has_no_generic_command_or_privileged_worker(name,args,root,monkeypatch):
    monkeypatch.setattr(prepare.subprocess,'run',lambda *_a,**_k:pytest.fail('Invalid command reached subprocess'))
    with pytest.raises(ModError):prepare.command(Path('/release'),Path('/python'),dict(uid=600,gid=600),name,args,root=root)


def test_private_children_clear_groups_and_use_minimal_environment(monkeypatch):
    def run(args,**options):
        assert args[1:3]==['-I','-B'] and args[-1]=='--apply'
        assert options['user']==options['group']==600 and options['extra_groups']==[]
        assert set(options['env'])=={'PATH','HOME','TMPDIR','LC_ALL','TZ'}
        return SimpleNamespace(returncode=0,stdout=b'{"phase":"ready"}',stderr=b'')
    monkeypatch.setattr(prepare.subprocess,'run',run)
    assert prepare.command(Path('/release'),Path('/python'),dict(uid=600,gid=600),'initialize-private-host-mac.py',['--apply'])==dict(phase='ready')


@pytest.fixture
def inputs(private_posix_path,monkeypatch):
    tmp_path=private_posix_path
    if os.name!='posix':pytest.skip('Actual POSIX private directories/nofollow files/fchown')
    monkeypatch.setattr(prepare,'ROOT',tmp_path)
    tmp_path.chmod(0o700);(tmp_path/'data').mkdir(mode=0o700)
    source=tmp_path/'incoming';source.mkdir(mode=0o700)
    payloads={name:('owned '+name).encode() for name in prepare.LIMITS}
    for name,wire in payloads.items():(source/name).write_bytes(wire);(source/name).chmod(0o600)
    return source,payloads,dict(uid=os.geteuid(),gid=os.getegid())


def test_private_copy_is_new_only_or_exact_reuse_and_rejects_output_paths(inputs):
    source,payloads,account=inputs
    assert prepare.read_inputs(source,os.geteuid())==payloads
    prepare.copy_inputs(payloads,account)
    target=prepare.ROOT/'data/host-input'
    before={p.name:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in target.iterdir()}
    prepare.copy_inputs(payloads,account)
    assert before=={p.name:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in target.iterdir()}
    (target/'origin.json').write_bytes(b'preserve changed input')
    with pytest.raises(ModError):prepare.copy_inputs(payloads,account)
    assert (target/'origin.json').read_bytes()==b'preserve changed input'
    with pytest.raises(ModError):prepare.copy_inputs({'../outside':b'forbidden'},account)


@pytest.mark.parametrize('damage',['mode','hardlink','fifo','symlink','extra','oversized'])
def test_online_input_aliases_and_unknown_state_are_preserved(inputs,damage):
    source,payloads,account=inputs;path=source/'catalog-1.pem'
    if damage=='mode':path.chmod(0o644)
    if damage=='hardlink':os.link(path,source.parent/'alias')
    if damage=='fifo':path.unlink();os.mkfifo(path,0o600)
    if damage=='symlink':path.rename(source.parent/'original');path.symlink_to(source.parent/'original')
    if damage=='extra':(source/'root-private.pem').write_bytes(b'preserve')
    if damage=='oversized':path.write_bytes(b'x'*16385)
    before={p.name:p.lstat().st_ino for p in source.iterdir()}
    with pytest.raises((OSError,ModError)):prepare.read_inputs(source,os.geteuid())
    assert before=={p.name:p.lstat().st_ino for p in source.iterdir()}


def test_real_input_ancestor_write_protection_remains_enforced(inputs):
    source,_,_=inputs
    source.parent.chmod(0o770)
    with pytest.raises(ValueError,match='ancestor'):prepare.read_inputs(source,os.geteuid())
