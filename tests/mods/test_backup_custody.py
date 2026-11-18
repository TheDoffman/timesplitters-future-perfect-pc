"""New-only account custody, interrupted encryption and independent recovery."""
import hashlib
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
import mod_backup_custody as custody
from mod_package import ModError
from mod_trust import encode

ORIGIN = 'https://backup-custody-fixture.example.test:8443'
PIN = 'a' * 64
PASSWORD = 'Disposable independent backup key fixture'
WINDOWS = pytest.mark.skipif(os.name != 'nt', reason='Actual Windows account DPAPI and private NTFS custody')


@pytest.fixture
def prepared(tmp_path):
    root = tmp_path / 'private-custody'
    result = custody.prepare(root, ORIGIN, PIN)
    return root, result['preparation_sha256']


def files(root):
    return {p.relative_to(root).as_posix(): (p.stat().st_ino, p.stat().st_mtime_ns, p.read_bytes())
            for p in root.rglob('*') if p.is_file()}


@WINDOWS
def test_preparation_alone_is_not_recovery_completion_and_does_not_replace_existing(prepared):
    root, pin = prepared
    assert set(p.name for p in root.iterdir()) == {custody.STORE, custody.PREPARATION}
    before = files(root)
    with pytest.raises((ModError, OSError)):
        custody.inspect(root, pin, ORIGIN, PIN)
    with pytest.raises(ModError):
        custody.prepare(root, ORIGIN, PIN)
    assert files(root) == before


@WINDOWS
def test_complete_private_and_portable_files_recover_same_key_without_rewriting(prepared):
    root, pin = prepared
    result = custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD)
    assert result['portable_verified'] is True and result['services_started'] is result['cross_host_verified'] is False
    assert custody.inspect(root, pin, ORIGIN, PIN) == result
    expected = custody.recipient.load_windows(root / custody.STORE, ORIGIN, PIN)
    data = (root / custody.RECOVERY).read_bytes()
    recovered = custody.recipient.unseal(data, PASSWORD, ORIGIN, PIN)
    assert custody.recipient.secret(expected, ORIGIN, PIN) == custody.recipient.secret(recovered, ORIGIN, PIN)
    assert custody.recipient.secret(expected, ORIGIN, PIN) not in data
    before = files(root)
    assert custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD) == result
    assert files(root) == before


@WINDOWS
def test_mismatched_passphrase_and_changed_preparation_cannot_write_recovery(prepared):
    root, pin = prepared
    before = files(root)
    with pytest.raises(ModError, match='differ'):
        custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD + 'different')
    assert files(root) == before
    file = root / custody.PREPARATION
    file.write_bytes(file.read_bytes() + b' ')
    with pytest.raises(ModError, match='preparation differs'):
        custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD)
    assert not (root / custody.RECOVERY).exists()


@WINDOWS
def test_real_process_exit_after_ciphertext_preserves_file_and_resumes_same_key(prepared):
    root, pin = prepared
    script = '''
import os,sys
sys.path.insert(0,sys.argv[1])
import mod_backup_custody as c
original=c.windows.new_file
def crash(path,data):
    if path.name==c.COMPLETE: os._exit(99)
    return original(path,data)
c.windows.new_file=crash
c.finish(sys.argv[2],sys.argv[3],sys.argv[4],sys.argv[5],sys.argv[6],sys.argv[6])
'''
    result = subprocess.run([sys.executable, '-I', '-c', script, str(ROOT / 'tools/mods'), str(root), pin,
                             ORIGIN, PIN, PASSWORD], capture_output=True, timeout=60)
    assert result.returncode == 99, result.stderr.decode(errors='replace')
    assert not (root / custody.COMPLETE).exists()
    before = files(root)
    with pytest.raises(ModError, match='authenticate'):
        custody.finish(root, pin, ORIGIN, PIN, PASSWORD + 'wrong', PASSWORD + 'wrong')
    assert files(root) == before
    result = custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD)
    assert files(root)[custody.RECOVERY] == before[custody.RECOVERY]
    assert result['recovery_sha256'] == hashlib.sha256(before[custody.RECOVERY][2]).hexdigest()


@WINDOWS
def test_foreign_file_or_changed_account_key_is_preserved_and_refused(prepared):
    root, pin = prepared
    marker = root / 'personal'
    marker.write_bytes(b'preserve')
    with pytest.raises(ModError, match='foreign'):
        custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD)
    assert marker.read_bytes() == b'preserve'
    marker.unlink()
    key = root / custody.STORE / 'recipient.dpapi'
    key.write_bytes(key.read_bytes() + b'tamper')
    with pytest.raises(ModError, match='store changed'):
        custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD)
    assert key.read_bytes().endswith(b'tamper')


@WINDOWS
def test_tampered_completed_recovery_cannot_be_replaced_with_new_password(prepared):
    root, pin = prepared
    custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD)
    file = root / custody.RECOVERY
    data = file.read_bytes()
    file.write_bytes(data[:-1] + bytes([data[-1] ^ 1]))
    before = files(root)
    with pytest.raises(ModError, match='changed'):
        custody.finish(root, pin, ORIGIN, PIN, PASSWORD + 'other', PASSWORD + 'other')
    assert files(root) == before


@WINDOWS
def test_boolean_schema_cannot_impersonate_completed_custody(prepared):
    root, pin = prepared
    custody.finish(root, pin, ORIGIN, PIN, PASSWORD, PASSWORD)
    file = root / custody.COMPLETE
    value = custody.read_json(file.read_bytes())
    value['schema'] = True
    file.write_bytes(encode(value))
    with pytest.raises(ModError, match='changed'):
        custody.inspect(root, pin, ORIGIN, PIN)


@WINDOWS
def test_local_password_window_masks_clears_and_never_displays_passphrase(tmp_path, monkeypatch):
    import tkinter as tk
    from tkinter import ttk
    spec = importlib.util.spec_from_file_location('backup_key_window', ROOT / 'tools/mods/hosting/backup-recipient-windows.py')
    window = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(window)
    monkeypatch.setattr(custody, 'opened', lambda *a: None)
    received = []
    def finish(*args):
        assert args[-2:] == (PASSWORD, PASSWORD)
        received.append(True)
    monkeypatch.setattr(custody, 'finish', finish)
    constructor = tk.Tk
    def create():
        form = constructor()
        form.withdraw()  # Exercise only this disposable app; never capture a desktop.
        def exercise():
            try:
                frame = form.winfo_children()[0]
                entries = [w for w in frame.winfo_children() if isinstance(w, ttk.Entry)]
                button = next(w for w in frame.winfo_children() if isinstance(w, ttk.Button))
                assert len(entries) == 2 and all(w.cget('show') == '*' for w in entries)
                entries[0].insert(0, 'short')
                entries[1].insert(0, 'different')
                button.invoke()
                assert not received
                for entry in entries:
                    entry.delete(0, 'end')
                    entry.insert(0, PASSWORD)
                button.invoke()
                assert all(w.get() == '' and w.instate(['disabled']) for w in entries)
                deadline = time.monotonic() + 3
                while button.cget('text') != 'Close' and time.monotonic() < deadline:
                    form.update()
                    time.sleep(.01)
                assert received == [True] and button.cget('text') == 'Close'
                form.update_idletasks()
                assert button.winfo_y() + button.winfo_height() <= frame.winfo_height()
                for label in frame.winfo_children():
                    if isinstance(label, ttk.Label):
                        text = label.cget('text')
                        variable = label.cget('textvariable')
                        if variable:
                            text += form.getvar(variable)
                        assert PASSWORD not in text
            finally:
                form.destroy()
        form.mainloop = exercise
        return form
    monkeypatch.setattr(tk, 'Tk', create)
    window.show(tmp_path / 'disposable-private-key', 'a' * 64, ORIGIN, PIN)


@pytest.fixture
def portable(tmp_path):
    parent = tmp_path / 'private-recovery'
    if os.name == 'nt':
        custody.windows.private(parent, create=True)
    else:
        parent.mkdir(mode=0o700)
    key = X25519PrivateKey.generate()
    public = custody.recipient.public(key, ORIGIN, PIN)
    data = custody.recipient.seal(key, PASSWORD, ORIGIN, PIN)
    file = parent / 'key.enc.json'
    file.write_bytes(data)
    if os.name != 'nt':
        file.chmod(0o600)
    return file, hashlib.sha256(data).hexdigest(), public['key_id']


def test_portable_check_requires_no_account_store_or_plaintext_output(portable, monkeypatch):
    file, pin, key_id = portable
    monkeypatch.setattr(custody.recipient, 'load_windows', lambda *a: pytest.fail('Portable check touched Windows custody'))
    result = custody.check_portable(file, ORIGIN, PIN, pin, key_id, PASSWORD)
    assert result == dict(schema=1, phase='portable-backup-key-verified', key_id=key_id, recovery_sha256=pin,
                          account_independent=True, services_started=False)
    assert set(p.name for p in file.parent.iterdir()) == {file.name}


def test_foreign_ciphertext_refused_before_password_derivation(portable, monkeypatch):
    file, pin, key_id = portable
    monkeypatch.setattr(custody.recipient, 'unseal', lambda *a: pytest.fail('Unsealed before authenticated ciphertext hash'))
    with pytest.raises(ModError, match='fingerprint'):
        custody.check_portable(file, ORIGIN, PIN, 'b' * 64, key_id, PASSWORD)


@pytest.mark.parametrize('fault', ['password', 'key-id', 'origin', 'root'])
def test_portable_check_does_not_admit_wrong_password_or_identity(portable, fault):
    file, pin, key_id = portable
    with pytest.raises(ModError):
        custody.check_portable(file, 'https://other.example.test' if fault == 'origin' else ORIGIN,
                              'b' * 64 if fault == 'root' else PIN, pin,
                              'b' * 64 if fault == 'key-id' else key_id,
                              PASSWORD + 'wrong' if fault == 'password' else PASSWORD)
