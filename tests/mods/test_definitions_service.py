"""Private configuration, failure gates and actual maintenance child bounds."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
from mod_package import ModError
spec = importlib.util.spec_from_file_location('definitions_service', ROOT / 'tools/mods/hosting/definitions-service-mac.py')
service = importlib.util.module_from_spec(spec)
spec.loader.exec_module(service)


@pytest.fixture
def data(tmp_path):
    if os.name != 'posix':
        pytest.skip('Private POSIX maintenance service')
    tmp_path.chmod(0o700)
    for name in ('database', 'state'):
        (tmp_path / name).mkdir(mode=0o700)
    binary = tmp_path / 'freshclam-fixture'
    binary.write_text('#!/bin/sh\nexit 99\n')
    binary.chmod(0o700)
    path = tmp_path / 'service.json'
    path.write_text(json.dumps(dict(schema=1, freshclam=str(binary), database=str(tmp_path / 'database'),
                                   state=str(tmp_path / 'state'))))
    path.chmod(0o600)
    return service.config(path)


def test_generated_agent_uses_pinned_interpreter_and_fixed_official_configuration(data, tmp_path):
    result = service.generate(data, tmp_path / 'agents')
    path = tmp_path / 'agents' / (result['label'] + '.plist')
    value = plistlib.loads(path.read_bytes())
    assert value['ProgramArguments'][0] == str(Path(sys.executable).absolute())
    assert value['ProgramArguments'][-1] == '--once'
    assert value['StartInterval'] == 21600 and value['RunAtLoad']
    assert value['AbandonProcessGroup'] is False
    assert value['StandardOutPath'] == value['StandardErrorPath'] == '/dev/null'
    assert path.stat().st_mode & 0o077 == 0
    config = (data['state'] / 'freshclam.conf').read_text()
    assert 'DatabaseMirror database.clamav.net\n' in config and 'TestDatabases yes\n' in config
    assert not any(word in config for word in ('OnUpdate', 'Proxy', 'Notify', 'UpdateLogFile', 'PrivateMirror'))
    with pytest.raises(ModError, match='new'):
        service.generate(data, tmp_path / 'agents')


def test_changed_config_and_unknown_status_are_preserved(data):
    path = service.prepared_config(data)
    path.write_text('OnUpdateExecute unwanted-command\n')
    with pytest.raises(ModError, match='Preserving'):
        service.prepared_config(data)
    assert path.read_text() == 'OnUpdateExecute unwanted-command\n'
    path = data['state'] / 'definitions.json'
    path.write_text('{"personal":"preserve"}')
    with pytest.raises(ModError, match='Preserving'):
        service.status(data, {'status': 'running'})
    assert json.loads(path.read_text()) == {'personal': 'preserve'}


@pytest.mark.parametrize('mutation', ['option', 'newline', 'quote', 'group-writable'])
def test_config_rejects_injection_and_unsafe_executables(data, mutation):
    path = data['config']
    value = json.loads(path.read_bytes())
    if mutation == 'option':
        value['arguments'] = ['--daemon']
    elif mutation in ('newline', 'quote'):
        value['database'] += '\nOnUpdateExecute bad' if mutation == 'newline' else '"'
    else:
        data['freshclam'].chmod(0o720)
    path.write_text(json.dumps(value))
    with pytest.raises(ModError):
        service.config(path)


def test_failed_update_retains_fault_and_success_does_not_claim_database_freshness(data, monkeypatch):
    def fail(*_):
        raise subprocess.TimeoutExpired('fixture', 1)
    monkeypatch.setattr(service, 'command', fail)
    with pytest.raises(subprocess.TimeoutExpired):
        service.update(data)
    path = data['state'] / 'definitions.json'
    value = json.loads(path.read_bytes())
    assert value['status'] == 'error' and value['last_fault']['reason']
    monkeypatch.setattr(service, 'command', lambda *_: dict(output_bytes=0, output_sha256=hashlib.sha256(b'').hexdigest()))
    assert service.update(data)
    value = json.loads(path.read_bytes())
    assert value['status'] == 'complete' and 'last_fault' in value and 'fresh' not in value


def test_other_instance_lock_and_linked_storage_are_withheld(data, monkeypatch, tmp_path):
    import fcntl
    lock = data['state'] / 'definitions.lock'
    with lock.open('wb') as owner:
        lock.chmod(0o600)
        fcntl.flock(owner, fcntl.LOCK_EX | fcntl.LOCK_NB)
        monkeypatch.setattr(service, 'command', lambda *_: pytest.fail('Overlapping updater executed'))
        assert service.update(data) is False
    target = tmp_path / 'preserve'
    target.write_bytes(b'canary')
    (data['database'] / 'daily.cvd').symlink_to(target)
    with pytest.raises(ModError):
        service.update(data)
    assert target.read_bytes() == b'canary'


@pytest.mark.skipif(sys.platform != 'darwin', reason='Actual Darwin lifetime guardian')
@pytest.mark.parametrize('mode', ['success', 'failure', 'timeout', 'flood'])
def test_actual_guarded_maintenance_exit_timeout_and_output_limits(data, tmp_path, mode):
    subprocess.run([sys.executable, str(ROOT / 'tools/build/build-scan-guardian.py')],
                   capture_output=True, check=True, timeout=90)
    source = tmp_path / 'probe.c'
    # Fixed native probe cannot spawn children or inspect personal files.
    body = {'success': 'puts("checked");return 0;', 'failure': 'return 9;',
            'timeout': 'sleep(30);return 0;',
            'flood': 'char buf[65536];memset(buf,120,sizeof(buf));for(;;)if(write(1,buf,sizeof(buf))<0)return 1;'}[mode]
    source.write_text('#include <stdio.h>\n#include <unistd.h>\n#include <string.h>\nint main(void){' + body + '}\n')
    binary = tmp_path / 'probe'
    subprocess.run(['/usr/bin/clang', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True, timeout=30)
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(binary)], capture_output=True, check=True, timeout=15)
    data['freshclam'] = binary
    cfg = service.prepared_config(data)
    if mode == 'success':
        assert service.command(data, cfg, timeout=2)['output_bytes'] == len(b'checked\n')
    else:
        error = subprocess.TimeoutExpired if mode == 'timeout' else ModError
        with pytest.raises(error):
            service.command(data, cfg, timeout=.25 if mode == 'timeout' else 3)
