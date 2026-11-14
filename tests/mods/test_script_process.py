"""Real native confinement and independent supervisor checks; no game data."""
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time
import uuid

import pytest

import mod_script_protocol as protocol

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope='session')
def process_tools(tmp_path_factory):
    spec = importlib.util.spec_from_file_location('build_mod_process', ROOT / 'tools/build/build-mod-worker.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    local = Path(sys.executable).with_name('cmake.exe' if os.name == 'nt' else 'cmake')
    cmake = str(local) if local.is_file() else shutil.which('cmake')
    assert cmake, 'Install the repository-pinned CMake for native isolation checks'
    worker = builder.build(tmp_path_factory.mktemp('mod-process'), cmake, testing=True)
    suffix = '.exe' if os.name == 'nt' else ''
    return dict(worker=worker, runner=worker.with_name('tsfp-mod-runner' + suffix),
                probe=worker.with_name('tsfp-mod-process-probe' + suffix))


def supervise(tools, cache, data, image='probe', **kwargs):
    return subprocess.run([str(tools['runner']), str(tools[image]), str(cache)],
                          input=data, capture_output=True, timeout=8, **kwargs)


@contextlib.contextmanager
def private_canaries(tmp_path):
    """Give the test broker capabilities the mod must never inherit."""
    path = tmp_path / 'broker-secret.txt'
    path.write_text('owned canary, never a personal file')
    registry = '-'
    with path.open('rb') as secret, socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(4)
        port = listener.getsockname()[1]
        if os.name == 'nt':
            import msvcrt
            import winreg
            registry = 'Software\\TSFPModWorkerTests\\' + uuid.uuid4().hex
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, registry) as key:
                winreg.SetValueEx(key, 'Canary', 0, winreg.REG_SZ, 'owned test key')
            hidden = msvcrt.get_osfhandle(secret.fileno())
            os.set_handle_inheritable(hidden, True)
            startup = subprocess.STARTUPINFO()
            startup.lpAttributeList = {'handle_list': [hidden]}
            inherit = dict(startupinfo=startup, close_fds=True)
            # Ordinary AppContainers can read this explicit package-group grant.
            # A low-privilege AppContainer must also fail this read canary.
            subprocess.run(['icacls', str(path), '/grant', '*S-1-15-2-1:R'],
                           check=True, capture_output=True)
        else:
            import fcntl
            hidden = fcntl.fcntl(secret.fileno(), fcntl.F_DUPFD_CLOEXEC, 123)
            inherit = dict(pass_fds=(hidden,))
        try:
            yield path, port, hidden, registry, inherit
        finally:
            if os.name == 'nt':
                os.set_handle_inheritable(hidden, False)
                winreg.DeleteKey(winreg.HKEY_CURRENT_USER, registry)
            else:
                os.close(hidden)


def probe_input(mode, path, parent=0, port=0, hidden=0, registry='-'):
    return f'{mode}\n{path}\n{parent} {port} {hidden}\n{registry}\n'.encode()


def test_native_escape_canaries_have_positive_controls(process_tools, tmp_path):
    cache = tmp_path / 'cache'
    cache.mkdir()
    sentinel = cache / 'preserve.txt'
    sentinel.write_text('existing broker cache data')
    owned_profile = tmp_path / 'broker-profile'
    owned_profile.mkdir()
    environment = dict(os.environ, TSFP_SUPERVISOR_SECRET='test-only-sensitive-value', APPDATA=str(owned_profile))
    with private_canaries(tmp_path) as (path, port, hidden, registry, inherit):
        baseline = subprocess.run([str(process_tools['probe'])],
                                  input=probe_input('baseline', path, os.getpid(), port, hidden, registry),
                                  env=environment, capture_output=True, timeout=8, **inherit)
        assert baseline.returncode == 0, baseline.stderr
        control = json.loads(baseline.stdout)
        assert control['sealed'] is False
        for key in ('read', 'write', 'network', 'spawn', 'descriptor', 'secret'):
            assert control[key] == 1, (key, control)
        if os.name == 'nt':
            assert control['parent'] == control['registry'] == control['acl_write'] == control['profile_write'] == 1
        path.with_suffix('.txt.blocked').unlink()
        # Give the runner a real inherited file handle and secret. It must pass
        # only its private stdin/stdout/stderr and synthetic environment onward.
        with subprocess.Popen([str(process_tools['runner']), str(process_tools['probe']), str(cache)],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=environment, **inherit) as runner:
            output, error = runner.communicate(probe_input('confined', path, runner.pid, port, hidden, registry), timeout=8)
            assert runner.returncode == 0, error
        result = json.loads(output)
        assert result['sealed'] is True
        assert all(value == 0 for key, value in result.items() if key != 'sealed'), result
        assert not path.with_suffix('.txt.blocked').exists()
        assert path.read_text() == 'owned canary, never a personal file'
    assert list(cache.iterdir()) == [sentinel]
    assert not list(owned_profile.iterdir())
    assert sentinel.read_text() == 'existing broker cache data'


@pytest.mark.parametrize('mode,reason', [('hang', 'independent'), ('flood', 'output budget'),
                                       ('diagnostic', 'diagnostic budget')])
def test_parent_stops_native_faults_without_worker_watchdog(process_tools, tmp_path, mode, reason):
    started = time.monotonic()
    result = supervise(process_tools, tmp_path, probe_input(mode, tmp_path / 'unused'))
    assert result.returncode == 125
    assert result.stdout == b''
    assert reason in result.stderr.decode(), result.stderr
    assert time.monotonic() - started < 7
    assert not list(tmp_path.iterdir())


def test_native_allocation_is_bounded(process_tools, tmp_path):
    result = supervise(process_tools, tmp_path, probe_input('memory', tmp_path / 'unused'))
    assert result.returncode in (0, 125), result.stderr
    if result.returncode == 0:
        assert json.loads(result.stdout)['allocated'] < 128 * 1024 * 1024
    else:
        assert b'budget' in result.stderr
    assert not list(tmp_path.iterdir())


def test_production_worker_runs_actual_mode_inside_os_policy(process_tools, tmp_path):
    source = (ROOT / 'examples/mods/first-to-three/scripts/main.lua').read_bytes()
    players = [(1, 0, 0, 100, 0., 1., 2.), (2, 1, 0, 100, 2., 3., 4.)]
    data = protocol.load(source) + protocol.event(1, 1, players=players)
    data += b''.join(protocol.event(i + 2, 3, tick=i + 1, players=players, actor=1, target=2) for i in range(3))
    data += protocol.event(5, 4, tick=4, players=players) + protocol.quit_worker()
    result = supervise(process_tools, tmp_path, data, image='worker')
    assert result.returncode == 0 and result.stderr == b'', result.stderr
    replies = protocol.replies(result.stdout)
    assert len(replies) == 6 and all(reply['status'] == 0 for reply in replies)
    for sequence, reply in enumerate(replies[1:], 1):
        protocol.validate_commands(reply, sequence, [1, 2], stopping=sequence == 5)
    assert [reply['commands'][0]['value'] for reply in replies[2:5]] == [1, 2, 3]
    assert replies[4]['commands'][-1]['kind'] == 2
    assert not list(tmp_path.iterdir())


@pytest.mark.skipif(os.name != 'nt', reason='Windows requires the broker to establish its token/job before entry')
def test_windows_worker_refuses_unconfined_direct_launch(process_tools):
    result = subprocess.run([str(process_tools['worker'])], input=protocol.load(b'return {api=1}'),
                            capture_output=True, timeout=3)
    assert result.returncode == 126 and result.stdout == b''
    assert b'isolation policy unavailable' in result.stderr
