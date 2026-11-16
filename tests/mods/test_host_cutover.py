"""Cutover observations bind fixed jobs and conservatively prove process exit."""
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
from mod_package import ModError


@pytest.fixture
def job():
    label = next(v for v in cutover.jobs() if '.gateway.' in v and not v.endswith('.monitor'))
    generated = Path('/protected monitor/gateway/worker.plist')
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
    entries = [(generated, dict(base, Label=label)) for label in labels]
    calls = []

    def command(arguments):
        calls.append(arguments)
        if arguments[0] == '/bin/ps':
            return (1, '') if change == 'process-failed' else (0, '1 0 1 0\n'+''.join(f'{pid} 1 {pid} 600\n' for pid in range(123,133)))
        label = arguments[-1].removeprefix('system/')
        output = template.replace(base['Label'], label)
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
