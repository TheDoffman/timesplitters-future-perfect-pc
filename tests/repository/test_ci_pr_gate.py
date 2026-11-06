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
    (('false', 'false', 'true', 'true'), ('skipped', 'success', 'success')),
])
def test_selected_work_passes(flags, results):
    assert 'passed' in gate_module.gate('success', *flags, *results)


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
        gate_module.gate(*args)


@pytest.mark.parametrize('name', ['LINUX', 'ISSUE_FORMS', 'ISSUE_RESOLUTION', 'TEXTURES'])
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
        assert 'passed' in gate_module.gate(*args, checks=checks)
    else:
        with pytest.raises(ValueError):
            gate_module.gate(*args, checks=checks)


@pytest.mark.parametrize('missing', [None, 'LINUX', 'ISSUE_FORMS', 'ISSUE_RESOLUTION', 'TEXTURES'])
def test_command_requires_all_repository_scope_outputs(missing):
    import subprocess
    import sys
    env = dict(PLAN_RESULT='success', RUN_TRANSITIONS='false', RUN_NATIVE='false',
               RUN_RELEASE='false', RUN_LAUNCHER='false', PATCHES_RESULT='skipped',
               NATIVE_RESULT='skipped', LAUNCHER_RESULT='skipped')
    for name in ('LINUX', 'ISSUE_FORMS', 'ISSUE_RESOLUTION', 'TEXTURES'):
        env['RUN_' + name] = 'false'
        env[name + '_RESULT'] = 'skipped'
    if missing:
        del env['RUN_' + missing]
    result = subprocess.run([sys.executable, str(root / 'tools/ci/ci-pr-gate.py')],
                            env=env, capture_output=True, text=True)
    assert (result.returncode == 0) == (missing is None), result.stderr
