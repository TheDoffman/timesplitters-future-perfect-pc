"""Preservation and bounded-result contracts; never bootstrap real system jobs."""
import ast
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'tools/mods/hosting/probe-system-service-mac.py'
spec = importlib.util.spec_from_file_location('system_service_probe', SOURCE)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
TARGET = 'org.timesplitters.workshop.permissions.' + 'a' * 32 + '.target'


def test_default_plan_never_starts_services():
    result = subprocess.run([sys.executable, '-B', str(SOURCE)], capture_output=True, check=True)
    value = json.loads(result.stdout)
    assert value['phase'] == 'plan-only' and value['temporary_jobs'] == 2
    assert not value['network_started'] and not value['production_services_deployed']


@pytest.mark.parametrize('role', ['target', 'watchdog'])
def test_system_job_is_locked_nonpersistent_and_interpreter_is_isolated(role):
    value = probe.job(TARGET.rsplit('.', 1)[0] + '.' + role, Path('/release/worker.py'), Path('/state'), role, TARGET)
    assert value['UserName'] == value['GroupName'] == '_tsfpworkshop'
    assert value['ProgramArguments'][:3] == ['/usr/bin/python3', '-I', '-B']
    assert value['RunAtLoad'] and not value['KeepAlive']
    assert value['HardResourceLimits'] == dict(Core=0, FileSize=65536, NumberOfFiles=64)
    assert value['StandardOutPath'] == value['StandardErrorPath'] == '/dev/null'


@pytest.mark.parametrize('label,role,target', [('other', 'target', TARGET), (TARGET, 'other', TARGET),
                                              ('other.target', 'target', 'other.target')])
def test_unrelated_job_identity_rejected(label, role, target):
    with pytest.raises(probe.identity.ProvisionError):
        probe.job(label, Path('/release/worker.py'), Path('/state'), role, target)


def test_apply_checks_privilege_before_reading_identity(monkeypatch):
    monkeypatch.setattr(probe.sys, 'platform', 'darwin')
    monkeypatch.setattr(probe.os, 'geteuid', lambda: 501, raising=False)
    monkeypatch.setattr(probe.identity, 'existing', lambda: pytest.fail('Read privileged state before authorization'))
    with pytest.raises(probe.identity.ProvisionError, match='administrator sudo'):
        probe.apply()


def test_worker_source_compiles_without_optional_dependencies():
    tree = ast.parse(probe.WORKER)
    imports = {alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names}
    assert imports <= {'json', 'os', 're', 'subprocess', 'sys', 'time'}
    compile(tree, 'worker.py', 'exec')


@pytest.mark.skipif(os.name != 'posix', reason='POSIX nofollow and ownership gate')
@pytest.mark.parametrize('kind', ['valid', 'symlink', 'hardlink', 'oversize', 'permissions', 'identity'])
def test_result_validation_preserves_unrecognized_files(tmp_path, kind):
    value = dict(uid=os.getuid(), gid=os.getuid(), pid=123, role='target')
    if kind == 'identity':
        value['uid'] += 1
    path = tmp_path / 'target.json'
    path.write_text(json.dumps(value) if kind != 'oversize' else 'x' * 4097)
    path.chmod(0o644 if kind == 'permissions' else 0o600)
    if kind == 'symlink':
        path.rename(tmp_path / 'original')
        path.symlink_to(tmp_path / 'original')
    if kind == 'hardlink':
        os.link(path, tmp_path / 'original')
    directory_fd = os.open(tmp_path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        if kind == 'valid':
            assert probe.read_result(directory_fd, 'target.json', os.getuid()) == value
        else:
            with pytest.raises((OSError, probe.identity.ProvisionError)):
                probe.read_result(directory_fd, 'target.json', os.getuid())
        assert path.exists()
    finally:
        os.close(directory_fd)
