"""Actual account DPAPI/NTFS protection and off-server root recovery ceremony."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mods'))
import mod_windows_recovery as recovery
from mod_package import ModError
spec=importlib.util.spec_from_file_location('windows_root_ceremony',ROOT/'tools/mods/hosting/prepare-workshop-trust-windows.py')
ceremony=importlib.util.module_from_spec(spec);spec.loader.exec_module(ceremony)

pytestmark=pytest.mark.skipif(os.name!='nt',reason='Actual Windows account DPAPI and NTFS private ACLs')


def test_dpapi_roundtrip_integrity_and_application_entropy(monkeypatch):
    secret=b'Disposable root recovery integrity fixture'
    protected=recovery.crypt(secret)
    assert secret not in protected and recovery.crypt(protected,decrypt=True)==secret
    damaged=bytearray(protected);damaged[len(damaged)//2]^=1
    with pytest.raises(ModError):recovery.crypt(bytes(damaged),decrypt=True)
    monkeypatch.setattr(recovery,'ENTROPY',b'wrong recovery application')
    with pytest.raises(ModError):recovery.crypt(protected,decrypt=True)


def test_private_directory_is_new_only_and_files_inherit_private_acl(tmp_path):
    directory=recovery.private(tmp_path/'private',create=True)
    assert recovery.private(directory)==directory
    path=directory/'secret.bin'
    recovery.new_file(path,b'disposable encrypted fixture')
    assert recovery.read_file(path)==b'disposable encrypted fixture'
    with pytest.raises(ModError):recovery.private(directory,create=True)
    with pytest.raises(FileExistsError):recovery.new_file(path,b'do not replace')
    linked=directory/'alias.bin';os.link(path,linked)
    with pytest.raises(ModError):recovery.read_file(path)
    assert path.read_bytes()==b'disposable encrypted fixture'


def test_actual_windows_root_ceremony_recovers_threshold_and_exports_only_online_inputs(tmp_path,trust_keys,monkeypatch):
    keys=iter([trust_keys[0],trust_keys[1],trust_keys[4]])
    monkeypatch.setattr(ceremony.rsa,'generate_private_key',lambda **_:next(keys))
    directory=tmp_path/'ceremony'
    result=ceremony.prepare(directory,'https://workshop.example.test:8443')
    assert result['phase']=='recovery-verified' and result['root_valid_now'] is True
    assert result['account_bound'] is True and result['root_keys_exported'] is False
    assert result['services_started'] is result['network_started'] is False
    incoming=directory/'host-input'
    assert set(p.name for p in incoming.iterdir())=={'origin.json','trusted-root.json','catalog-1.pem'}
    for name in ('root-a','root-b'):
        assert b'ENCRYPTED PRIVATE KEY' in (directory/name/'private.pem').read_bytes()
    assert b'PRIVATE KEY' not in (directory/'recovery.dpapi').read_bytes()
    before={p:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in directory.rglob('*') if p.is_file()}
    again=ceremony.check(directory)
    assert again==result
    assert before=={p:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in before}
    with pytest.raises(ModError):ceremony.prepare(directory,'https://workshop.example.test:8443')
    # The already checked store is preserved when its receipt or payload changes.
    receipt=directory/'ceremony.json';original=receipt.read_bytes()
    value=json.loads(original);value['phase']='preparing';receipt.write_text(json.dumps(value))
    with pytest.raises(ModError):ceremony.check(directory)
    receipt.write_bytes(original)
    capsule=directory/'recovery.dpapi';payload=capsule.read_bytes()
    capsule.write_bytes(payload+b'tamper')
    with pytest.raises(ModError):ceremony.check(directory)
    assert capsule.read_bytes()==payload+b'tamper'


def test_foreign_recovery_acl_is_refused(tmp_path):
    # A standard inherited temp directory has a different ACL; no mutation occurs.
    marker=tmp_path/'personal';marker.write_bytes(b'keep')
    with pytest.raises(ModError):recovery.private(tmp_path)
    assert marker.read_bytes()==b'keep'


def test_private_tree_checks_every_descendant_without_rewriting_foreign_acl(tmp_path):
    directory = recovery.private(tmp_path/'tree', create=True)
    nested = directory/'nested'
    nested.mkdir()
    child = nested/'content'
    child.write_bytes(b'preserve')
    assert recovery.private(directory, tree=True) == directory
    # Add a foreign principal only to this disposable child's ACL. Inherited
    # rules from a copied ACL would instead be recomputed from its private parent.
    powershell = Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    command = '''
$ErrorActionPreference='Stop'
$item=[IO.FileInfo]::new($args[0])
$acl=$item.GetAccessControl()
$sid=[Security.Principal.SecurityIdentifier]::new('S-1-1-0')
$rule=[Security.AccessControl.FileSystemAccessRule]::new($sid,[Security.AccessControl.FileSystemRights]::Read,[Security.AccessControl.AccessControlType]::Allow)
$acl.AddAccessRule($rule)
$item.SetAccessControl($acl)
'''
    # Use a script file so file paths are arguments, never executable code.
    script = tmp_path/'foreign-acl.ps1'
    script.write_text(command, encoding='utf-8')
    changed = subprocess.run([str(powershell), '-NoProfile', '-NonInteractive', '-File', str(script), str(child)],
                             capture_output=True, timeout=30)
    assert changed.returncode == 0, changed.stderr.decode(errors='replace')
    with pytest.raises(ModError):
        recovery.private(child)
    with pytest.raises(ModError):
        recovery.private(directory, tree=True)
    assert child.read_bytes() == b'preserve'
    with pytest.raises(ModError):
        recovery.private(child)


def test_tree_refuses_junction_without_touching_outside_files(tmp_path):
    directory = recovery.private(tmp_path/'private-tree', create=True)
    outside = tmp_path/'outside'
    outside.mkdir()
    marker = outside/'personal'
    marker.write_bytes(b'preserve')
    junction = directory/'junction'
    result = subprocess.run(['cmd', '/c', 'mklink', '/J', str(junction), str(outside)], capture_output=True)
    assert result.returncode == 0
    try:
        with pytest.raises(ModError):
            recovery.private(directory, tree=True)
        assert marker.read_bytes() == b'preserve'
    finally:
        junction.rmdir()  # Removes this directory junction, never its target.
