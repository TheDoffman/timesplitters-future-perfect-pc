"""Local preflight and CI use identical suites and propagate every failure."""
import importlib.util
import json
from pathlib import Path
import re
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('runtime_checks', ROOT / 'tools/ci/run-runtime-checks.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def test_shared_manifest_has_real_paths_and_retains_both_host_routes():
    manifest = json.loads((ROOT / 'tools/ci/runtime-suites.json').read_text())
    assert runner.suite_paths('native', 'Darwin') == manifest['native']
    assert runner.suite_paths('native', 'Windows') == manifest['native'] + manifest['native_windows']
    for suite in runner.SUITES:
        assert len(runner.suite_paths(suite)) == len(set(runner.suite_paths(suite)))
    for path in ('tests/repository/test_runtime_checks.py', 'tests/support/test_runtime_shards.py'):
        assert path in manifest['native']
        assert path in (ROOT / '.github/workflows/runtime-patches.yml').read_text()


def test_failed_suite_does_not_conceal_later_suites(monkeypatch):
    seen = []
    def run(suite, args):
        seen.append(suite)
        return 1 if suite == 'patches' else 0
    monkeypatch.setattr(runner, 'run_suite', run)
    assert runner.main(['all']) == 1
    assert seen == ['patches', 'native', 'release']
    monkeypatch.setattr(runner, 'run_suite', lambda suite, args: 0)
    assert runner.main(['all']) == 0


def test_preparation_failure_still_runs_independent_release_checks(monkeypatch):
    seen = []
    monkeypatch.setattr(runner.subprocess, 'run', lambda *a, **k: SimpleNamespace(returncode=1))
    monkeypatch.setattr(runner, 'run_suite', lambda suite, args: seen.append(suite) or 0)
    assert runner.main(['all', '--prepare']) == 1
    assert seen == ['release']


def test_runner_uses_all_paths_even_when_previous_failures_go_first(tmp_path, monkeypatch):
    monkeypatch.setattr(runner, 'ROOT', tmp_path)
    paths = ['tests/test_first.py', 'tests/test_second.py']
    monkeypatch.setattr(runner, 'suite_paths', lambda suite: paths)
    calls = []
    monkeypatch.setattr(runner, 'report', lambda *args: None)
    def execute(command, **kwargs):
        calls.append(command)
        assert kwargs['cwd'] == tmp_path
        return SimpleNamespace(returncode=2)
    monkeypatch.setattr(runner.subprocess, 'run', execute)
    assert runner.main(['native', '--shard', '2/3', '--failed-first', '--profile-prefix', 'ci']) == 1
    command, = calls
    assert all(path in command for path in paths)
    assert '--failed-first' in command and '--lf' not in command
    assert '--runtime-shard=2/3' in command
    assert '--suite-profile=ci-native-s2-of-3' in command
    assert 'cache_dir=' + str(tmp_path / 'build/test-cache/native-s2-of-3') in command
    assert '--max-worker-restart=0' in command


@pytest.mark.parametrize('arguments', [
    ['native', '--shard', '4/3'], ['release', '--shard', '1/3'],
    ['all', '--shard', '1/3'], ['native', '--workers', '0'],
    ['native', '--profile-prefix', '../invalid'],
])
def test_bad_arguments_cannot_produce_partial_green_results(arguments):
    with pytest.raises(SystemExit) as error:
        runner.main(arguments)
    assert error.value.code != 0


def test_result_summary_retains_setup_errors_and_escapes_test_text(tmp_path, monkeypatch):
    xml = tmp_path / 'report.xml'
    xml.write_text('<testsuites><testsuite><testcase classname="fixture" name="one">'
                   '<error message="missing &lt;hook&gt;"/></testcase>'
                   '<testcase name="two"><skipped/></testcase></testsuite></testsuites>')
    summary = tmp_path / 'summary.md'
    monkeypatch.setenv('GITHUB_STEP_SUMMARY', str(summary))
    runner.report('native', 1, xml)
    output = summary.read_text()
    assert 'FAILED' in output and '1 failures/errors' in output and '1 skipped' in output
    assert 'missing &lt;hook&gt;' in output


def test_summary_groups_dependent_fixture_failures_by_compiler_diagnostic(tmp_path, monkeypatch):
    xml = tmp_path / 'report.xml'
    xml.write_text('<testsuites><testsuite>' + ''.join(
        f'<testcase name="case{i}"><error message="AssertionError: Compiler banner">'
        'E file.c(9): error C2065: missing_hook</error></testcase>' for i in range(6))
        + '</testsuite></testsuites>')
    summary = tmp_path / 'summary.md'
    monkeypatch.setenv('GITHUB_STEP_SUMMARY', str(summary))
    runner.report('native', 1, xml)
    output = summary.read_text()
    assert '6 failures/errors' in output and '6 failure(s)' in output
    assert output.count('error C2065: missing_hook') == 1
    assert 'Compiler banner' not in output and 'and 2 more' in output


def test_workflow_keeps_gpu_and_release_independent_and_required():
    workflow = (ROOT / '.github/workflows/runtime-patches.yml').read_text()
    plan = workflow.split('\n  plan:\n')[1].split('\n  patches:\n')[0]
    assert "('pytest==', 'pytest-xdist==', 'execnet==')" in plan
    for name in ('mac-gpu', 'release'):
        job = re.split(r'\n  [a-z_-]+:\n', workflow.split('\n  ' + name + ':\n')[1], maxsplit=1)[0]
        assert 'needs: plan' in job
        assert 'needs: native' not in job and '\n    continue-on-error:' not in job
        assert '${{ needs.' + name + '.result }}' in workflow
    assert 'RELEASE_RESULT:' in workflow and 'RUN_MAC_GPU:' in workflow
