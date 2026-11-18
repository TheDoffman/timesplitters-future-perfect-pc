"""Driver observations cannot turn crashes, timeouts or partial work into passes."""
import importlib.util
import json
from pathlib import Path
import subprocess
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('query_lifetime_probe', ROOT / 'tools/testing/test-vulkan-query-lifetime.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def result(completed=16, **updates):
    value = dict(schema=1, phase='query-lifetime-probe', iterations=16, completed=completed,
                 query_type='occlusion-reset', timeline_wait=True, device='disposable test device')
    value.update(updates)
    return json.dumps(value)


@pytest.mark.parametrize('code,status', [(-11, 'crashed'), (2, 'failed'), (77, 'unsupported')])
def test_native_failure_does_not_admit_even_valid_looking_stdout(monkeypatch, code, status):
    monkeypatch.setattr(probe.subprocess, 'run', lambda *a, **k: subprocess.CompletedProcess(a, code, result(), 'driver result'))
    assert probe.observe(Path('/probe'), {}, 16)['status'] == status


@pytest.mark.parametrize('data', [result(0), result(15), result(schema=True), result(timeline_wait=False),
                                    result(iterations=1), result(query_type='other'), '{}', 'not json', result() + 'extra'])
def test_incomplete_or_malformed_native_success_is_not_admitted(monkeypatch, data):
    monkeypatch.setattr(probe.subprocess, 'run', lambda *a, **k: subprocess.CompletedProcess(a, 0, data, ''))
    assert probe.observe(Path('/probe'), {}, 16)['status'] == 'failed'


def test_exact_observation_is_admitted_and_output_tails_are_bounded(monkeypatch):
    monkeypatch.setattr(probe.subprocess, 'run', lambda *a, **k: subprocess.CompletedProcess(a, 0, result(), 'x' * 50000))
    value = probe.observe(Path('/probe'), {}, 16)
    assert value['status'] == 'passed' and value['observation']['completed'] == 16
    assert len(value['stderr_tail']) == 8192


def test_timeout_is_recorded_with_partial_evidence_and_never_admitted(monkeypatch):
    def hang(*args, **kwargs):
        raise subprocess.TimeoutExpired(args[0], kwargs['timeout'], output=b'partial', stderr=b'stalled')
    monkeypatch.setattr(probe.subprocess, 'run', hang)
    result = probe.observe(Path('/probe'), {}, 16)
    assert result == dict(status='timeout', returncode=None, stdout_tail='partial', stderr_tail='stalled')


@pytest.mark.parametrize('iterations,runs', [(0, 1), (16385, 1), (1, 0), (1, 9), (True, 1)])
def test_invalid_limits_do_not_create_output(tmp_path, monkeypatch, iterations, runs):
    monkeypatch.setattr(probe.platform, 'system', lambda: 'Linux')
    target = tmp_path / 'not-created'
    with pytest.raises(ValueError):
        probe.execute(target, iterations=iterations, runs=runs)
    assert not target.exists()


def test_existing_output_is_preserved_before_any_compiler_or_driver_work(tmp_path, monkeypatch):
    monkeypatch.setattr(probe.platform, 'system', lambda: 'Linux')
    marker = tmp_path / 'personal'
    marker.write_bytes(b'preserve')
    monkeypatch.setattr(probe.shutil, 'which', lambda *a: pytest.fail('Existing output reached compiler selection'))
    with pytest.raises(ValueError, match='new diagnostic'):
        probe.execute(tmp_path)
    assert marker.read_bytes() == b'preserve'


def test_low_disk_refuses_before_sdk_setup_compilation_or_output_creation(tmp_path, monkeypatch):
    monkeypatch.setattr(probe.platform, 'system', lambda: 'Darwin')
    monkeypatch.setattr(probe.shutil, 'disk_usage', lambda *a: SimpleNamespace(free=128 * 1024**2))
    monkeypatch.setattr(probe.vulkan_sdk, 'setup', lambda *a: pytest.fail('Low disk reached SDK installation'))
    monkeypatch.setattr(probe.shutil, 'which', lambda *a: pytest.fail('Low disk reached compiler selection'))
    target = tmp_path / 'not-created'
    with pytest.raises(ValueError, match='headroom'):
        probe.execute(target, setup=True)
    assert not target.exists()
