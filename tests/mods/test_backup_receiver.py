"""Portable managed reception, durable crash states and deletion boundaries."""
import io
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_backup_receiver as receiver
import mod_backup_exports as exports
from mod_package import ModError
from mod_trust import encode
from test_backup_exports import case, ORIGIN, PIN

START = 1700000000
REMOTE = dict(host='fixture-mac', python='/python', entry='/entry', bundle='/bundle', record='/record')


class Crash(BaseException):
    pass


def pool(case, name, **kwargs):
    path = case.root / ('receiver-' + name)
    receiver.initialize(path, case.public, ORIGIN, PIN, REMOTE, **kwargs)
    return path


def sender(case, created, *, export_id=None, fault=None):
    value = dict(case.receipt, created=created, export_id=export_id or format(created, '032x'))
    wire = encode(value)
    frame = exports.MAGIC + struct.pack('>I', len(wire)) + wire + case.payload
    if fault == 'truncate':
        frame = frame[:-9]
    def receive(destination, accept, settings):
        result = exports.receive_stream(io.BytesIO(frame), destination, case.public, ORIGIN, PIN,
                                         verify_transport=lambda: None, accept_receipt=accept)
        if fault == 'crash-after-receipt':
            raise Crash('Process stopped after durable transfer completion')
        return result
    return receive


def tick(case, path, created=START, *, force=True, fault=None, now=None, export_id=None):
    return receiver.run(path, case.public, ORIGIN, PIN, sender(case, created, fault=fault, export_id=export_id),
                        force=force, clock=lambda: created if now is None else now)


def admitted(case, path):
    with receiver.opened(path, case.public, ORIGIN, PIN) as (root, settings, db):
        return [row for row in receiver.copies(db, settings, case.public, ORIGIN, PIN) if row['state'] == 'complete']


def test_receive_due_time_and_verified_rotation_preserve_only_known_old_copy(case):
    path = pool(case, 'rotation', keep=1)
    result = tick(case, path)
    assert result['status'] == 'received' and result['copies'] == {'complete': 1}
    original = path / admitted(case, path)[0]['name']
    def no_network(*args):
        raise AssertionError('Not-due worker contacted sender')
    result = receiver.run(path, case.public, ORIGIN, PIN, no_network, clock=lambda: START + 1)
    assert result['status'] == 'not-due' and original.exists()
    result = tick(case, path, START + 2)
    assert result['copies'] == {'complete': 1} and result['export_created'] == START + 2
    assert not original.exists() and (case.folder / exports.PAYLOAD).read_bytes() == case.payload
    current = path / admitted(case, path)[0]['name']
    assert (current / exports.PAYLOAD).read_bytes() == case.payload


def test_capacity_never_predeletes_last_good_copy_to_make_room(case):
    path = pool(case, 'capacity', keep=1, maximum_bytes=len(case.payload) + 1)
    tick(case, path)
    original = path / admitted(case, path)[0]['name']
    with pytest.raises(ModError, match='capacity exhausted'):
        tick(case, path, START + 1)
    assert (original / exports.PAYLOAD).read_bytes() == case.payload
    assert len(admitted(case, path)) == 1


@pytest.mark.parametrize('kind', ['older', 'same-time', 'future', 'changed-identity'])
def test_replay_skew_and_changed_known_export_refuse_before_copy_output(case, kind):
    path = pool(case, kind)
    tick(case, path)
    created = dict(older=START - 1, **{'same-time': START, 'future': START + 1000, 'changed-identity': START + 1})[kind]
    identifier = format(START, '032x') if kind == 'changed-identity' else 'f' * 32
    before = {item.name for item in path.iterdir()}
    with pytest.raises(ModError):
        tick(case, path, created, now=START + 10, export_id=identifier)
    assert {item.name for item in path.iterdir()} == before
    assert len(admitted(case, path)) == 1


def test_repeated_export_does_not_refresh_success_or_hide_staleness(case):
    path = pool(case, 'duplicate')
    first = tick(case, path)
    later = tick(case, path, now=START + 2 * 86400 + 1)
    assert later['status'] == 'source-unchanged' and later['copies'] == {'complete': 1}
    assert later['last_success'] == first['last_success'] == START
    assert later['stale_export'] and later['export_created'] == START


def test_failed_partial_is_preserved_but_later_success_can_proceed(case):
    path = pool(case, 'failure', keep=1)
    tick(case, path)
    with pytest.raises(ModError):
        tick(case, path, START + 10, fault='truncate')
    with receiver.opened(path, case.public, ORIGIN, PIN) as (root, settings, db):
        failed = [row for row in receiver.copies(db, settings, case.public, ORIGIN, PIN) if row['state'] == 'failed']
        assert len(failed) == 1 and (root / failed[0]['name'] / exports.PAYLOAD).exists()
        assert not (root / failed[0]['name'] / exports.RECEIPT).exists()
        partial = root / failed[0]['name'] / exports.PAYLOAD
        data = partial.read_bytes()
    result = tick(case, path, START + 20)
    assert result['copies'] == {'complete': 1, 'failed': 1} and partial.read_bytes() == data


def test_completed_known_transfer_recovers_after_crash_without_copying_again(case):
    path = pool(case, 'admission', keep=1)
    tick(case, path)
    with pytest.raises(Crash):
        tick(case, path, START + 10, fault='crash-after-receipt')
    def no_network(*args):
        raise AssertionError('Recovered complete transfer was downloaded again')
    result = receiver.run(path, case.public, ORIGIN, PIN, no_network, clock=lambda: START + 20)
    assert result['copies'] == {'complete': 1} and result['export_created'] == START + 10
    assert result['last_success'] == result['last_attempt'] == START + 20


def interrupt_retention(case, path, monkeypatch):
    tick(case, path)
    original = path / admitted(case, path)[0]['name']
    unlink = os.unlink
    def interrupted(target, *args, **kwargs):
        result = unlink(target, *args, **kwargs)
        if Path(target) == original / exports.PAYLOAD:
            raise Crash('Stopped between the two known unlinks')
        return result
    with monkeypatch.context() as patch:
        patch.setattr(receiver.os, 'unlink', interrupted)
        with pytest.raises(Crash):
            tick(case, path, START + 10)
    assert original.exists() and not (original / exports.PAYLOAD).exists()
    assert (original / exports.RECEIPT).exists()
    return original


def test_retention_resumes_interrupted_exact_file_deletion(case, monkeypatch):
    path = pool(case, 'delete', keep=1)
    original = interrupt_retention(case, path, monkeypatch)
    def no_network(*args):
        raise AssertionError('Retention recovery unnecessarily downloaded a copy')
    result = receiver.run(path, case.public, ORIGIN, PIN, no_network, clock=lambda: START + 11)
    assert result['copies'] == {'complete': 1} and not original.exists()
    assert len(admitted(case, path)) == 1


def test_replaced_retention_file_is_preserved_even_with_identical_bytes(case, monkeypatch):
    path = pool(case, 'delete-replaced', keep=1)
    original = interrupt_retention(case, path, monkeypatch)
    replacement = case.root / 'same-receipt-new-inode'
    data = (original / exports.RECEIPT).read_bytes()
    replacement.write_bytes(data)
    if os.name == 'posix':
        replacement.chmod(0o600)
    os.replace(replacement, original / exports.RECEIPT)
    with pytest.raises(ModError, match='replaced retention file'):
        tick(case, path, START + 11)
    assert (original / exports.RECEIPT).read_bytes() == data
    assert len(admitted(case, path)) == 1


def test_foreign_content_corrupt_copy_and_pool_exchange_are_preserved(case):
    path = pool(case, 'foreign')
    tick(case, path)
    unknown = path / 'keep'
    unknown.write_bytes(b'foreign')
    with pytest.raises(ModError, match='foreign'):
        tick(case, path, START + 1)
    assert unknown.read_bytes() == b'foreign'
    unknown.unlink()
    current = path / admitted(case, path)[0]['name'] / exports.PAYLOAD
    wire = current.read_bytes()
    current.write_bytes(wire[:-1] + bytes([wire[-1] ^ 1]))
    with pytest.raises(ModError, match='differs'):
        tick(case, path, START + 1)
    assert current.read_bytes() != wire
    first = pool(case, 'exchange-one')
    second = pool(case, 'exchange-two')
    swapped = (second / receiver.DATABASE).read_bytes()
    (first / receiver.DATABASE).write_bytes(swapped)
    with pytest.raises(ModError, match='Invalid receiver state'):
        tick(case, first)
    assert (first / receiver.DATABASE).read_bytes() == swapped


def test_exclusive_journal_lease_spans_commits_and_blocks_real_second_process(case):
    path = pool(case, 'lease')
    code = '''import sqlite3,sys
db=sqlite3.connect(sys.argv[1],timeout=0)
try: db.execute('SELECT * FROM meta').fetchall()
except sqlite3.OperationalError: sys.exit(0)
sys.exit(3)
'''
    with receiver.opened(path, case.public, ORIGIN, PIN) as (root, settings, db):
        with receiver.transaction(db):
            db.execute("UPDATE meta SET last_result='initialized'")
        result = subprocess.run([sys.executable, '-c', code, str(path / receiver.DATABASE)],
                                stdin=subprocess.DEVNULL, capture_output=True, timeout=10)
        assert result.returncode == 0
        with pytest.raises(sqlite3.OperationalError):
            with receiver.opened(path, case.public, ORIGIN, PIN):
                pytest.fail('Competing receiver admitted')
    result = subprocess.run([sys.executable, '-c', code, str(path / receiver.DATABASE)],
                            stdin=subprocess.DEVNULL, capture_output=True, timeout=10)
    assert result.returncode == 3


def test_operator_initialization_and_status_use_fixed_policy(case):
    record = case.root / 'receiver-public.json'
    record.write_bytes(encode(case.public))
    if os.name == 'posix':
        record.chmod(0o600)
    entry = Path(__file__).resolve().parents[2] / 'tools/mods/hosting/backup-receiver.py'
    path = case.root / 'receiver-cli'
    common = ['--pool', str(path), '--recipient-record', str(record), '--origin', ORIGIN, '--trusted-root-sha256', PIN]
    options = ['--ssh-host', REMOTE['host'], '--remote-python', REMOTE['python'], '--remote-entry', REMOTE['entry'],
               '--remote-bundle', REMOTE['bundle'], '--remote-recipient-record', REMOTE['record']]
    result = subprocess.run([sys.executable, str(entry), 'init', *common, *options], capture_output=True, timeout=30)
    assert result.returncode == 0 and b'backup-receiver-initialized' in result.stdout
    result = subprocess.run([sys.executable, str(entry), 'status', *common], capture_output=True, timeout=30)
    assert result.returncode == 3 and b'"stale_export": true' in result.stdout
    result = subprocess.run([sys.executable, str(entry), 'status', *common, *options], capture_output=True, timeout=30)
    assert result.returncode == 2 and b'Traceback' not in result.stderr
