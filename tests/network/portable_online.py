"""Check an actual portable package with system Python/Git removed from PATH."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def probe(stage, game):
    import ssl
    import tkinter
    from online_client import Client, Session
    from online_host import LocalDirectory, invitation, parse_invitation
    from online_paths import ROOT, PORTABLE, HANDSHAKES, SETTINGS, MANIFEST
    from online_transport import LOCAL, LOCAL_MAGIC, virtual_ip
    import importlib.util

    assert PORTABLE and ROOT == stage
    assert HANDSHAKES == stage/'Logs/Online' and SETTINGS == stage/'Settings/pc-online.json'
    assert all(str(path).lower().startswith(str(stage/'Support').lower()) for path in sys.path)
    window = tkinter.Tk()
    window.withdraw()
    window.update()
    window.destroy()
    version = json.loads(MANIFEST.read_text())['compatibility']
    spec = importlib.util.spec_from_file_location('network_identity', stage/'Support/Online/network-identity.py')
    identity = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(identity)
    identity.verify_content(game, json.loads(MANIFEST.read_text()))
    sessions, games = [], []
    with tempfile.TemporaryDirectory(prefix='tsfp-portable-online-') as temporary:
        root = Path(temporary)
        marker = root/'Support/Online/online.py'
        marker.parent.mkdir(parents=True)
        marker.write_text('')
        directory = LocalDirectory(root, '203.0.113.7', 0)
        try:
            client = Client(directory.address, directory.fingerprint)
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as available:
                available.bind(('127.0.0.1', 0))
                port = available.getsockname()[1]
            host = Session.create(client, version, 'Portable self-test', port, True)
            sessions.append(host)
            value = invitation(directory.public_url, directory.fingerprint, host.lobby['code'])
            assert parse_invitation(value)[2] == host.lobby['code']
            assert client.request('GET', '/v2/lobbies?compatibility='+version)['lobbies']
            with directory.server.directory.lock:
                directory.server.directory.online[host.lobby['code']]['address'] = '127.0.0.1'
            sessions.extend([Session.join(client, version, host.lobby['code']) for _ in range(2)])
            for active in sessions:
                game = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                games.append(game)
                game.bind(('127.0.0.1', 0))
                game.settimeout(3)
                bridge = active.bridge
                game.sendto(LOCAL.pack(LOCAL_MAGIC, bytes.fromhex(version), bridge.local_token,
                                      virtual_ip(bridge.peer_id), bytes(4), 1000, 0), bridge.local.getsockname())
                deadline = time.monotonic()+3
                while 1000 not in bridge.ports and time.monotonic() < deadline:
                    time.sleep(.01)
                assert 1000 in bridge.ports
            # An authenticated guest-to-guest packet must traverse the host.
            first, second = sessions[1].bridge, sessions[2].bridge
            games[1].sendto(LOCAL.pack(LOCAL_MAGIC, bytes.fromhex(version), first.local_token,
                                     virtual_ip(first.peer_id), virtual_ip(second.peer_id), 1000, 1000)
                            + b'portable multiplayer', first.local.getsockname())
            while True:
                packet = games[2].recv(8192)
                if LOCAL.unpack_from(packet)[-1] != 0:  # Native code consumes bridge RTT telemetry.
                    break
            assert packet[LOCAL.size:] == b'portable multiplayer'
            cert = root/'Settings/OnlineDirectory/certificate.pem'
            assert cert.is_file() and len(directory.fingerprint) == 64
            assert hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest() == directory.fingerprint
        finally:
            for game in games:
                game.close()
            for active in reversed(sessions):
                active.close()
            directory.close()
        assert not directory.server.directory.online
    print('PASS: game-content verification, isolated portable Python/Tk, HTTPS certificates, directory, invitations and three-peer UDP relay')


if __name__ == '__main__':
    import hashlib
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--game', type=Path, help='Imported game folder (defaults to package/Data)')
    parser.add_argument('--probe', action='store_true')
    args = parser.parse_args()
    stage = args.stage.resolve()
    game = (args.game or stage/'Data').resolve()
    if not game.is_dir():
        parser.error('Import game files first, or pass --game with the imported folder.')
    if args.probe:
        probe(stage, game)
    else:
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith(('PYTHON', 'TCL', 'TK_', 'RECOMP_', 'TSFP_'))}
        environment.update(PATH=str(Path(os.environ['SystemRoot'])/'System32'),
                           PYTHONHOME='Z:/no-system-python', PYTHONPATH='Z:/no-system-packages')
        checked = subprocess.run([str(stage/'Support/Python/python.exe'), '-B', str(Path(__file__).resolve()),
                                  '--stage', str(stage), '--game', str(game), '--probe'], env=environment,
                                 capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        print(checked.stdout, end='')
        if checked.returncode:
            raise RuntimeError(checked.stderr)
