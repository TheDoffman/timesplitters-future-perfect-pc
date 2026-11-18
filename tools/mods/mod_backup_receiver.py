"""Private managed encrypted reception, durable admission and verified retention.

Only copies created by this newly initialized pool are managed. Failed partials
are preserved and count against capacity. This module installs no scheduled job.
"""
from contextlib import closing, contextmanager
from itertools import islice
import os
from pathlib import Path
import re
import shutil
import sqlite3
import time
import uuid

import mod_backup_cipher as cipher
import mod_backup_exports as exports
import mod_backup_transport as transport
from mod_package import ModError, read_json, require
from mod_trust import encode

POLICY = 'receiver.json'
DATABASE = 'receiver.sqlite'
TYPE = 'tsfp-backup-receiver-v1'
MAX_COPIES = 64
MAX_DB = 1024**2
MAX_POLICY = 64 * 1024
RESERVE = 4 * MAX_DB
SCHEMA = {
    'meta': 'CREATE TABLE meta (singleton INTEGER PRIMARY KEY CHECK(singleton=1), pool_id TEXT NOT NULL, last_attempt INTEGER NOT NULL, last_success INTEGER NOT NULL, highwater INTEGER NOT NULL, last_result TEXT NOT NULL)',
    'copies': 'CREATE TABLE copies (name TEXT PRIMARY KEY, state TEXT NOT NULL, receipt TEXT, deletion TEXT)',
}


class AlreadyHeld(ModError):
    pass


def read_private(path, maximum):
    with cipher.incoming(path, maximum) as (stream, size, stable):
        wire = exports.bounded_read(stream, size)
        require(stream.read(1) == b'', 'Receiver metadata grew')
        stable()
    return wire


def initialize(pool, recipient, service_origin, pin, remote, *, keep=7, interval=86400,
               maximum_bytes=cipher.MAX_WIRE):
    value = dict(schema=1, type=TYPE, pool_id=uuid.uuid4().hex, recipient=recipient,
                 remote=remote, keep=keep, interval=interval, maximum_bytes=maximum_bytes)
    check_policy(value, recipient, service_origin, pin)
    require(len(encode(value)) <= MAX_POLICY, 'Receiver policy exceeds budget')
    pool = cipher.private_path(pool)
    require(shutil.disk_usage(pool.parent).free >= RESERVE + cipher.MIN_FREE, 'Insufficient receiver journal headroom')
    pool = exports.new_directory(pool)
    with cipher.outgoing(pool / POLICY, len(encode(value))) as output:
        output.write(encode(value))
    fd = os.open(pool / DATABASE, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    os.close(fd)
    with closing(sqlite3.connect(pool / DATABASE, isolation_level=None)) as db:
        db.execute('PRAGMA page_size=4096')
        db.execute('PRAGMA synchronous=FULL')
        db.execute('PRAGMA max_page_count=256')
        with transaction(db):
            for statement in SCHEMA.values():
                db.execute(statement)
            db.execute('INSERT INTO meta VALUES(1,?,-1,-1,-1,?)', (value['pool_id'], 'initialized'))
    exports.sync(pool)
    exports.sync(pool.parent)
    return value


def check_policy(value, recipient, service_origin, pin):
    exports.recipients.checked_public(recipient, service_origin, pin)
    require(isinstance(value, dict) and set(value) ==
            {'schema', 'type', 'pool_id', 'recipient', 'remote', 'keep', 'interval', 'maximum_bytes'}
            and type(value['schema']) is int and value['schema'] == 1 and value['type'] == TYPE
            and isinstance(value['pool_id'], str) and re.fullmatch('[0-9a-f]{32}', value['pool_id'])
            and value['recipient'] == recipient, 'Receiver authority or pool identity differs')
    require(type(value['keep']) is int and 1 <= value['keep'] <= 32
            and type(value['interval']) is int and 3600 <= value['interval'] <= 604800
            and type(value['maximum_bytes']) is int and 0 < value['maximum_bytes'] <= 32 * cipher.MAX_WIRE,
            'Invalid receiver limits')
    remote = value['remote']
    require(isinstance(remote, dict) and set(remote) == {'host', 'python', 'entry', 'bundle', 'record'},
            'Invalid fixed receiver SSH settings')
    transport.ssh_command(*(remote[key] for key in ('host', 'python', 'entry', 'bundle', 'record')), service_origin, pin)


@contextmanager
def transaction(db):
    db.execute('BEGIN EXCLUSIVE')
    try:
        yield
        db.execute('COMMIT')
    except BaseException:
        if db.in_transaction:
            db.execute('ROLLBACK')
        raise


def copy_name(settings, name):
    return isinstance(name, str) and re.fullmatch('copy-' + settings['pool_id'] + '-[0-9a-f]{32}', name)


def layout(pool, settings):
    with os.scandir(pool) as stream:
        names = [entry.name for entry in islice(stream, MAX_COPIES + 4)]
    require(len(names) <= MAX_COPIES + 3 and {POLICY, DATABASE} <= set(names), 'Receiver directory budget or inventory differs')
    for name in names:
        require(name in (POLICY, DATABASE, DATABASE + '-journal') or copy_name(settings, name),
                'Preserving foreign receiver entry')
    if DATABASE + '-journal' in names:
        cipher.regular(pool / (DATABASE + '-journal'), 2 * MAX_DB)
    return set(names) - {POLICY, DATABASE, DATABASE + '-journal'}


@contextmanager
def opened(pool, recipient, service_origin, pin):
    pool = cipher.private_path(Path(pool) / POLICY).parent
    wire = read_private(pool / POLICY, MAX_POLICY)
    settings = read_json(wire)
    require(encode(settings) == wire, 'Noncanonical receiver policy')
    check_policy(settings, recipient, service_origin, pin)
    layout(pool, settings)
    before = cipher.regular(pool / DATABASE, MAX_DB)
    db = sqlite3.connect((pool / DATABASE).as_uri() + '?mode=rw', uri=True, timeout=0, isolation_level=None)
    db.row_factory = sqlite3.Row
    try:
        db.execute('PRAGMA trusted_schema=OFF')
        db.execute('PRAGMA synchronous=FULL')
        require(db.execute('PRAGMA journal_mode').fetchone()[0] == 'delete', 'Unexpected receiver journal mode')
        require(db.execute('PRAGMA locking_mode=EXCLUSIVE').fetchone()[0] == 'exclusive', 'Receiver lease unavailable')
        with transaction(db):
            observed = {row['name']: row['sql'] for row in db.execute("SELECT name,sql FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'")}
            require(observed == SCHEMA and db.execute('PRAGMA page_size').fetchone()[0] == 4096
                    and db.execute('PRAGMA quick_check').fetchone()[0] == 'ok', 'Unknown or corrupt receiver journal')
            meta = state(db, settings)
        after = cipher.regular(pool / DATABASE, MAX_DB)
        require((before.st_dev, before.st_ino) == (after.st_dev, after.st_ino), 'Receiver journal identity changed')
        require(db.execute('PRAGMA max_page_count=256').fetchone()[0] == 256, 'Receiver journal budget exceeded')
        rows = copies(db, settings, recipient, service_origin, pin)
        require(layout(pool, settings) == {row['name'] for row in rows if (pool / row['name']).exists() or (pool / row['name']).is_symlink()},
                'Preserving unrecorded receiver copy')
        yield pool, settings, db
        require(read_private(pool / POLICY, MAX_POLICY) == wire, 'Receiver policy changed during operation')
    finally:
        db.close()


def state(db, settings):
    rows = db.execute('SELECT * FROM meta LIMIT 2').fetchall()
    require(len(rows) == 1, 'Invalid receiver journal identity')
    value = dict(rows[0])
    require(value['singleton'] == 1 and value['pool_id'] == settings['pool_id']
            and all(type(value[key]) is int and -1 <= value[key] < 10**12 for key in ('last_attempt', 'last_success', 'highwater'))
            and value['last_success'] <= value['last_attempt']
            and value['last_result'] in ('initialized', 'received', 'failed', 'source-unchanged'), 'Invalid receiver state')
    return value


def copies(db, settings, recipient, service_origin, pin):
    rows = [dict(row) for row in db.execute('SELECT * FROM copies ORDER BY name LIMIT ?', (MAX_COPIES + 1,))]
    require(len(rows) <= MAX_COPIES, 'Receiver copy count exceeded')
    for row in rows:
        require(copy_name(settings, row['name']) and row['state'] in ('receiving', 'complete', 'failed', 'deleting'), 'Unknown receiver copy')
        if row['receipt'] is not None:
            require(isinstance(row['receipt'], str), 'Invalid recorded receiver receipt')
            row['value'] = exports.checked_receipt(row['receipt'].encode('ascii'), recipient, service_origin, pin)
        else:
            require(row['state'] in ('receiving', 'failed'), 'Missing admitted receiver receipt')
        require((row['deletion'] is not None) == (row['state'] == 'deleting'), 'Invalid receiver deletion intent')
        if row['deletion'] is not None:
            require(isinstance(row['deletion'], str) and len(row['deletion']) <= 4096, 'Receiver deletion record exceeds budget')
    complete = [row['value'] for row in rows if row['state'] == 'complete']
    require(len({value['export_id'] for value in complete}) == len(complete)
            and len({value['created'] for value in complete}) == len(complete), 'Duplicate admitted receiver export')
    return rows


def inspect(pool, row, recipient, service_origin, pin):
    folder, wire, value = exports.inspect_export(pool / row['name'], recipient, service_origin, pin)
    require(wire.decode('ascii') == row['receipt'], 'Receiver copy differs from admitted receipt')
    return folder, value


def totals(pool, rows, recipient, service_origin, pin):
    total = 0
    for row in rows:
        folder = pool / row['name']
        if row['state'] == 'complete':
            total += inspect(pool, row, recipient, service_origin, pin)[1]['cipher_bytes']
        elif folder.exists() or folder.is_symlink():
            cipher.private_path(folder / exports.PAYLOAD)
            with os.scandir(folder) as stream:
                names = [entry.name for entry in islice(stream, 3)]
            require(set(names) <= {exports.PAYLOAD, exports.RECEIPT} and len(names) <= 2,
                    'Preserving foreign partial receiver content')
            for name in names:
                info = cipher.regular(folder / name, cipher.MAX_WIRE if name == exports.PAYLOAD else exports.MAX_RECEIPT)
                if name == exports.PAYLOAD:
                    total += info.st_size
    return total


def admit(db, settings, name, receipt, now):
    require(receipt['created'] > state(db, settings)['highwater'], 'Receiver export would roll back')
    with transaction(db):
        updated = db.execute("UPDATE copies SET state='complete' WHERE name=? AND state='receiving' AND receipt=?",
                             (name, encode(receipt).decode('ascii')))
        require(updated.rowcount == 1, 'Receiver attempt changed before admission')
        db.execute("UPDATE meta SET highwater=?,last_success=?,last_attempt=MAX(last_attempt,?),last_result='received'",
                   (receipt['created'], now, now))


def recover(pool, settings, db, recipient, service_origin, pin, now):
    for row in copies(db, settings, recipient, service_origin, pin):
        if row['state'] != 'receiving':
            continue
        folder = pool / row['name']
        if not folder.exists() and not folder.is_symlink():
            with transaction(db):
                db.execute('DELETE FROM copies WHERE name=?', (row['name'],))
        elif (folder / exports.RECEIPT).exists():
            require(row['receipt'] is not None, 'Preserving unbound receiver completion')
            receipt = inspect(pool, row, recipient, service_origin, pin)[1]
            admit(db, settings, row['name'], receipt, now)
        else:
            with transaction(db):
                db.execute("UPDATE copies SET state='failed' WHERE name=?", (row['name'],))


def identity(path, maximum):
    return list(cipher.file_stamp(cipher.regular(path, maximum)))


def remove(pool, row, recipient, service_origin, pin):
    """Resume only an admitted exact two-file deletion; never recurse/follow."""
    record = read_json(row['deletion'].encode('ascii'))
    require(isinstance(record, dict) and set(record) == {'directory', 'files'}
            and isinstance(record['directory'], list) and len(record['directory']) == 2
            and isinstance(record['files'], dict) and set(record['files']) == {exports.PAYLOAD, exports.RECEIPT},
            'Invalid receiver deletion binding')
    stamps = [record['directory'], *record['files'].values()]
    require(all(isinstance(stamp, list) and all(type(n) is int and 0 <= n < 2**128 for n in stamp) for stamp in stamps)
            and all(len(record['files'][name]) == (4 if os.name == 'nt' else 5)
                    for name in (exports.PAYLOAD, exports.RECEIPT)), 'Invalid receiver deletion identity')
    folder = pool / row['name']
    if not folder.exists() and not folder.is_symlink():
        return
    cipher.private_path(folder / exports.PAYLOAD)
    info = folder.lstat()
    require([info.st_dev, info.st_ino] == record['directory'], 'Preserving replaced receiver directory')
    with os.scandir(folder) as stream:
        names = [entry.name for entry in islice(stream, 3)]
    require(set(names) <= {exports.PAYLOAD, exports.RECEIPT} and len(names) <= 2, 'Preserving foreign retention content')
    for name in names:
        maximum = cipher.MAX_WIRE if name == exports.PAYLOAD else exports.MAX_RECEIPT
        require(identity(folder / name, maximum) == record['files'][name], 'Preserving replaced retention file')
        if name == exports.PAYLOAD:
            exports.verify_payload(folder / name, row['value'], recipient, service_origin, pin)
        else:
            require(read_private(folder / name, maximum).decode('ascii') == row['receipt'], 'Retention receipt changed')
    for name in (exports.PAYLOAD, exports.RECEIPT):
        if name in names:
            require(identity(folder / name, cipher.MAX_WIRE if name == exports.PAYLOAD else exports.MAX_RECEIPT)
                    == record['files'][name], 'Retention file changed before removal')
            os.unlink(folder / name)
    exports.sync(folder)
    folder.rmdir()
    exports.sync(pool)


def retain(pool, settings, db, recipient, service_origin, pin):
    rows = copies(db, settings, recipient, service_origin, pin)
    complete = sorted((row for row in rows if row['state'] == 'complete'), key=lambda row: row['value']['created'])
    highwater = state(db, settings)['highwater']
    require((not complete and highwater == -1) or (complete and complete[-1]['value']['created'] == highwater),
            'Receiver high-water copy is missing')
    if complete:
        inspect(pool, complete[-1], recipient, service_origin, pin)
    pending = [row for row in rows if row['state'] == 'deleting']
    for row in complete[:-settings['keep']]:
        folder = inspect(pool, row, recipient, service_origin, pin)[0]
        info = folder.lstat()
        deletion = dict(directory=[info.st_dev, info.st_ino], files={
            name: identity(folder / name, cipher.MAX_WIRE if name == exports.PAYLOAD else exports.MAX_RECEIPT)
            for name in (exports.PAYLOAD, exports.RECEIPT)})
        with transaction(db):
            db.execute("UPDATE copies SET state='deleting',deletion=? WHERE name=?", (encode(deletion).decode('ascii'), row['name']))
        pending.append(dict(row, state='deleting', deletion=encode(deletion).decode('ascii')))
    require(not pending or complete, 'A verified retained copy is required for deletion')
    for row in pending:
        require(row['value']['created'] < highwater, 'Newest receiver copy must survive retention')
        remove(pool, row, recipient, service_origin, pin)
        with transaction(db):
            db.execute('DELETE FROM copies WHERE name=?', (row['name'],))


def report(db, settings, now):
    value = state(db, settings)
    counts = dict(db.execute('SELECT state,COUNT(*) FROM copies GROUP BY state').fetchall())
    complete = [read_json(row[0]) for row in db.execute("SELECT receipt FROM copies WHERE state='complete' LIMIT ?", (MAX_COPIES + 1,))]
    require((not complete and value['highwater'] == -1) or
            (complete and max(receipt['created'] for receipt in complete) == value['highwater']), 'Receiver high-water copy is missing')
    age = now - value['highwater'] if value['highwater'] >= 0 else None
    return dict(schema=1, phase='backup-receiver', status=value['last_result'], last_attempt=value['last_attempt'],
                last_success=value['last_success'], export_created=value['highwater'], export_age_seconds=age,
                stale_export=age is None or age > 2 * settings['interval'], copies=counts)


def run(pool, recipient, service_origin, pin, receive, *, force=False, clock=time.time):
    """Trusted receive(destination, accept_receipt, policy) uses receipt-last reception."""
    require(type(force) is bool and callable(receive), 'Invalid receiver worker')
    now = int(clock())
    require(0 <= now < 10**12, 'Invalid receiver clock')
    with opened(pool, recipient, service_origin, pin) as (pool, settings, db):
        require(now >= state(db, settings)['last_attempt'], 'Receiver clock moved backwards')
        recover(pool, settings, db, recipient, service_origin, pin, now)
        retain(pool, settings, db, recipient, service_origin, pin)
        meta = state(db, settings)
        rows = copies(db, settings, recipient, service_origin, pin)
        total = totals(pool, rows, recipient, service_origin, pin)
        require(total <= settings['maximum_bytes'], 'Receiver storage budget exceeded')
        if not force and ((meta['last_success'] >= 0 and now - meta['last_success'] < settings['interval'])
                          or (meta['last_attempt'] > meta['last_success'] and now - meta['last_attempt'] < 3600)):
            return dict(report(db, settings, now), status='not-due')
        require(len(rows) < MAX_COPIES, 'Receiver partial copies require operator inspection')
        require(shutil.disk_usage(pool).free >= RESERVE + cipher.MIN_FREE, 'Insufficient receiver journal headroom')
        name = 'copy-' + settings['pool_id'] + '-' + uuid.uuid4().hex
        with transaction(db):
            db.execute("INSERT INTO copies VALUES(?,'receiving',NULL,NULL)", (name,))
            db.execute('UPDATE meta SET last_attempt=?', (now,))

        def accept(value):
            exports.checked_receipt(encode(value), recipient, service_origin, pin)
            for row in rows:
                if row['state'] == 'complete' and row['value']['export_id'] == value['export_id']:
                    require(row['value'] == value, 'Known receiver export identity changed')
                    raise AlreadyHeld('Receiver already holds this export')
            require(meta['highwater'] < value['created'] <= now + 300, 'Receiver export timestamp would roll back or is in the future')
            require(total + value['cipher_bytes'] <= settings['maximum_bytes'], 'Receiver capacity exhausted; previous copies preserved')
            require(shutil.disk_usage(pool).free >= value['cipher_bytes'] + RESERVE + cipher.MIN_FREE,
                    'Insufficient managed receiver headroom')
            with transaction(db):
                updated = db.execute('UPDATE copies SET receipt=? WHERE name=? AND receipt IS NULL',
                                     (encode(value).decode('ascii'), name))
                require(updated.rowcount == 1, 'Receiver attempt receipt is already bound')

        try:
            receipt = receive(pool / name, accept, settings)
            row = dict(db.execute('SELECT * FROM copies WHERE name=?', (name,)).fetchone())
            require(row['receipt'] == encode(receipt).decode('ascii'), 'Receiver completion was not admitted')
            inspect(pool, row, recipient, service_origin, pin)
            require(read_private(pool / POLICY, MAX_POLICY) == encode(settings), 'Receiver policy changed before admission')
            admit(db, settings, name, receipt, now)
        except AlreadyHeld:
            require(not (pool / name).exists() and not (pool / name).is_symlink(), 'Preserving unexpected duplicate reception')
            with transaction(db):
                db.execute('DELETE FROM copies WHERE name=?', (name,))
                db.execute("UPDATE meta SET last_result='source-unchanged'")
        except Exception:
            with transaction(db):
                if (pool / name).exists() or (pool / name).is_symlink():
                    db.execute("UPDATE copies SET state='failed' WHERE name=?", (name,))
                else:
                    db.execute('DELETE FROM copies WHERE name=?', (name,))
                db.execute("UPDATE meta SET last_result='failed'")
            raise
        retain(pool, settings, db, recipient, service_origin, pin)
        return report(db, settings, now)


def run_ssh(pool, recipient, service_origin, pin, *, force=False, seconds=transport.MAX_SECONDS):
    def receive(destination, accept, settings):
        remote = settings['remote']  # Use only the policy admitted under the journal lease.
        return transport.receive_ssh(*(remote[key] for key in ('host', 'python', 'entry', 'bundle', 'record')),
                                     destination, recipient, service_origin, pin, seconds=seconds, accept_receipt=accept)
    return run(pool, recipient, service_origin, pin, receive, force=force)
