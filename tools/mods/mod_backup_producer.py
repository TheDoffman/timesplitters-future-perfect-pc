"""POSIX encrypted exports from the newest verified managed snapshot.

The plaintext capture job remains separate. Read-only senders use only an atomic
public export pointer and ciphertext, never the source pool or private journal.
"""
from contextlib import contextmanager
from itertools import islice
import os
from pathlib import Path
import re
import shutil
import time
import uuid

import mod_backup as backup
import mod_backup_archive as archive
import mod_backup_cipher as cipher
import mod_backup_exports as exports
import mod_backup_pool as snapshots
import mod_backup_receiver as journal
from mod_package import SHA, ModError, read_json, require
from mod_trust import encode

POLICY = 'producer.json'
DATABASE = 'producer.sqlite'
LOCK = '.exports.lock'
CURRENT = 'current.json'
TYPE = 'tsfp-backup-producer-v1'
POINTER = 'tsfp-backup-export-current-v1'
MAX_CURRENT = 4096
SCHEMA = dict(journal.SCHEMA)
SCHEMA['meta'] = SCHEMA['meta'][:-1] + ', publication TEXT)'
SCHEMA['plans'] = 'CREATE TABLE plans (name TEXT PRIMARY KEY REFERENCES copies(name) ON DELETE CASCADE, source_name TEXT NOT NULL, source_hash TEXT NOT NULL, source_created INTEGER NOT NULL)'


def check_policy(value, recipient, service_origin, pin):
    exports.recipients.checked_public(recipient, service_origin, pin)
    require(isinstance(value, dict) and set(value) == {'schema', 'type', 'pool_id', 'recipient', 'source_pool',
            'source_pool_id', 'source_policy_sha256', 'keep', 'interval', 'maximum_bytes'}
            and type(value['schema']) is int and value['schema'] == 1 and value['type'] == TYPE
            and value['recipient'] == recipient, 'Producer authority differs')
    require(all(isinstance(value[key], str) and re.fullmatch('[0-9a-f]{32}', value[key]) for key in ('pool_id', 'source_pool_id'))
            and isinstance(value['source_policy_sha256'], str) and SHA.fullmatch(value['source_policy_sha256']), 'Invalid producer identity')
    journal.transport.remote_path(value['source_pool'])  # Syntax only: senders cannot access plaintext paths.
    require(type(value['keep']) is int and 1 <= value['keep'] <= 32
            and type(value['interval']) is int and 3600 <= value['interval'] <= 604800
            and type(value['maximum_bytes']) is int and 0 < value['maximum_bytes'] <= 32 * cipher.MAX_WIRE,
            'Invalid producer limits')


def policy(pool, recipient, service_origin, pin):
    pool = cipher.private_path(Path(pool) / POLICY).parent
    wire = journal.read_private(pool / POLICY, journal.MAX_POLICY)
    value = read_json(wire)
    require(encode(value) == wire, 'Noncanonical producer policy')
    check_policy(value, recipient, service_origin, pin)
    return pool, value, wire


def source_policy(settings, pool):
    source, value = snapshots.policy(settings['source_pool'])
    require(str(source) == settings['source_pool'] and value['pool_id'] == settings['source_pool_id']
            and cipher.digest(journal.read_private(source / snapshots.POLICY, 8192)) == settings['source_policy_sha256'],
            'Managed snapshot source binding changed')
    for target in (source, Path(value['repository'])):
        require(not pool.is_relative_to(target) and not target.is_relative_to(pool), 'Separate producer, source pool and repository')
    return source, value


def initialize(source, pool, recipient, service_origin, pin, *, keep=1, maximum_bytes=cipher.MAX_WIRE):
    require(os.name == 'posix', 'Run the export producer on a POSIX host')
    source, value = snapshots.policy(source)
    settings = dict(schema=1, type=TYPE, pool_id=uuid.uuid4().hex, recipient=recipient,
                    source_pool=str(source), source_pool_id=value['pool_id'],
                    source_policy_sha256=cipher.digest(journal.read_private(source / snapshots.POLICY, 8192)),
                    keep=keep, interval=value['interval'], maximum_bytes=maximum_bytes)
    check_policy(settings, recipient, service_origin, pin)
    require(len(encode(settings)) <= journal.MAX_POLICY, 'Producer policy exceeds budget')
    pool = cipher.private_path(pool)
    source_policy(settings, pool)
    require(shutil.disk_usage(pool.parent).free >= cipher.MIN_FREE + journal.RESERVE, 'Insufficient producer journal headroom')
    pool = exports.new_directory(pool)
    with cipher.outgoing(pool / POLICY, len(encode(settings))) as output:
        output.write(encode(settings))
    fd = os.open(pool / LOCK, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.fsync(fd)
    os.close(fd)
    journal.create_database(pool / DATABASE, SCHEMA, settings['pool_id'])
    exports.sync(pool)
    exports.sync(pool.parent)
    return settings


@contextmanager
def lease(pool):
    require(os.name == 'posix', 'Run the export producer/sender on a POSIX host')
    import fcntl
    with cipher.incoming(pool / LOCK, 0) as (stream, size, stable):
        try:
            fcntl.flock(stream.fileno(), fcntl.LOCK_SH | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ModError('Producer export publication is busy') from error
        class Access:
            def exclusive(self):
                try:
                    fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                    return True
                except BlockingIOError:
                    return False  # No mutation after a failed upgrade.

            def shared(self):
                fcntl.flock(stream.fileno(), fcntl.LOCK_SH | fcntl.LOCK_NB)
        yield Access()
        stable()


def checked_pointer(value, settings):
    require(isinstance(value, dict) and set(value) == {'schema', 'type', 'pool_id', 'name', 'created', 'receipt_sha256'}
            and type(value['schema']) is int and value['schema'] == 1 and value['type'] == POINTER
            and value['pool_id'] == settings['pool_id'] and journal.copy_name(settings, value['name'])
            and type(value['created']) is int and 0 <= value['created'] < 10**12
            and isinstance(value['receipt_sha256'], str) and SHA.fullmatch(value['receipt_sha256']), 'Invalid current export pointer')
    return value


def pointer(pool, settings):
    if not (pool / CURRENT).exists() and not (pool / CURRENT).is_symlink():
        return None
    wire = journal.read_private(pool / CURRENT, MAX_CURRENT)
    value = checked_pointer(read_json(wire), settings)
    require(encode(value) == wire, 'Noncanonical current export pointer')
    return value


def publication(db, settings):
    raw = journal.state(db, settings)['publication']
    if raw is None:
        return None
    require(isinstance(raw, str) and len(raw) <= 3 * MAX_CURRENT, 'Publication intent exceeds budget')
    value = read_json(raw)
    require(isinstance(value, dict) and set(value) == {'temp', 'previous', 'next'}
            and isinstance(value['temp'], str) and re.fullmatch(r'\.current-[0-9a-f]{32}\.tmp', value['temp'])
            and encode(value).decode('ascii') == raw, 'Invalid publication intent')
    checked_pointer(value['next'], settings)
    if value['previous'] is not None:
        checked_pointer(value['previous'], settings)
        require(value['previous']['created'] <= value['next']['created'], 'Publication pointer would roll back')
    return value


def namespace(pool, settings):
    with os.scandir(pool) as stream:
        names = [entry.name for entry in islice(stream, journal.MAX_COPIES + 7)]
    require(len(names) <= journal.MAX_COPIES + 6 and {POLICY, DATABASE, LOCK} <= set(names), 'Producer directory budget or inventory differs')
    for name in names:
        require(name in (POLICY, DATABASE, DATABASE + '-journal', LOCK, CURRENT)
                or journal.copy_name(settings, name) or re.fullmatch(r'\.current-[0-9a-f]{32}\.tmp', name), 'Preserving foreign producer entry')
    if DATABASE + '-journal' in names:
        cipher.regular(pool / (DATABASE + '-journal'), 2 * journal.MAX_DB)
    return set(names) - {POLICY, DATABASE, DATABASE + '-journal', LOCK, CURRENT}


@contextmanager
def opened(pool, recipient, service_origin, pin):
    pool, settings, wire = policy(pool, recipient, service_origin, pin)
    with lease(pool) as exclusive:
        namespace(pool, settings)
        with journal.database(pool / DATABASE, SCHEMA) as db:
            rows = journal.copies(db, settings, recipient, service_origin, pin)
            plans = [dict(row) for row in db.execute('SELECT * FROM plans LIMIT ?', (journal.MAX_COPIES + 1,))]
            require(len(plans) == len(rows) and {plan['name'] for plan in plans} == {row['name'] for row in rows}, 'Producer plan inventory differs')
            for plan in plans:
                require(isinstance(plan['source_name'], str) and re.fullmatch('snapshot-' + settings['source_pool_id'] + r'-[0-9]{10,12}-[0-9a-f]{8}', plan['source_name'])
                        and isinstance(plan['source_hash'], str) and SHA.fullmatch(plan['source_hash'])
                        and type(plan['source_created']) is int and 0 <= plan['source_created'] < 10**12, 'Invalid producer source plan')
            pending = publication(db, settings)
            expected = {row['name'] for row in rows if (pool / row['name']).exists() or (pool / row['name']).is_symlink()}
            if pending is not None and ((pool / pending['temp']).exists() or (pool / pending['temp']).is_symlink()):
                expected.add(pending['temp'])
                cipher.regular(pool / pending['temp'], MAX_CURRENT)
            require(namespace(pool, settings) == expected, 'Preserving unrecorded producer output')
            current = pointer(pool, settings)
            if current is not None:
                require(any(row['state'] == 'complete' and row['name'] == current['name']
                            and row['value']['created'] == current['created']
                            and cipher.digest(row['receipt'].encode('ascii')) == current['receipt_sha256'] for row in rows),
                        'Current export is not an admitted producer copy')
            yield pool, settings, db, exclusive
            require(journal.read_private(pool / POLICY, journal.MAX_POLICY) == wire, 'Producer policy changed during operation')


def recover(pool, settings, db, recipient, service_origin, pin, now):
    for row in journal.copies(db, settings, recipient, service_origin, pin):
        if row['state'] != 'receiving':
            continue
        folder = pool / row['name']
        if not folder.exists() and not folder.is_symlink():
            with journal.transaction(db):
                db.execute('DELETE FROM copies WHERE name=?', (row['name'],))
        elif (folder / exports.RECEIPT).exists():
            value = exports.inspect_export(folder, recipient, service_origin, pin)[2]
            plan = db.execute('SELECT * FROM plans WHERE name=?', (row['name'],)).fetchone()
            require(value['created'] == plan['source_created'] and value['created'] <= now, 'Producer completion differs from its source plan')
            if row['receipt'] is not None:
                require(row['receipt'] == encode(value).decode('ascii'), 'Producer completion receipt changed')
            with journal.transaction(db):
                db.execute('UPDATE copies SET receipt=? WHERE name=?', (encode(value).decode('ascii'), row['name']))
            journal.admit(db, settings, row['name'], value, now)
        else:
            with journal.transaction(db):
                db.execute("UPDATE copies SET state='failed' WHERE name=?", (row['name'],))


def publish(pool, settings, db, recipient, service_origin, pin):
    rows = journal.copies(db, settings, recipient, service_origin, pin)
    complete = sorted((row for row in rows if row['state'] == 'complete'), key=lambda row: row['value']['created'])
    highwater = journal.state(db, settings)['highwater']
    require((not complete and highwater == -1) or (complete and complete[-1]['value']['created'] == highwater),
            'Producer high-water copy differs')
    if not complete:
        require(journal.state(db, settings)['highwater'] == -1 and publication(db, settings) is None,
                'Producer high-water copy is missing')
        return
    row = complete[-1]
    require(row['value']['created'] == journal.state(db, settings)['highwater'], 'Producer high-water copy differs')
    journal.inspect(pool, row, recipient, service_origin, pin)
    desired = dict(schema=1, type=POINTER, pool_id=settings['pool_id'], name=row['name'],
                   created=row['value']['created'], receipt_sha256=cipher.digest(row['receipt'].encode('ascii')))
    pending = publication(db, settings)
    current = pointer(pool, settings)
    if pending is None:
        if current == desired:
            return
        require(current is None or current['created'] <= desired['created'], 'Current export would roll back')
        pending = dict(temp='.current-' + uuid.uuid4().hex + '.tmp', previous=current, next=desired)
        with journal.transaction(db):
            db.execute('UPDATE meta SET publication=?', (encode(pending).decode('ascii'),))
    require(pending['next'] == desired and current in (pending['previous'], desired), 'Preserving changed current export pointer')
    temporary = pool / pending['temp']
    if temporary.exists() or temporary.is_symlink():
        # This reconstructable metadata file is owned by the durable publication
        # intent, unlike a partial encrypted backup (which is always preserved).
        cipher.regular(temporary, MAX_CURRENT)
        temporary.unlink()
    if current != desired:
        with cipher.outgoing(temporary, len(encode(desired))) as output:
            output.write(encode(desired))
        require(pointer(pool, settings) == pending['previous'], 'Current export changed before publication')
        os.replace(temporary, pool / CURRENT)
        exports.sync(pool)
    with journal.transaction(db):
        db.execute('UPDATE meta SET publication=NULL')


def settle(pool, settings, db, exclusive, recipient, service_origin, pin):
    rows = journal.copies(db, settings, recipient, service_origin, pin)
    complete = sorted((row for row in rows if row['state'] == 'complete'), key=lambda row: row['value']['created'])
    highwater = journal.state(db, settings)['highwater']
    require((not complete and highwater == -1) or (complete and complete[-1]['value']['created'] == highwater),
            'Producer high-water copy differs')
    current = pointer(pool, settings)
    already_current = ((not complete and current is None) or
                       (complete and current is not None and current['name'] == complete[-1]['name']
                        and current['created'] == complete[-1]['value']['created']
                        and current['receipt_sha256'] == cipher.digest(complete[-1]['receipt'].encode('ascii'))))
    if (already_current and publication(db, settings) is None and len(complete) <= settings['keep']
            and not any(row['state'] == 'deleting' for row in rows)):
        return True
    if not exclusive.exclusive():
        return False
    publish(pool, settings, db, recipient, service_origin, pin)
    journal.retain(pool, settings, db, recipient, service_origin, pin)
    return True


def report(pool, settings, db, now, *, deferred=False):
    result = journal.report(db, settings, now)
    published = pointer(pool, settings)
    created = published['created'] if published is not None else -1
    age = now - created if created >= 0 else None
    return dict(result, phase='backup-export-producer', status='publication-pending' if deferred else
                ('exported' if result['status'] == 'received' else result['status']),
                published_created=created, export_age_seconds=age,
                stale_export=age is None or age > 2 * settings['interval'])


def run(pool, recipient, service_origin, pin, *, clock=time.time):
    now = int(clock())
    require(0 <= now < 10**12, 'Invalid producer clock')
    with opened(pool, recipient, service_origin, pin) as (pool, settings, db, exclusive):
        require(now >= journal.state(db, settings)['last_attempt'], 'Producer clock moved backwards')
        recover(pool, settings, db, recipient, service_origin, pin, now)
        if not settle(pool, settings, db, exclusive, recipient, service_origin, pin):
            return report(pool, settings, db, now, deferred=True)
        exclusive.shared()  # Existing published exports can be read during encryption.
        source, config = source_policy(settings, pool)
        with snapshots.locked(source):
            current, _ = snapshots.entries(source, config)
            require(current, 'No complete managed snapshot is available')
            selected = current[-1]
            proof = backup.verify_snapshot(selected['path'])
            rows = journal.copies(db, settings, recipient, service_origin, pin)
            total = journal.totals(pool, rows, recipient, service_origin, pin)
            latest = max((row for row in rows if row['state'] == 'complete'), key=lambda row: row['value']['created'], default=None)
            if latest is not None:
                previous = db.execute('SELECT source_hash FROM plans WHERE name=?', (latest['name'],)).fetchone()[0]
                if proof['manifest_sha256'] == previous:
                    with journal.transaction(db):
                        db.execute("UPDATE meta SET last_attempt=?,last_result='source-unchanged'", (now,))
                    return report(pool, settings, db, now)
            require(journal.state(db, settings)['highwater'] < proof['created'] <= now,
                    'Managed snapshot timestamp would roll back or is in the future')
            require(len(rows) < journal.MAX_COPIES, 'Producer partial exports require operator inspection')
            _, manifest, _ = backup.backup_manifest(selected['path'])
            prefix, payload_bytes = archive.header(manifest)
            plain = len(prefix) + payload_bytes
            require(plain <= cipher.MAX_PLAIN, 'Producer archive exceeds encryption budget')
            estimate = 12 + cipher.MAX_HEADER + plain + 16 * ((plain + cipher.CHUNK - 1) // cipher.CHUNK + 1) + 32
            require(total + estimate <= settings['maximum_bytes'], 'Producer capacity exhausted; previous exports preserved')
            require(shutil.disk_usage(pool).free >= estimate + journal.RESERVE + cipher.MIN_FREE, 'Insufficient producer export headroom')
            name = 'copy-' + settings['pool_id'] + '-' + uuid.uuid4().hex
            with journal.transaction(db):
                db.execute("INSERT INTO copies VALUES(?,'receiving',NULL,NULL)", (name,))
                db.execute('INSERT INTO plans VALUES(?,?,?,?)', (name, selected['path'].name, proof['manifest_sha256'], proof['created']))
                db.execute('UPDATE meta SET last_attempt=?', (now,))
            try:
                value = exports.export_snapshot(selected['path'], pool / name, recipient, service_origin, pin, clock=lambda: proof['created'])
                require(journal.read_private(pool / POLICY, journal.MAX_POLICY) == encode(settings), 'Producer policy changed before admission')
                with journal.transaction(db):
                    db.execute('UPDATE copies SET receipt=? WHERE name=?', (encode(value).decode('ascii'), name))
                journal.admit(db, settings, name, value, now)
            except Exception:
                with journal.transaction(db):
                    if (pool / name).exists() or (pool / name).is_symlink():
                        db.execute("UPDATE copies SET state='failed' WHERE name=?", (name,))
                    else:
                        db.execute('DELETE FROM copies WHERE name=?', (name,))
                    db.execute("UPDATE meta SET last_result='failed'")
                raise
        deferred = not settle(pool, settings, db, exclusive, recipient, service_origin, pin)
        return report(pool, settings, db, now, deferred=deferred)


def send_current(pool, output, recipient, service_origin, pin):
    pool, settings, _ = policy(pool, recipient, service_origin, pin)
    with lease(pool):
        current = pointer(pool, settings)
        require(current is not None, 'No published encrypted export exists')
        folder, wire, value = exports.inspect_export(pool / current['name'], recipient, service_origin, pin)
        require(cipher.digest(wire) == current['receipt_sha256'] and value['created'] == current['created'], 'Published export differs from pointer')
        return exports.send_stream(folder, output, recipient, service_origin, pin)
