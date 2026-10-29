"""Timing warnings must never replace coverage or the actual pytest result."""
import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest
import suite_timing as timing


def sample(seconds=100, cases=None, **values):
    result = dict(timestamp='2026-10-28T00:00:00Z', seconds=seconds, exitstatus=0,
                  complete=True, revision='abc', dirty=False,
                  counts={'passed': 1}, cases=cases or {'test_one': 'passed'}, slowest=[])
    result.update(values)
    return result


def trained():
    state = {}
    for seconds in (90, 100, 110):
        timing.update(state, sample(seconds))
    assert state['baseline']['seconds'] == 100
    return state


def test_fixed_median_requires_three_consecutive_slow_runs_and_never_drifts():
    state = trained()
    assert not timing.update(state, sample(125))['warnings']
    for seconds in (126, 140):
        assert not timing.update(state, sample(seconds))['warnings']
    assert 'Sustained' in timing.update(state, sample(150))['warnings'][0]
    assert state['baseline']['seconds'] == 100
    assert not timing.update(state, sample(110))['warnings']
    assert state['slow_streak'] == 0


@pytest.mark.parametrize('values', [dict(exitstatus=1), dict(complete=False),
                                  dict(cases={'test_one': 'failed'}), dict(cases={'test_one': 'xfailed'})])
def test_unhealthy_runs_never_train_or_extend_slow_streak(values):
    state = {}
    timing.update(state, sample(10000, **values))
    assert not state.get('samples') and not state.get('baseline')
    state = trained()
    timing.update(state, sample(200))
    timing.update(state, sample(200, **values))
    assert state['slow_streak'] == 0
    assert len(state['history']) == 5


@pytest.mark.parametrize('cases', [{'other': 'passed'}, {'test_one': 'skipped'},
                                 {'test_one': 'passed', 'added': 'skipped'}])
def test_missing_or_new_skips_warn_even_when_faster_and_do_not_update_guard(cases):
    state = trained()
    result = timing.update(state, sample(10, cases))
    assert any('Coverage changed' in message for message in result['warnings'])
    assert state['guard'] == {'test_one': 'passed'}
    assert result['ratio'] is None


def test_added_passes_join_guard_without_erasing_original_timing_budget():
    state = trained()
    timing.update(state, sample(120, {'test_one': 'passed', 'added': 'passed'}))
    assert state['baseline']['seconds'] == 100
    result = timing.update(state, sample())
    assert result['coverage']['missing'] == ['added']


def test_calibration_requires_same_inventory_and_keeps_known_skips():
    state = {}
    timing.update(state, sample())
    cases = {'test_one': 'passed', 'optional': 'skipped'}
    for _ in range(2):
        timing.update(state, sample(cases=cases))
        assert not state.get('baseline')
    timing.update(state, sample(cases=cases))
    assert state['guard'] == cases
    for _ in range(65):
        timing.update(state, sample(cases=cases))
    assert len(state['history']) == timing.HISTORY_LIMIT


def test_history_is_separate_for_host_worker_and_platform_and_preserves_corruption(tmp_path):
    config = SimpleNamespace(getoption=lambda name, default=None: default)
    identity = timing.environment(config)
    folders = []
    for change in ({}, {'workers': 4}, {'system': 'another-os'}, {'host': 'another-machine'}):
        folder, _ = timing.store_run(tmp_path, 'test', dict(identity, **change), sample())
        folders.append(folder)
    assert len(set(folders)) == 4
    state = folders[0]/'state.json'
    state.write_text('{broken', encoding='utf-8')
    with pytest.raises(ValueError):
        timing.store_run(tmp_path, 'test', identity, sample())
    assert state.read_text() == '{broken'
    assert not (folders[0]/'state.lock').exists()


def test_occupied_lock_is_not_removed_or_overwritten(tmp_path):
    folder, _ = timing.store_run(tmp_path, 'test', {}, sample())
    lock = folder/'state.lock'
    lock.write_text('another writer')
    previous = (folder/'state.json').read_bytes()
    with pytest.raises(FileExistsError):
        timing.store_run(tmp_path, 'test', {}, sample(999))
    assert lock.read_text() == 'another writer'
    assert (folder/'state.json').read_bytes() == previous


def test_teardown_failure_and_subtests_do_not_disappear_or_inflate_test_counts():
    monitor = timing.Monitor(None, 'test')
    def report(when, outcome, **extra):
        return SimpleNamespace(nodeid='test_one@tsfp-'+'a'*24, duration=1,
                               when=when, outcome=outcome, failed=outcome=='failed', **extra)
    monitor.pytest_runtest_logreport(report('call', 'passed', context=object()))
    monitor.pytest_runtest_logreport(report('call', 'passed'))
    monitor.pytest_runtest_logreport(report('teardown', 'failed'))
    assert monitor.outcomes == {'test_one': 'error'}
    assert monitor.subtests == {'passed': 1}
    assert monitor.durations == {'test_one': 2}


@pytest.mark.parametrize('mode', ['serial', 'parallel', 'failed', 'filtered', 'collect', 'corrupt', 'slowdown'])
def test_real_pytest_records_once_and_preserves_result(tmp_path, mode):
    source = tmp_path/'test_example.py'
    source.write_text('def test_one():\n    assert '+('False' if mode == 'failed' else 'True')+
                      '\ndef test_two():\n    pass\n', encoding='utf-8')
    history = tmp_path/'history'
    env = os.environ.copy()
    helper = Path(timing.__file__).parent
    env['PYTHONPATH'] = str(helper)+os.pathsep+env.get('PYTHONPATH', '')
    command = [sys.executable, '-m', 'pytest', '-p', 'suite_timing', str(source), '-q',
               '-p', 'no:cacheprovider', '--suite-profile=test', '--suite-history-root', str(history)]
    if mode == 'parallel':
        command += ['-n', '2', '--dist=loadgroup', '--max-worker-restart=0']
    elif mode == 'filtered':
        command += ['-k', 'test_one']
    elif mode == 'collect':
        command += ['--collect-only']
    def execute():
        return subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    result = execute()
    assert result.returncode == (1 if mode == 'failed' else 0), result.stdout+result.stderr
    states = list(history.glob('test/*/state.json'))
    assert len(states) == 1
    state = json.loads(states[0].read_text())
    assert len(state['history']) == 1
    run = json.loads(states[0].with_name('last-run.json').read_text())
    assert run['complete'] == (mode not in ('filtered', 'collect'))
    assert len(run['cases']) == (0 if mode == 'collect' else 1 if mode == 'filtered' else 2)
    if mode in ('filtered', 'failed', 'collect'):
        assert not state.get('samples')
    if mode == 'corrupt':
        states[0].write_text('{broken')
        result = execute()
        assert result.returncode == 0 and 'Timing monitor unavailable' in result.stdout
        assert states[0].read_text() == '{broken'
    if mode == 'slowdown':
        state['baseline'] = dict(seconds=0.000001, samples=3, revisions=['fixture']*3)
        state['guard'] = run['cases']
        state['slow_streak'] = 2
        states[0].write_text(json.dumps(state))
        result = execute()
        assert result.returncode == 0 and 'WARNING: Sustained suite slowdown' in result.stdout


def test_ci_persists_separate_advisory_history_and_reports():
    workflow = (Path(__file__).resolve().parents[2]/'.github/workflows/runtime-patches.yml').read_text()
    for profile in ('ci-patches', 'ci-native', 'ci-release'):
        assert '--suite-profile='+profile in workflow
    assert workflow.count('name: Restore suite timing history') == 2
    assert workflow.count('name: Save suite timing history') == 2
    assert workflow.count('name: Upload suite timing reports') == 2
    assert 'tests/support/test_suite_timing.py' in workflow
