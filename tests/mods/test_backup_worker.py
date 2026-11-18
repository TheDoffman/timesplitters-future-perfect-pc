"""Bound worker authority, overlap exclusion and durable crash/failure health."""
from pathlib import Path
import sqlite3
import subprocess
import sys
import uuid

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_backup_worker as worker
import mod_backup_receiver as receiver
from mod_package import ModError
from mod_trust import encode
from test_backup_receiver import pool, sender, START
from test_backup_exports import case, ORIGIN, PIN


@pytest.fixture
def bound(case):
    name = uuid.uuid4().hex
    target = pool(case, 'worker-' + name, keep=2)
    control = case.root / ('worker-control-' + name)
    result = worker.initialize(control, target, case.public, ORIGIN, PIN, seconds=30)
    return case, target, control, result['binding_sha256']


def local(case, created):
    def receive(pool, recipient, origin, pin, *, seconds, policy_sha256):
        assert seconds == 30
        return receiver.run(pool, recipient, origin, pin, sender(case, created), clock=lambda: created,
                            policy_sha256=policy_sha256)
    return receive


def test_real_receive_and_duplicate_do_not_disguise_stale_source(bound):
    case, target, control, pin = bound
    assert worker.tick(control, pin, clock=lambda: START, receive=local(case, START)) == 0
    assert worker.health(control, pin, clock=lambda: START)['status'] == 'ok'
    late = START + 3 * 86400
    assert worker.tick(control, pin, clock=lambda: late, receive=local(case, late)) == 0
    # Same source reported unchanged after three days still returns stale3.
    def duplicate(*args, **kwargs):
        return receiver.run(*args[:4], sender(case, late), clock=lambda: late + 3 * 86400,
                            policy_sha256=kwargs['policy_sha256'])
    assert worker.tick(control, pin, clock=lambda: late + 3 * 86400, receive=duplicate) == 3
    state = worker.health(control, pin, clock=lambda: late + 3 * 86400)
    assert state['status'] == 'stale' and state['last_success'] == late
    assert state['missed_run'] is False and state['interrupted'] is False


def test_binding_changed_refused_before_any_receiver_work(bound):
    case, target, control, pin = bound
    path = control / worker.BINDING
    original = path.read_bytes()
    path.write_bytes(original + b' ')
    called = []
    with pytest.raises(ModError, match='binding differs'):
        worker.tick(control, pin, receive=lambda *a, **k: called.append(True))
    assert not called and path.read_bytes() == original + b' '


def test_policy_change_is_refused_before_recovery_or_network(bound):
    case, target, control, pin = bound
    path = target / receiver.POLICY
    original = path.read_bytes()
    value = receiver.read_json(original)
    value['remote']['host'] = 'other-host'
    path.write_bytes(encode(value))
    before = (target / receiver.DATABASE).read_bytes()
    with pytest.raises(ModError, match='scheduled binding'):
        worker.tick(control, pin, clock=lambda: START, receive=local(case, START))
    assert (target / receiver.DATABASE).read_bytes() == before
    assert worker.health(control, pin, clock=lambda: START)['status'] == 'failed'


def test_overlapping_worker_cannot_change_health_or_receiver(bound):
    case, target, control, pin = bound
    def held(*args, **kwargs):
        with pytest.raises(sqlite3.OperationalError):
            worker.tick(control, pin, clock=lambda: START + 1, receive=lambda *a, **k: pytest.fail('Overlapping receive'))
        return dict(stale_export=False)
    assert worker.tick(control, pin, clock=lambda: START, receive=held) == 0
    assert worker.health(control, pin, clock=lambda: START)['last_attempt'] == START


def test_interrupted_health_is_durable_and_next_attempt_can_recover(bound):
    case, target, control, pin = bound
    class Crash(BaseException):
        pass
    def crash(*a, **k):
        raise Crash()
    with pytest.raises(Crash):
        worker.tick(control, pin, clock=lambda: START, receive=crash)
    value = worker.health(control, pin, clock=lambda: START + 7201)
    assert value['interrupted'] is value['missed_run'] is True
    assert value['last_success'] == -1
    assert worker.tick(control, pin, clock=lambda: START + 7202, receive=local(case, START + 7202)) == 0


def test_failure_records_only_status_and_preserves_last_success(bound):
    case, target, control, pin = bound
    assert worker.tick(control, pin, clock=lambda: START, receive=local(case, START)) == 0
    def fail(*a, **k):
        raise OSError('PRIVATE remote stderr /secret/path')
    with pytest.raises(OSError):
        worker.tick(control, pin, clock=lambda: START + 1, receive=fail)
    value = worker.health(control, pin, clock=lambda: START + 1)
    assert value['status'] == 'failed' and value['last_success'] == START
    assert b'PRIVATE' not in (control / worker.DATABASE).read_bytes()
    with pytest.raises(ModError, match='backwards'):
        worker.tick(control, pin, clock=lambda: START, receive=fail)


def test_new_only_separate_control_and_foreign_entries_preserved(bound):
    case, target, control, pin = bound
    with pytest.raises(ModError):
        worker.initialize(control, target, case.public, ORIGIN, PIN)
    with pytest.raises(ModError, match='separate'):
        worker.initialize(target / 'control', target, case.public, ORIGIN, PIN)
    marker = control / 'personal-file'
    marker.write_bytes(b'preserve')
    with pytest.raises(ModError, match='foreign'):
        worker.health(control, pin)
    assert marker.read_bytes() == b'preserve'


def test_actual_process_exit_leaves_detectable_interruption_and_releases_lease(bound):
    case, target, control, pin = bound
    script = '''
import os,sys
sys.path.insert(0,sys.argv[1])
import mod_backup_worker as worker
def crash(*a,**k): os._exit(99)
worker.tick(sys.argv[2],sys.argv[3],clock=lambda:1700000000,receive=crash)
'''
    result = subprocess.run([sys.executable, '-I', '-c', script,
                             str(Path(worker.__file__).parent), str(control), pin],
                            capture_output=True, timeout=30)
    assert result.returncode == 99, result.stderr.decode(errors='replace')
    assert worker.health(control, pin, clock=lambda: START)['interrupted'] is True
    assert worker.tick(control, pin, clock=lambda: START + 1, receive=local(case, START + 1)) == 0
