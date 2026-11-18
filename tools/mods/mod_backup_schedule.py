"""New-only, account-scoped Windows receiver task with pinned action authority."""
from datetime import datetime, timedelta, timezone
import os
from pathlib import Path
import subprocess
import time

import mod_backup_runtime as runtime
import mod_backup_worker as worker
from mod_package import read_json, require
from mod_trust import encode

SPEC = 'schedule.json'
TYPE = 'tsfp-backup-schedule-v1'
SCRIPT = Path(__file__).parent / 'hosting/schedule-backup-receiver-windows.ps1'


def powershell(operation, spec=None, fingerprint=None):
    require(os.name == 'nt' and operation in ('Identity', 'Install', 'Run', 'Status', 'Remove'), 'Unknown Windows scheduler operation')
    executable = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    args = [str(executable), '-NoProfile', '-NonInteractive', '-File', str(SCRIPT), '-Operation', operation]
    if spec is not None:
        args += ['-Spec', str(spec), '-Sha256', fingerprint]
    result = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, timeout=30,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    require(result.returncode == 0 and len(result.stdout) <= 8192 and len(result.stderr) <= 8192,
            'Windows scheduler refused the operation; preserve existing task/state')
    value = read_json(result.stdout)
    require(isinstance(value, dict) and value.get('schema') == 1, 'Unknown scheduler result')
    return value


def check(value):
    require(isinstance(value, dict) and set(value) == {'schema', 'type', 'job_id', 'sid', 'runtime', 'runtime_sha256',
            'control', 'binding_sha256', 'executable', 'arguments', 'start_boundary'}
            and type(value['schema']) is int and value['schema'] == 1 and value['type'] == TYPE,
            'Unknown backup schedule')
    require(isinstance(value['job_id'], str) and worker.receiver.re.fullmatch('[0-9a-f]{32}', value['job_id'])
            and isinstance(value['sid'], str) and worker.receiver.re.fullmatch('S-1-5-21-(?:[0-9]+-){3}[0-9]+', value['sid']),
            'Unknown schedule account/job identity')
    worker.receiver.cipher.checked_sha(value['runtime_sha256'])
    worker.receiver.cipher.checked_sha(value['binding_sha256'])
    for name in ('runtime', 'control', 'executable'):
        require(isinstance(value[name], str) and str(Path(value[name]).absolute()) == value[name], 'Schedule paths must be canonical absolute paths')
    root = Path(value['runtime'])
    require(value['executable'] == str(root / 'runtime/pythonw.exe'), 'Schedule executable differs from the frozen worker')
    args = ['-I', '-S', '-B', str(root / 'bootstrap.py'), '--runtime-sha256', value['runtime_sha256'],
            '--control', value['control'], '--binding-sha256', value['binding_sha256']]
    require(value['arguments'] == subprocess.list2cmdline(args), 'Schedule worker arguments differ')
    require(isinstance(value['start_boundary'], str) and worker.receiver.re.fullmatch(
        r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\+00:00', value['start_boundary']), 'Schedule time must have an explicit UTC offset')
    datetime.fromisoformat(value['start_boundary'])
    return value


def ready(value):
    root = Path(value['runtime'])
    runtime.bootstrap().verify(root, value['runtime_sha256'])
    from mod_windows_recovery import private
    private(root, tree=True)
    checked = subprocess.run([str(root / 'runtime/python.exe'), '-I', '-S', '-B', str(root / 'bootstrap.py'),
                              '--runtime-sha256', value['runtime_sha256'], '--check-runtime'],
                             stdin=subprocess.DEVNULL, capture_output=True, timeout=60,
                             creationflags=subprocess.CREATE_NO_WINDOW)
    require(checked.returncode == 0 and len(checked.stdout) <= 4096 and len(checked.stderr) <= 8192,
            'Independent frozen runtime admission failed')
    confirmation = read_json(checked.stdout)
    require(confirmation.get('phase') == 'backup-runtime-verified' and confirmation.get('isolated') is True,
            'Unknown frozen runtime admission result')
    _, bound = worker.binding(value['control'], value['binding_sha256'])
    require(bound['job_id'] == value['job_id'], 'Schedule worker identity changed')
    state = worker.health(value['control'], value['binding_sha256'])
    require(state['status'] == 'ok' and not state['missed_run'] and not state['interrupted']
            and 0 <= int(time.time()) - state['last_success'] <= 7200, 'Verify a current worker receive before registration/start')
    with worker.receiver.opened(bound['pool'], bound['recipient'], bound['origin'], bound['trusted_root_sha256'],
                                policy_sha256=bound['policy_sha256']) as (pool, settings, db):
        worker.receiver.totals(pool, worker.receiver.copies(db, settings, bound['recipient'], bound['origin'], bound['trusted_root_sha256']),
                               bound['recipient'], bound['origin'], bound['trusted_root_sha256'])
        require(not worker.receiver.report(db, settings, int(time.time()))['stale_export'], 'Receiver backup is stale')


def prepare(destination, frozen, runtime_sha256, control, binding_sha256):
    require(os.name == 'nt', 'Prepare receiver schedules on Windows')
    frozen, control, destination = (Path(p).absolute() for p in (frozen, control, destination))
    _, bound = worker.binding(control, binding_sha256)
    require(all(destination != p and destination not in p.parents and p not in destination.parents
                for p in (frozen, control, Path(bound['pool']))), 'Schedule metadata must be separate from runtime/worker/receiver storage')
    sid = powershell('Identity')['sid']
    arguments = subprocess.list2cmdline(['-I', '-S', '-B', str(frozen / 'bootstrap.py'), '--runtime-sha256', runtime_sha256,
                                        '--control', str(control), '--binding-sha256', binding_sha256])
    value = check(dict(schema=1, type=TYPE, job_id=bound['job_id'], sid=sid, runtime=str(frozen),
                       runtime_sha256=runtime_sha256, control=str(control), binding_sha256=binding_sha256,
                       executable=str(frozen / 'runtime/pythonw.exe'), arguments=arguments,
                       start_boundary=(datetime.now(timezone.utc) + timedelta(hours=1)).replace(microsecond=0).isoformat()))
    ready(value)
    destination = worker.receiver.exports.new_directory(destination)
    wire = encode(value)
    require(len(wire) <= 65536, 'Schedule metadata exceeds budget')
    with worker.receiver.cipher.outgoing(destination / SPEC, len(wire)) as stream:
        stream.write(wire)
    worker.receiver.exports.sync(destination)
    return dict(schema=1, phase='backup-schedule-prepared', job_id=value['job_id'],
                schedule_sha256=worker.receiver.cipher.digest(wire), services_started=False)


def manage(operation, directory, expected_sha256):
    require(operation in ('Install', 'Run', 'Status', 'Remove'), 'Unknown backup schedule operation')
    worker.receiver.cipher.checked_sha(expected_sha256)
    directory = worker.receiver.cipher.private_path(Path(directory) / SPEC).parent
    require(set(p.name for p in worker.islice(directory.iterdir(), 2)) == {SPEC}, 'Preserving foreign schedule metadata')
    wire = worker.receiver.read_private(directory / SPEC, 65536)
    require(worker.receiver.cipher.digest(wire) == expected_sha256, 'Schedule fingerprint differs')
    value = check(read_json(wire))
    require(encode(value) == wire, 'Noncanonical schedule metadata')
    if operation in ('Install', 'Run'):
        ready(value)
    # Status/removal remain available when the runtime is damaged, using this
    # trusted operator helper and strict task/action/principal/security identity.
    result = powershell(operation, directory / SPEC, expected_sha256)
    require(result.get('job_id') == value['job_id'], 'Scheduler result identity differs')
    return result
