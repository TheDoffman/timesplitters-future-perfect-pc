"""Frozen operator admission rejects changed inventories and import injection."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
import mod_backup_runtime as runtime

admission = runtime.bootstrap()


@pytest.fixture
def frozen(tmp_path):
    root = tmp_path / 'private-frozen'
    names = ['bootstrap.py', 'runtime/python.exe', 'runtime/pythonw.exe', 'runtime/python3.dll',
             'runtime/python312.dll', 'runtime/LICENSE.txt', 'source/mod_windows_job.py',
             'source/mod_windows_recovery.py', 'source/mod_backup_worker.py',
             'source/hosting/private-recovery-windows.ps1', 'runtime/Lib/empty/__init__.py']
    for name in names:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'' if name.endswith('__init__.py') else b'disposable non-executable admission fixture')
    (root / 'runtime/python312._pth').write_bytes(b'Lib\nDLLs\n.\n')
    value = dict(schema=1, type=admission.TYPE, source_revision='a' * 40, python_version=[3, 12, 14],
                 files={p.relative_to(root).as_posix(): dict(bytes=p.stat().st_size, sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                        for p in root.rglob('*') if p.is_file()})
    def publish():
        wire = admission.canonical(value)
        (root / admission.MANIFEST).write_bytes(wire)
        return hashlib.sha256(wire).hexdigest()
    return root, value, publish


def test_exact_frozen_inventory_including_empty_package_initializers(frozen):
    root, value, publish = frozen
    assert admission.verify(root, publish()) == value
    assert (root / 'runtime/Lib/empty/__init__.py').read_bytes() == b''


@pytest.mark.parametrize('name', ['../outside', '/absolute', 'source/../outside', 'runtime/con.txt', 'runtime/bad.', 'runtime/bad:ads', 'runtime\\escape'])
def test_manifest_path_injection_refused_before_any_inventory_reads(frozen, name):
    root, value, publish = frozen
    value['files'][name] = dict(bytes=1, sha256='b' * 64)
    with pytest.raises(ValueError, match='inventory name'):
        admission.verify(root, publish())


def test_changed_manifest_or_payload_and_foreign_entries_preserved(frozen):
    root, value, publish = frozen
    pin = publish()
    with pytest.raises(ValueError, match='manifest differs'):
        admission.verify(root, 'b' * 64)
    path = root / 'source/mod_backup_worker.py'
    old = path.read_bytes()
    path.write_bytes(old + b'changed')
    with pytest.raises(ValueError, match='fingerprint'):
        admission.verify(root, pin)
    assert path.read_bytes() == old + b'changed'
    path.write_bytes(old)
    foreign = root / 'personal'
    foreign.write_bytes(b'preserve')
    with pytest.raises(ValueError, match='Foreign runtime file'):
        admission.verify(root, pin)
    assert foreign.read_bytes() == b'preserve'
    foreign.unlink()
    foreign.mkdir()
    with pytest.raises(ValueError, match='Foreign runtime directory'):
        admission.verify(root, pin)


def test_hashes_do_not_authorize_site_or_environment_imports(frozen):
    root, value, publish = frozen
    name = 'runtime/python312._pth'
    data = b'Lib\nDLLs\n.\nimport site\n'
    (root / name).write_bytes(data)
    value['files'][name] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    with pytest.raises(ValueError, match='import isolation'):
        admission.verify(root, publish())


def test_hashes_do_not_authorize_colliding_names_or_hardlinks(frozen):
    root, value, publish = frozen
    name = 'source/mod_backup_worker.py'
    value['files'][name.upper()] = value['files'][name]
    with pytest.raises(ValueError, match='Colliding'):
        admission.verify(root, publish())
    del value['files'][name.upper()]
    pin = publish()
    os.link(root / name, root.parent / 'hardlink-outside')
    with pytest.raises(ValueError, match='Unsafe runtime object'):
        admission.verify(root, pin)


def test_duplicate_json_and_changed_tree_budget_are_refused(frozen, monkeypatch):
    root, value, publish = frozen
    publish()
    data = (root / admission.MANIFEST).read_bytes().replace(b'"schema":1', b'"schema":1,"schema":1')
    (root / admission.MANIFEST).write_bytes(data)
    with pytest.raises(ValueError, match='Duplicate'):
        admission.verify(root, hashlib.sha256(data).hexdigest())
    pin = publish()
    monkeypatch.setattr(admission, 'MAX_TOTAL', 1)
    with pytest.raises(ValueError, match='payload exceeds'):
        admission.verify(root, pin)
