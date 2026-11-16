"""Cutover observations bind fixed jobs and conservatively prove process exit."""
from pathlib import Path
import os
import plistlib
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
from mod_package import ModError


def generated_path(label):
    role = label.split('.')[3]
    monitor = label.endswith('.monitor')
    if role == 'backup':return cutover.ROOT/'backup-jobs'/('a'*64)/'worker.plist'
    if role == 'scan':return cutover.ROOT/'monitor'/('monitor.plist' if monitor else 'scan.plist')
    if role == 'definitions':return cutover.ROOT/'monitor/definitions.plist'
    return cutover.ROOT/'monitor'/role/('monitor.plist' if monitor else 'worker.plist')


@pytest.fixture
def job():
    label = next(v for v in cutover.jobs() if '.gateway.' in v and not v.endswith('.monitor'))
    generated = generated_path(label)
    specification = dict(Label=label, ProgramArguments=['/sealed runtime/python', '-I', '-S', '-B', '/sealed source/bootstrap.py', '--role', 'gateway'])
    output = ('system/'+label+' = {\n\tpath = '+str(generated)+'\n\ttype = LaunchDaemon\n'
              '\tprogram = /sealed runtime/python\n\targuments = {\n'
              + ''.join('\t\t'+v+'\n' for v in specification['ProgramArguments'])
              + '\t}\n\tpid = 123\n\tresource coalition = {\n\t\tpid = 999\n\t}\n}\n')
    return output, generated, specification


def test_loaded_job_preserves_space_paths_and_ignores_nested_pid(job):
    output, generated, spec = job
    assert cutover.loaded_job(output, generated, spec) == dict(path=str(generated), pid=123)
    boot = str(cutover.LAUNCHD/(spec['Label']+'.plist'))
    assert cutover.loaded_job(output.replace(str(generated), boot), generated, spec)['path'] == boot
    assert cutover.loaded_job(output.replace('\tpid = 123\n', ''), generated, spec)['pid'] is None


@pytest.mark.parametrize('old,new', [
    ('system/', 'gui/501/'), ('\ttype = LaunchDaemon', '\ttype = LaunchAgent'),
    ('\tpath = ', '\tpath = /foreign'), ('\tprogram = ', '\tprogram = /foreign'),
    ('\t\t-I\n', '\t\t-c\n'), ('\tpid = 123\n', '\tpid = 123\n\tpid = 124\n'),
    ('\tpid = 123', '\tpid = 1'), ('\tpid = 123', '\tpid = -123'),
    ('\tpid = 123', '\tpid = 2147483648'),
    ('\targuments = {', '\targuments = broken\n\targuments = {'),
    ('\ttype = LaunchDaemon', '\t\ttype = LaunchDaemon'),
    ('\tprogram = /sealed runtime/python', '\tprogram = /sealed runtime/python\n\tprogram = /sealed runtime/python'),
    ('\n', '\r\n'), ('\t\t--role\n', '\t\t--role\x00\n'),
])
def test_loaded_job_refuses_changed_ambiguous_or_malformed_bindings(job, old, new):
    output, generated, spec = job
    with pytest.raises(ModError):cutover.loaded_job(output.replace(old, new), generated, spec)


def test_loaded_job_rejects_unmanaged_label_and_unbounded_output(job):
    output, generated, spec = job
    with pytest.raises(ModError):cutover.loaded_job(output, generated, dict(spec, Label='other.job'))
    with pytest.raises(ModError):cutover.loaded_job(output+'x'*cutover.MAX_OUTPUT, generated, spec)
    with pytest.raises(ModError):cutover.loaded_job(output, generated, dict(spec, ProgramArguments=['bad\narg']))


def test_process_exit_requires_descendants_groups_and_escaped_service_processes():
    rows = cutover.process_rows('1 0 1 0\n10 1 10 0\n11 10 10 600\n12 11 12 600\n30 1 30 501\n40 1 40 -2\n')
    tracked = cutover.tracked_processes({10}, rows, 600)
    assert tracked == dict(pids=[10,11,12], groups=[10,12])
    gone = {pid:row for pid,row in rows.items() if pid not in tracked['pids']}
    assert cutover.processes_gone(tracked, gone, 600)
    for pid,row in [(11,(1,99,501)), (99,(1,10,501)), (99,(1,99,600))]:
        assert not cutover.processes_gone(tracked, gone | {pid:row}, 600)
    with pytest.raises(ModError):cutover.tracked_processes({999}, rows, 600)
    with pytest.raises(ModError):cutover.tracked_processes({30}, rows, 600)
    with pytest.raises(ModError):cutover.tracked_processes({10}, rows, True)


@pytest.mark.parametrize('output', ['', '1 0 1 root', '1 0 1 0\n1 0 1 0', '0 0 0 0', '1 -1 1 0', '1 0 1 -3', '2147483648 0 1 0', '1 0 1 0 extra'])
def test_process_observation_refuses_unknown_or_duplicate_rows(output):
    with pytest.raises(ModError):cutover.process_rows(output)


def test_descendant_budget_and_unknown_process_group():
    rows = {pid:(pid-1,10,600) for pid in range(10,140)}
    with pytest.raises(ModError):cutover.tracked_processes({10}, rows, 600)
    with pytest.raises(ModError):cutover.tracked_processes({10}, {10:(1,0,600)}, 600)


@pytest.mark.parametrize('change', [None, 'pid', 'arguments', 'unloaded', 'process-failed', 'shared-pid'])
def test_complete_observation_rechecks_each_fixed_job_around_process_sample(job, change):
    template, generated, base = job
    labels = cutover.jobs()
    entries = [(generated_path(label), dict(base, Label=label)) for label in labels]
    calls = []

    def command(arguments):
        calls.append(arguments)
        if arguments[0] == '/bin/ps':
            return (1, '') if change == 'process-failed' else (0, '1 0 1 0\n'+''.join(f'{pid} 1 {pid} 600\n' for pid in range(123,133)))
        label = arguments[-1].removeprefix('system/')
        output = template.replace(base['Label'], label).replace(str(generated), str(generated_path(label)))
        if change != 'shared-pid':output = output.replace('\tpid = 123\n', f'\tpid = {123+labels.index(label)}\n')
        if len(calls) > 11:
            if change == 'pid':output = output.replace('\tpid = 123\n', '\tpid = 999\n')
            if change == 'arguments':output = output.replace('\t\t-I\n', '\t\t-c\n')
            if change == 'unloaded':return 113, 'not loaded'
        return 0, output

    if change:
        with pytest.raises(ModError):cutover.observe(entries, 600, command)
    else:
        result = cutover.observe(entries, 600, command)
        assert result['tracked'] == dict(pids=list(range(123,133)), groups=list(range(123,133)))
        assert sorted(result['jobs']) == labels and len(calls) == 21
    for bad in (entries[:-1], entries+[entries[0]], [entries[0]]*10):
        count = len(calls)
        with pytest.raises(ModError):cutover.observe(bad, 600, command)
        assert len(calls) == count


@pytest.fixture
def protected_jobs(private_posix_path, monkeypatch):
    import mod_host_migration
    root = private_posix_path/'workshop';root.mkdir(mode=0o755)
    boot = private_posix_path/'LaunchDaemons';boot.mkdir(mode=0o755)
    monkeypatch.setattr(cutover, 'ROOT', root)
    monkeypatch.setattr(mod_host_migration, 'ROOT', root)
    monkeypatch.setattr(cutover, 'ROOT_UID', os.geteuid())
    monkeypatch.setattr(cutover, 'LAUNCHD', boot)
    entries = []
    for label in cutover.jobs():
        path = generated_path(label)
        spec = dict(Label=label, ProgramArguments=['/sealed/python', '-I', '-S', '-B', '/sealed/bootstrap.py'])
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(plistlib.dumps(spec));path.chmod(0o444 if label.endswith('.backup') else 0o600)
        target = boot/(label+'.plist');target.write_bytes(plistlib.dumps(spec));target.chmod(0o644)
        entries.append((path, spec))
    return entries


def test_protected_job_preflight_reads_all_exact_staging_and_boot_files(protected_jobs):
    entries = protected_jobs
    files = cutover.job_files(entries)
    assert len(files) == 20 and cutover.job_files(entries, files) == files
    count = 0

    def command(arguments):
        nonlocal count
        count += 1
        if arguments[0] == '/bin/ps':return 0, '1 0 1 0\n'
        label = arguments[-1].removeprefix('system/')
        path, spec = next(v for v in entries if v[1]['Label'] == label)
        return 0, ('system/'+label+' = {\n\tpath = '+str(path)+'\n\ttype = LaunchDaemon\n'
                   '\tprogram = /sealed/python\n\targuments = {\n'
                   + ''.join('\t\t'+v+'\n' for v in spec['ProgramArguments'])+'\t}\n}\n')

    assert cutover.preflight(entries, 600, command)['files'] == files and count == 21


@pytest.mark.parametrize('target', ['staging', 'boot', 'backup'])
@pytest.mark.parametrize('damage', ['bytes', 'mode', 'symlink', 'hardlink', 'fifo', 'same-bytes-replacement'])
def test_protected_jobs_preserve_changed_or_aliased_artifacts(protected_jobs, target, damage):
    entries = protected_jobs
    files = cutover.job_files(entries)
    path, spec = next(v for v in entries if v[1]['Label'].endswith('.backup')) if target == 'backup' else entries[0]
    if target == 'boot':path = cutover.LAUNCHD/(spec['Label']+'.plist')
    wire = path.read_bytes();mode = path.stat().st_mode & 0o777
    if damage == 'bytes':
        path.chmod(mode | 0o200);path.write_bytes(wire+b'changed');path.chmod(mode)
    elif damage == 'mode':path.chmod(0o666)
    else:
        saved = path.with_suffix('.saved');path.rename(saved)
        if damage == 'symlink':path.symlink_to(saved.name)
        elif damage == 'hardlink':os.link(saved, path)
        elif damage == 'fifo':os.mkfifo(path, mode)
        else:path.write_bytes(wire);path.chmod(mode)
    identity = path.lstat()
    with pytest.raises((ModError, OSError)):cutover.job_files(entries, files)
    assert cutover.stamp(path.lstat()) == cutover.stamp(identity)


def test_protected_jobs_refuse_directory_links_writable_ancestors_and_unknown_paths(protected_jobs):
    entries = protected_jobs
    generated, spec = entries[0]
    with pytest.raises(ModError):cutover.job_files([(generated.with_name('foreign.plist'), spec), *entries[1:]])
    saved = generated.parent.with_name('saved-parent');generated.parent.rename(saved);generated.parent.symlink_to(saved.name)
    with pytest.raises(OSError):cutover.job_files(entries)
    generated.parent.unlink();saved.rename(generated.parent)
    generated.parent.chmod(0o777)
    with pytest.raises(ModError):cutover.job_files(entries)
