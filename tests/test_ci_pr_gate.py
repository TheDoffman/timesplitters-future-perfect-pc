"""The combined check must fail when either host or the launcher was skipped."""
import importlib.util
from pathlib import Path

import pytest


root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ci_pr_gate', root / 'tools/ci-pr-gate.py')
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
