"""The combined check must fail when either host or the launcher was skipped."""


import importlib.util
from pathlib import Path

import pytest


root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('ci_pr_gate', root / 'tools/ci/ci-pr-gate.py')
gate_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate_module)


@pytest.mark.parametrize('flags,results', [
    (('false', 'false', 'false', 'false'), ('skipped', 'skipped', 'skipped')),
    (('true', 'true', 'false', 'true'), ('success', 'success', 'success')),
    (('false', 'false', 'true', 'true'), ('skipped', 'skipped', 'success')),
])
def test_selected_work_passes(flags, results):
    assert 'passed' in gate_module.gate('success', *flags, *results, release_result='success' if flags[2] == 'true' else 'skipped')


@pytest.mark.parametrize('index,bad', [
    (0, 'failure'), (0, 'cancelled'), (0, 'skipped'),
    (5, 'failure'), (6, 'cancelled'), (7, 'skipped'),
    (1, ''), (2, 'true'), (3, 'true'), (4, 'true'),
])
def test_failure_or_unexpected_skip_cannot_pass(index, bad):
    args = ['success', 'false', 'false', 'false', 'false',
            'skipped', 'skipped', 'skipped']
    if index in (5, 6, 7):
        args[index - 4] = 'true'
    args[index] = bad
    with pytest.raises(ValueError):
        gate_module.gate(*args, release_result='skipped')


@pytest.mark.parametrize('name', ['MAC_GPU', 'LINUX', 'ISSUE_FORMS', 'ISSUE_RESOLUTION', 'TEXTURES'])
@pytest.mark.parametrize('selected,result,passes', [
    ('true', 'success', True), ('false', 'skipped', True),
    ('true', 'failure', False), ('true', 'cancelled', False),
    ('true', 'skipped', False), ('true', '', False),
    ('false', 'failure', False), ('false', 'success', False),
    ('', 'skipped', False), ('TRUE', 'success', False),
])
def test_extra_checks_fail_closed(name, selected, result, passes):
    args = ('success', 'false', 'false', 'false', 'false',
            'skipped', 'skipped', 'skipped')
    checks = [(name, selected, result)]
    if passes:
        assert 'passed' in gate_module.gate(*args, release_result='skipped', checks=checks)
    else:
        with pytest.raises(ValueError):
            gate_module.gate(*args, release_result='skipped', checks=checks)


@pytest.mark.parametrize('selected,result,passes', [
    ('true', 'success', True), ('false', 'skipped', True),
    ('true', 'failure', False), ('true', 'cancelled', False),
    ('true', 'skipped', False), ('true', '', False),
    ('false', 'success', False), ('false', 'failure', False),
])
def test_independent_release_result_is_required(selected, result, passes):
    args = ('success', 'false', 'false', selected, 'false',
            'skipped', 'skipped', 'skipped')
    if passes:
        assert 'passed' in gate_module.gate(*args, release_result=result)
    else:
        with pytest.raises(ValueError):
            gate_module.gate(*args, release_result=result)


@pytest.mark.parametrize('missing', [None, 'MAC_GPU', 'LINUX', 'ISSUE_FORMS', 'ISSUE_RESOLUTION', 'TEXTURES', 'RELEASE_RESULT'])
def test_command_requires_all_repository_scope_outputs(missing):
    import subprocess
    import sys
    env = dict(PLAN_RESULT='success', RUN_TRANSITIONS='false', RUN_NATIVE='false',
               RUN_RELEASE='false', RUN_LAUNCHER='false', PATCHES_RESULT='skipped',
               NATIVE_RESULT='skipped', LAUNCHER_RESULT='skipped', RELEASE_RESULT='skipped')
    for name in ('MAC_GPU', 'LINUX', 'ISSUE_FORMS', 'ISSUE_RESOLUTION', 'TEXTURES'):
        env['RUN_' + name] = 'false'
        env[name + '_RESULT'] = 'skipped'
    if missing:
        del env[missing if missing == 'RELEASE_RESULT' else 'RUN_' + missing]
    result = subprocess.run([sys.executable, str(root / 'tools/ci/ci-pr-gate.py')],
                            env=env, capture_output=True, text=True)
    assert (result.returncode == 0) == (missing is None), result.stderr


summary_spec = importlib.util.spec_from_file_location(
    'pr_check_summary', root / 'tools/ci/pr-check-summary.py')
summary = importlib.util.module_from_spec(summary_spec)
summary_spec.loader.exec_module(summary)


def test_summary_groups_shards_and_links_failed_jobs():
    jobs = [dict(id=i, name=f'Runtime · Windows · {i}/3',
                 conclusion='failure' if i == 2 else 'success',
                 started_at='2026-11-08T21:00:00Z',
                 completed_at=f'2026-11-08T21:0{i}:00Z') for i in (1, 2, 3)]
    jobs += [dict(id=4, name='textures', conclusion='skipped'),
             dict(id=5, name='PR validation', conclusion=None)]
    report = summary.render(jobs, 'https://github.com/owner/repo/actions/runs/123')
    assert report.count('| Runtime · Windows |') == 1
    assert 'Needs attention (2/3)' in report and '3m 00s' in report
    assert '/job/2)' in report and '/job/1)' not in report
    assert '<details>' in report and '- textures' in report
    assert '| PR validation |' not in report


@pytest.mark.parametrize('result', ['success', 'cancelled', 'timed_out', None, 'skipped'])
def test_summary_handles_every_job_outcome_and_escapes_labels(result):
    report = summary.render([dict(id=1, name='<script>|\nname', conclusion=result)],
                            'https://github.com/owner/repo/actions/runs/123')
    assert '<script>' not in report and '&lt;script&gt;&#124; name' in report
    if result == 'skipped':
        assert 'Skipped checks' in report
    elif result == 'success':
        assert 'Passed (1/1)' in report
    else:
        assert 'Needs attention (0/1)' in report and '/job/1)' in report


def test_summary_reads_only_current_attempt_with_pagination(tmp_path, monkeypatch):
    import io
    import json
    urls = []
    def fetch(request, timeout):
        urls.append(request.full_url)
        assert timeout == 15
        jobs = [dict(id=i, name='Runtime · Mac · 1/3', conclusion='success')
                for i in range(100)] if len(urls) == 1 else []
        return io.StringIO(json.dumps(dict(jobs=jobs)))
    for key, value in dict(GITHUB_REPOSITORY='owner/repo', GITHUB_RUN_ID='123',
                           GITHUB_RUN_ATTEMPT='2', GH_TOKEN='fixture-token',
                           GITHUB_STEP_SUMMARY=str(tmp_path / 'summary.md')).items():
        monkeypatch.setenv(key, value)
    monkeypatch.setattr(summary, 'urlopen', fetch)
    summary.main()
    assert len(urls) == 2
    assert all('/runs/123/attempts/2/jobs?' in url for url in urls)
    assert urls[-1].endswith('page=2')
    output = (tmp_path / 'summary.md').read_text(encoding='utf-8')
    assert 'Passed (100/100)' in output and 'fixture-token' not in output


def test_summary_cannot_override_the_required_gate():
    workflow = (root / '.github/workflows/runtime-patches.yml').read_text(encoding='utf-8')
    assert "name: Runtime · ${{ matrix.os == 'windows-latest' && 'Windows' || 'Mac' }} · ${{ matrix.shard }}/3" in workflow
    assert "name: Patches · ${{ matrix.os == 'windows-latest' && 'Windows' || 'Mac' }} · ${{ matrix.shard }}/3" in workflow
    gate = workflow.split('\n  pr-validation:\n')[1]
    assert 'name: PR validation' in gate and 'actions: read' in gate
    decision, reporting = gate.split('      - name: Summarize check results and durations')
    assert 'run: python tools/ci/ci-pr-gate.py' in decision
    assert 'continue-on-error' not in decision
    assert 'if: always()' in reporting and 'continue-on-error: true' in reporting
