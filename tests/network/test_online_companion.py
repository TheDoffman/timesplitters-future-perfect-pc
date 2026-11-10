"""Run the real browser, supervisor and UDP bridge with isolated content/settings."""
import hashlib
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import threading
import time

import pytest
from bundle_online import COMPANION, repo_file
from lobby_server import Directory, Server, RelayServer
from online_client import Client, Session

ROOT = Path(__file__).resolve().parents[2]
VERSION = 'ab'*32


def test_game_process_detects_parent_exit(tmp_path):
    """An abrupt parent exit must stop supervision without trusting a recycled PID."""
    monitor = (
        'import sys,time; from pathlib import Path; from online_platform import GameProcess; '
        'parent=GameProcess(int(sys.argv[1])); folder=Path(sys.argv[2]); '
        'folder.joinpath("ready").touch(); deadline=time.monotonic()+15\n'
        'while not parent.exited() and time.monotonic()<deadline: time.sleep(.02)\n'
        'folder.joinpath("result").write_text(str(parent.exited())); parent.close()\n')
    owner = ('import subprocess,sys,os,time; '
             'subprocess.Popen([sys.executable,"-c",sys.argv[1],str(os.getpid()),sys.argv[2]]); '
             'time.sleep(20)')
    env = dict(os.environ, PYTHONPATH=str(ROOT/'tools/network'))
    process = subprocess.Popen([sys.executable, '-c', owner, monitor, str(tmp_path)], env=env)
    try:
        deadline = time.monotonic()+10
        while not (tmp_path/'ready').exists() and time.monotonic()<deadline:
            time.sleep(.02)
        assert (tmp_path/'ready').exists()
        process.terminate(); process.wait(timeout=5)
        while not (tmp_path/'result').exists() and time.monotonic()<deadline:
            time.sleep(.02)
        assert (tmp_path/'result').read_text() == 'True'
    finally:
        if process.poll() is None:
            process.terminate(); process.wait(timeout=5)


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin', 'linux') or
                    (sys.platform=='linux' and not os.environ.get('DISPLAY')),
                    reason='Desktop Tk runtime/display required')
@pytest.mark.parametrize('action', ['host', 'join', 'cancel'])
@pytest.mark.parametrize('transport', ['direct', 'relay'])
def test_browser_lifetime(tmp_path, action, transport):
    run_browser_lifetime(tmp_path, action, transport=transport)


@contextmanager
def transport_directory(transport):
    directory = Directory()
    relay = RelayServer(directory) if transport == 'relay' else None
    thread = None
    try:
        if relay:
            directory.relay_address = relay.socket.getsockname()
            thread = threading.Thread(target=relay.run, daemon=True)
            thread.start()
        yield directory
    finally:
        if relay:
            relay.close()
            thread.join(timeout=3)
            assert not thread.is_alive()


def run_browser_lifetime(tmp_path, action, package=None, *, transport='direct'):
    compatibility = VERSION if package is None else json.loads(
        (package/'Support/Online/network-identity.json').read_text())['compatibility']
    support = tmp_path/'Support/Online'; support.mkdir(parents=True)
    for name in COMPANION:
        shutil.copy2(repo_file(ROOT, 'tools', name), support/name)
    game = tmp_path/'Data'; game.mkdir(); (game/'default.xbe').write_bytes(b'isolated test content')
    manifest = dict(protocol='tsfp-pc-system-link-v1', compatibility=compatibility,
                    files=[['game/default.xbe', hashlib.sha256((game/'default.xbe').read_bytes()).hexdigest()]])
    (support/'network-identity.json').write_text(json.dumps(manifest))
    handshakes = tmp_path/'Logs/Online'; handshakes.mkdir(parents=True)
    result = handshakes/f'{os.getpid()}.ready'
    with transport_directory(transport) as directory, Server(('127.0.0.1', 0), directory) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        address = f'http://127.0.0.1:{server.server_port}'
        client = Client(address)
        # UI validates the advertised UDP port. Reserve a free unprivileged port.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
        host = Session.create(client, compatibility, 'fixture', port, True,
                              transport=transport) if action == 'join' else None
        command = tmp_path/'private/network-validation/command.json'; command.parent.mkdir(parents=True)
        command.write_text(json.dumps(dict(action=action, directory=address, port=port, transport=transport,
                                          code=host.lobby['code'] if host else '')))
        environment = dict(os.environ, RECOMP_GAME_DIR=str(game),
            TSFP_ONLINE_HANDSHAKES=str(handshakes), TSFP_ONLINE_MANIFEST=str(support/'network-identity.json'),
            TSFP_ONLINE_SETTINGS=str(tmp_path/'Settings/online.json'), TSFP_ONLINE_TEST_COMMAND=str(command))
        command_line = [sys.executable, '-B', str(support/'online.py'), '--pid', str(os.getpid()),
                        '--result', str(result), '--compatibility', compatibility]
        if package is not None:
            environment.update(TSFP_ONLINE_SCRIPT=str(support/'online.py'),
                TSFP_ONLINE_PYTHON=str(package/'Support/Python/bin/python3'))
            if action=='cancel':environment['TSFP_ONLINE_TEST_CANCEL']='1'
            command_line=[str(package/'Bin/futureperfect_recomp'), '--online-self-test']
        process = subprocess.Popen(command_line, env=environment,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            if action != 'cancel' and package is None:
                deadline = time.monotonic()+15
                while not result.exists() and process.poll() is None and time.monotonic()<deadline:
                    time.sleep(.05)
                assert result.exists(), 'Browser did not publish its bridge handshake'
                parts = result.read_text().split()
                assert len(parts) == 3 and int(parts[0]) > 0 and len(parts[1]) == 32
                assert parts[2].startswith('10.254.')
                assert process.poll() is None  # Bridge survives the withdrawn browser.
                result.with_suffix('.stop').touch()
            stdout, stderr = process.communicate(timeout=12)
            assert process.returncode == 0, stdout+stderr
            assert not result.exists() and not result.with_suffix('.stop').exists()
            lobbies = client.request('GET', '/v2/lobbies?compatibility='+compatibility)['lobbies']
            assert len(lobbies) == (1 if host else 0)
        finally:
            if process.poll() is None:
                process.kill(); process.communicate()
            if host:
                host.close()
            server.shutdown(); thread.join(timeout=3)


@pytest.mark.skipif(sys.platform != "linux" or not os.environ.get("DISPLAY") or
                    not os.environ.get("TSFP_TEST_LINUX_PACKAGE"),
                    reason="Requires a local Linux package and private desktop display")
@pytest.mark.parametrize("action", ["host", "join", "cancel"])
def test_packaged_native_adapter_lifetime(tmp_path, action):
    run_browser_lifetime(tmp_path, action, Path(os.environ["TSFP_TEST_LINUX_PACKAGE"]).resolve())
