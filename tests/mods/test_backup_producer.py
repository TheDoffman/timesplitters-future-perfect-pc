"""Managed snapshot selection, ciphertext publication and read-only delivery."""
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_backup as backup
import mod_backup_pool as snapshots
import mod_backup_producer as producer
import mod_backup_exports as exports
import mod_backup_receiver as receiver
from mod_package import ModError
from mod_repository import Repository
from mod_trust import encode
from test_backup_exports import case, ORIGIN, PIN

pytestmark = pytest.mark.skipif(os.name != 'posix', reason='POSIX managed snapshot producer/sender')


class Crash(BaseException):
    pass


@pytest.fixture
def host(tmp_path, case, monkeypatch):
    root = tmp_path.resolve()
    root.chmod(0o700)
    clock = [int(backup.time.time()) - 100]
    monkeypatch.setattr(backup.time, 'time', lambda: clock[0])
    repository = Repository(root / 'repository')
    repository.root.chmod(0o700)
    sha = exports.cipher.digest(case.plain)
    blob = repository.root / 'blobs' / (sha + '.zip')
    blob.write_bytes(case.plain)
    blob.chmod(0o600)
    with repository.db() as db:
        db.execute('INSERT INTO releases VALUES(?,?,?,?,?,?)', (sha, 'fixture', '1.0.0', '{}', 'approved', '{}'))
    source = root / 'snapshots'
    snapshots.initialize(repository.root, source, keep=2, interval=3600)
    def capture():
        snapshots.run(source, force=True, clock=lambda: clock[0])
    capture()
    pool = root / 'producer'
    producer.initialize(source, pool, case.public, ORIGIN, PIN)
    return SimpleNamespace(root=root, pool=pool, source=source, clock=clock, capture=capture, public=case.public, key=case.key)


def tick(host):
    return producer.run(host.pool, host.public, ORIGIN, PIN, clock=lambda: host.clock[0])


def advance(host, seconds=10):
    host.clock[0] += seconds
    host.capture()


def current(host):
    pool, settings, _ = producer.policy(host.pool, host.public, ORIGIN, PIN)
    pointer = producer.pointer(pool, settings)
    return pointer, exports.inspect_export(pool / pointer['name'], host.public, ORIGIN, PIN)[2]


def test_newest_snapshot_selected_and_unchanged_export_never_refreshed(host):
    first = tick(host)
    old_pointer, old_receipt = current(host)
    old_plain = snapshots.entries(*snapshots.policy(host.source))[0][0]['path']
    assert first['status'] == 'exported' and first['published_created'] == host.clock[0]
    host.clock[0] += 20
    repeated = tick(host)
    assert current(host) == (old_pointer, old_receipt)
    assert repeated['status'] == 'source-unchanged' and repeated['last_success'] == first['last_success']
    advance(host)
    advance(host)
    updated = tick(host)
    pointer, receipt = current(host)
    assert receipt['created'] == host.clock[0] and receipt['cipher_sha256'] != old_receipt['cipher_sha256']
    assert not (host.pool / old_pointer['name']).exists() and not old_plain.exists()
    assert updated['copies'] == {'complete': 1}
    with producer.opened(host.pool, host.public, ORIGIN, PIN) as (pool, settings, db, access):
        assert db.execute('SELECT COUNT(*) FROM plans').fetchone()[0] == 1


def test_readonly_sender_needs_neither_source_pool_nor_private_journal(host, monkeypatch):
    tick(host)
    pointer, receipt = current(host)
    clone = exports.new_directory(host.root / 'readonly')
    for name in (producer.POLICY, producer.LOCK, producer.CURRENT):
        shutil.copyfile(host.pool / name, clone / name)
        (clone / name).chmod(0o600)
    shutil.copytree(host.pool / pointer['name'], clone / pointer['name'])
    (clone / pointer['name']).chmod(0o700)
    assert not (clone / producer.DATABASE).exists()
    def denied(*args, **kwargs):
        raise AssertionError('Read-only sender accessed private source or journal')
    monkeypatch.setattr(producer, 'source_policy', denied)
    monkeypatch.setattr(producer.journal, 'database', denied)
    stream = io.BytesIO()
    assert producer.send_current(clone, stream, host.public, ORIGIN, PIN) == receipt
    received = exports.receive_stream(io.BytesIO(stream.getvalue()), host.root / 'received', host.public, ORIGIN, PIN,
                                      verify_transport=lambda: None)
    assert received == receipt


def test_sender_lease_defers_publication_without_blocking_new_encryption(host):
    tick(host)
    previous, receipt = current(host)
    advance(host)
    with producer.lease(host.pool):
        deferred = tick(host)
        assert deferred['status'] == 'publication-pending' and deferred['copies'] == {'complete': 2}
        assert current(host) == (previous, receipt)
        output = io.BytesIO()
        assert producer.send_current(host.pool, output, host.public, ORIGIN, PIN) == receipt
    settled = tick(host)
    assert settled['published_created'] == host.clock[0] and settled['copies'] == {'complete': 1}
    assert current(host)[0] != previous


@pytest.mark.parametrize('boundary', ['receipt', 'pointer-before', 'pointer-after'])
def test_crash_recovers_known_completion_or_pointer_without_encrypting_again(host, monkeypatch, boundary):
    tick(host)
    advance(host)
    with monkeypatch.context() as patch:
        if boundary == 'receipt':
            original = producer.exports.export_snapshot
            def crash(*args, **kwargs):
                original(*args, **kwargs)
                raise Crash('After complete local export, before journal admission')
            patch.setattr(producer.exports, 'export_snapshot', crash)
        else:
            original = os.replace
            def crash(source, destination):
                if Path(destination) == host.pool / producer.CURRENT:
                    if boundary == 'pointer-after':
                        original(source, destination)
                    raise Crash('Publication pointer boundary')
                return original(source, destination)
            patch.setattr(producer.os, 'replace', crash)
        with pytest.raises(Crash):
            tick(host)
    def no_encryption(*args, **kwargs):
        raise AssertionError('Known complete export was encrypted again')
    monkeypatch.setattr(producer.exports, 'export_snapshot', no_encryption)
    result = tick(host)
    assert result['published_created'] == host.clock[0] and result['copies'] == {'complete': 1}
    with producer.opened(host.pool, host.public, ORIGIN, PIN) as (pool, settings, db, access):
        assert receiver.state(db, settings)['publication'] is None
    assert not list(host.pool.glob('.current-*.tmp'))


def test_failed_ciphertext_is_preserved_and_healthy_retry_can_publish(host, monkeypatch):
    tick(host)
    advance(host)
    with monkeypatch.context() as patch:
        def disk_failure(source, destination, *args, **kwargs):
            destination = exports.new_directory(destination)
            (destination / exports.PAYLOAD).write_bytes(b'partial')
            (destination / exports.PAYLOAD).chmod(0o600)
            raise OSError('Injected disk failure')
        patch.setattr(producer.exports, 'export_snapshot', disk_failure)
        with pytest.raises(OSError):
            tick(host)
    result = tick(host)
    assert result['copies'] == {'complete': 1, 'failed': 1}
    with producer.opened(host.pool, host.public, ORIGIN, PIN) as (pool, settings, db, access):
        row = next(row for row in receiver.copies(db, settings, host.public, ORIGIN, PIN) if row['state'] == 'failed')
        assert (pool / row['name'] / exports.PAYLOAD).read_bytes() == b'partial'


def test_capacity_and_source_policy_exchange_preserve_published_export(host):
    limited = host.root / 'limited'
    producer.initialize(host.source, limited, host.public, ORIGIN, PIN, maximum_bytes=2 * 1024**2)
    producer.run(limited, host.public, ORIGIN, PIN, clock=lambda: host.clock[0])
    before = (limited / producer.CURRENT).read_bytes()
    advance(host)
    with pytest.raises(ModError, match='capacity exhausted'):
        producer.run(limited, host.public, ORIGIN, PIN, clock=lambda: host.clock[0])
    assert (limited / producer.CURRENT).read_bytes() == before
    source_policy = host.source / snapshots.POLICY
    original = source_policy.read_bytes()
    value = json.loads(original)
    value['interval'] += 1
    source_policy.write_bytes(encode(value))
    with pytest.raises(ModError, match='source binding changed'):
        producer.run(limited, host.public, ORIGIN, PIN, clock=lambda: host.clock[0])
    assert (limited / producer.CURRENT).read_bytes() == before


def test_receivers_with_fixed_producer_binding_get_successive_fresh_generations(host):
    tick(host)
    remote = dict(host='fixture-mac', python='/python', entry='/entry', bundle='/producer', record='/record', kind='producer')
    target = host.root / 'receiver'
    receiver.initialize(target, host.public, ORIGIN, PIN, remote, keep=1)
    def receive(destination, accept, settings):
        assert settings['remote']['kind'] == 'producer'
        wire = io.BytesIO()
        producer.send_current(host.pool, wire, host.public, ORIGIN, PIN)
        return exports.receive_stream(io.BytesIO(wire.getvalue()), destination, host.public, ORIGIN, PIN,
                                       verify_transport=lambda: None, accept_receipt=accept)
    first = receiver.run(target, host.public, ORIGIN, PIN, receive, clock=lambda: host.clock[0])
    advance(host)
    tick(host)
    second = receiver.run(target, host.public, ORIGIN, PIN, receive, force=True, clock=lambda: host.clock[0])
    assert second['export_created'] > first['export_created'] and second['copies'] == {'complete': 1}


def test_operator_producer_cli_and_binary_sender(host):
    record = host.root / 'public.json'
    record.write_bytes(encode(host.public))
    record.chmod(0o600)
    entry = Path(__file__).resolve().parents[2] / 'tools/mods/hosting/backup-producer.py'
    path = host.root / 'cli-producer'
    common = ['--pool', str(path), '--recipient-record', str(record), '--origin', ORIGIN, '--trusted-root-sha256', PIN]
    result = subprocess.run([sys.executable, str(entry), 'init', *common, '--snapshot-pool', str(host.source)],
                            capture_output=True, timeout=15)
    assert result.returncode == 0
    result = subprocess.run([sys.executable, str(entry), 'tick', *common], capture_output=True, timeout=15)
    assert result.returncode == 0 and json.loads(result.stdout)['published_created'] == host.clock[0]
    transfer = entry.with_name('backup-transfer.py')
    result = subprocess.run([sys.executable, str(transfer), 'send-current', '--producer', str(path),
                             '--recipient-record', str(record), '--origin', ORIGIN, '--trusted-root-sha256', PIN],
                            capture_output=True, timeout=15)
    assert result.returncode == 0 and result.stdout.startswith(exports.MAGIC) and not result.stderr


def test_unchanged_snapshot_remains_stale_and_future_snapshot_refuses(host):
    first = tick(host)
    host.clock[0] += 2 * 3600 + 1
    old = tick(host)
    assert old['stale_export'] and old['last_success'] == first['last_success']
    advance(host)
    with pytest.raises(ModError, match='future'):
        producer.run(host.pool, host.public, ORIGIN, PIN, clock=lambda: host.clock[0] - 1)
