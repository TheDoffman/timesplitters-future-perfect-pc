"""Immutable off-device receiver binding and bounded durable worker health.

The control directory is separate from the receiver pool's strict inventory.
No secrets, remote stderr, arbitrary commands or notifications are stored here.
The caller authenticates the binding hash before using any path from it.
"""
from itertools import islice
from pathlib import Path
import time
import uuid

import mod_backup_receiver as receiver
from mod_package import read_json, require
from mod_trust import encode

BINDING = 'worker.json'
DATABASE = 'worker.sqlite'
TYPE = 'tsfp-backup-receiver-worker-v1'
MAX_BINDING = 65536
SCHEMA = {'meta': receiver.SCHEMA['meta']}


def check(value):
    require(isinstance(value, dict) and set(value) == {'schema', 'type', 'job_id', 'pool', 'pool_id',
            'policy_sha256', 'recipient', 'origin', 'trusted_root_sha256', 'seconds'}
            and type(value['schema']) is int and value['schema'] == 1 and value['type'] == TYPE,
            'Unknown backup worker binding')
    require(isinstance(value['job_id'], str) and receiver.re.fullmatch('[0-9a-f]{32}', value['job_id'])
            and isinstance(value['pool_id'], str) and receiver.re.fullmatch('[0-9a-f]{32}', value['pool_id']),
            'Unknown backup worker identity')
    require(isinstance(value['pool'], str) and str(Path(value['pool']).absolute()) == value['pool']
            and type(value['seconds']) is int and 1 <= value['seconds'] <= receiver.transport.MAX_SECONDS,
            'Invalid backup worker path or deadline')
    receiver.cipher.checked_sha(value['policy_sha256'])
    receiver.exports.recipients.checked_public(value['recipient'], value['origin'], value['trusted_root_sha256'])
    return value


def initialize(control, pool, recipient, service_origin, pin, *, seconds=receiver.transport.MAX_SECONDS):
    control, pool = Path(control).absolute(), Path(pool).absolute()
    require(control != pool and control not in pool.parents and pool not in control.parents,
            'Worker control and receiver storage must be separate')
    # This acquires the real pool lease and validates current contents. Binding
    # creation never captures data or changes retention/receiver attempt times.
    with receiver.opened(pool, recipient, service_origin, pin) as (_, settings, db):
        receiver.totals(pool, receiver.copies(db, settings, recipient, service_origin, pin), recipient, service_origin, pin)
        value = check(dict(schema=1, type=TYPE, job_id=uuid.uuid4().hex, pool=str(pool), pool_id=settings['pool_id'],
                           policy_sha256=receiver.cipher.digest(encode(settings)), recipient=recipient,
                           origin=service_origin, trusted_root_sha256=pin, seconds=seconds))
    wire = encode(value)
    require(len(wire) <= MAX_BINDING, 'Worker binding exceeds budget')
    receiver.cipher.private_path(control)
    control = receiver.exports.new_directory(control)
    receiver.create_database(control / DATABASE, SCHEMA, value['job_id'])
    with receiver.cipher.outgoing(control / BINDING, len(wire)) as output:
        output.write(wire)  # Admission marker last; never adopt partial control.
    receiver.exports.sync(control)
    receiver.exports.sync(control.parent)
    return dict(job_id=value['job_id'], binding_sha256=receiver.cipher.digest(wire))


def binding(control, expected_sha256):
    receiver.cipher.checked_sha(expected_sha256)
    control = receiver.cipher.private_path(Path(control) / BINDING).parent
    wire = receiver.read_private(control / BINDING, MAX_BINDING)
    require(receiver.cipher.digest(wire) == expected_sha256, 'Worker binding differs from the scheduled authority')
    value = check(read_json(wire))
    require(encode(value) == wire, 'Noncanonical worker binding')
    require(control != Path(value['pool']) and control not in Path(value['pool']).parents
            and Path(value['pool']) not in control.parents, 'Worker storage overlaps the receiver')
    require(set(p.name for p in islice(control.iterdir(), 4)) in ({BINDING, DATABASE}, {BINDING, DATABASE, DATABASE + '-journal'}),
            'Preserving foreign worker control entries')
    if (control / (DATABASE + '-journal')).exists():
        receiver.cipher.regular(control / (DATABASE + '-journal'), 2 * receiver.MAX_DB)
    return control, value


def state(db, value):
    rows = db.execute('SELECT * FROM meta LIMIT 2').fetchall()
    require(len(rows) == 1, 'Invalid worker health inventory')
    result = dict(rows[0])
    require(result['singleton'] == 1 and result['pool_id'] == value['job_id'] and result['highwater'] == -1
            and all(type(result[key]) is int and -1 <= result[key] < 10**12 for key in ('last_attempt', 'last_success'))
            and result['last_success'] <= result['last_attempt']
            and result['last_result'] in ('initialized', 'running', 'ok', 'stale', 'failed'), 'Invalid worker health identity')
    return result


def health(control, expected_sha256, *, clock=time.time):
    control, value = binding(control, expected_sha256)
    now = int(clock())
    with receiver.database(control / DATABASE, SCHEMA) as db:
        result = state(db, value)
    require(0 <= now < 10**12 and now >= result['last_attempt'], 'Worker health clock moved backwards')
    return dict(schema=1, phase='backup-receiver-worker', job_id=value['job_id'], status=result['last_result'],
                last_attempt=result['last_attempt'], last_success=result['last_success'],
                missed_run=result['last_attempt'] < 0 or now - result['last_attempt'] > 7200,
                interrupted=result['last_result'] == 'running')


def tick(control, expected_sha256, *, clock=time.time, receive=receiver.run_ssh):
    control, value = binding(control, expected_sha256)
    now = int(clock())
    require(0 <= now < 10**12, 'Invalid worker clock')
    # An exclusive control lease spans network work as well as journal commits.
    # A second invocation cannot mark the first as failed or start another copy.
    with receiver.database(control / DATABASE, SCHEMA) as db:
        previous = state(db, value)
        require(now >= previous['last_attempt'], 'Worker clock moved backwards')
        with receiver.transaction(db):
            db.execute("UPDATE meta SET last_attempt=?,last_result='running'", (now,))
        try:
            # Pool identity and policy are checked under the receiver lease,
            # before recovery, retention, attempt creation or network access.
            result = receive(value['pool'], value['recipient'], value['origin'], value['trusted_root_sha256'],
                             seconds=value['seconds'], policy_sha256=value['policy_sha256'])
            require(isinstance(result, dict) and type(result.get('stale_export')) is bool,
                    'Receiver did not return a verified health result')
            code = 3 if result['stale_export'] else 0
            with receiver.transaction(db):
                db.execute('UPDATE meta SET last_result=?,last_success=?',
                           ('stale' if code else 'ok', previous['last_success'] if code else now))
        except Exception:
            with receiver.transaction(db):
                db.execute("UPDATE meta SET last_result='failed'")
            raise
    return code
