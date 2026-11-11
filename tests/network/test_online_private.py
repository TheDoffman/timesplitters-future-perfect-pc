"""The in-game host action starts the former player-owned directory directly."""
from types import SimpleNamespace
import socket

import pytest

import online_menu as menu
from online_client import Client, DirectoryError, Session
from online_private import Directory, LobbyError, LocalDirectory, certificate, start_directory


VERSION = 'a' * 64


def request(action, sequence=1, **values):
    data = dict(sequence=sequence, action=action, mode=1, source=1, public=True,
                port=0, name='My Lobby', code='', address='192.0.2.1', fingerprint='')
    data.update(values)
    return data


def wait(controller):
    import time
    deadline = time.monotonic() + 20
    while controller.status == menu.BUSY and time.monotonic() < deadline:
        time.sleep(.01)
    assert controller.status != menu.BUSY
    assert controller.status != menu.ERROR, controller.message


def udp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def test_native_host_starts_directory_before_any_lobby_and_retains_other_hosts(tmp_path, monkeypatch):
    monkeypatch.setattr(menu, 'ROOT', tmp_path)
    monkeypatch.delenv('TSFP_PRIVATE_SERVER_COMMAND', raising=False)
    parent = SimpleNamespace(exited=lambda: False)
    owner = menu.Controller(VERSION, menu.private_launcher(tmp_path/'result', parent))
    friend = menu.Controller(VERSION)
    guest = menu.Controller(VERSION)
    directory = None
    try:
        owner.begin(request(menu.PRIVATE_HOST)); wait(owner)
        directory = owner.private_process
        assert isinstance(directory, LocalDirectory) and directory.poll() is None
        assert owner.status == menu.PRIVATE_READY and owner.private_direct
        assert owner.session is None and directory.server.directory.online == {}
        assert menu.parse_server_invitation(menu.server_invitation(*owner.private)) == owner.private
        client = Client(directory.address, directory.fingerprint)
        assert client.request('GET', '/v2/lobbies?compatibility='+VERSION)['private_directory'] is True

        owner.begin(request(menu.CREATE, 2, port=udp_port())); wait(owner)
        assert isinstance(owner.session, Session) and owner.session.bridge.relay is None
        assert owner.session.lobby['capacity'] == 16
        code = owner.session.lobby['code']

        invitation = menu.server_invitation(directory.address, directory.fingerprint)
        friend.begin(request(menu.PRIVATE_CONNECT, code=invitation)); wait(friend)
        assert friend.private_direct
        friend.begin(request(menu.CREATE, 2, mode=2, port=udp_port(), name='Story team')); wait(friend)
        assert friend.session.lobby['capacity'] == 4

        guest.private = (directory.address, directory.fingerprint)
        guest.begin(request(menu.JOIN, code=code)); wait(guest)
        assert guest.session.bridge.ready.is_set()
        guest.begin(request(menu.LEAVE, 2)); wait(guest)
        assert directory.poll() is None

        owner.begin(request(menu.LEAVE, 3)); wait(owner)
        assert directory.poll() is None and owner.session is None
        owner.begin(request(menu.BROWSE, 4)); wait(owner)
        assert [row['name'] for row in owner.lobbies] == ['Story team']
        assert owner.private_process is directory
    finally:
        guest.close()
        friend.close()
        owner.close()
    assert directory is not None and directory.poll() == 0


def test_directory_certificate_is_reused_and_incomplete_pair_is_preserved(tmp_path):
    folder = tmp_path/'certificate'
    cert, key, pin = certificate(folder)
    original = cert.read_bytes(), key.read_bytes()
    assert certificate(folder) == (cert, key, pin)
    assert (cert.read_bytes(), key.read_bytes()) == original
    key.unlink()
    with pytest.raises(ValueError, match='incomplete'):
        certificate(folder)
    assert cert.read_bytes() == original[0] and not key.exists()


def test_default_host_detects_address_and_can_isolate_its_state(tmp_path, monkeypatch):
    import online_private
    state = tmp_path/'isolated-certificates'
    monkeypatch.setattr(online_private, 'fetch_public_ipv4', lambda: '192.0.2.1')
    monkeypatch.setenv('TSFP_PRIVATE_DIRECTORY_STATE', str(state))
    directory = start_directory(tmp_path, request(menu.PRIVATE_HOST, address=''))
    try:
        assert directory.public_url.startswith('https://192.0.2.1:')
        assert (state/'certificate.pem').is_file()
        assert not (tmp_path/'private/online-directory').exists()
    finally:
        directory.close()


@pytest.mark.parametrize('mode,capacity', [('arcade', 16), ('story', 4)])
def test_directory_credentials_capacity_expiry_and_match_metadata(mode, capacity):
    now = [0.0]
    directory = Directory(clock=lambda: now[0], public_address='192.0.2.1')
    body = dict(compatibility=VERSION, name='Match', port=41831, public=True, mode=mode)
    _, lobby = directory.request('POST', '/v2/lobbies', {}, body, '127.0.0.1', '')
    path = '/v2/lobbies/'+lobby['code']
    info = dict(map_scene=2, game_type_id=2, in_progress=False,
                map_name='Time to Split', game_type='Story')
    with pytest.raises(LobbyError) as error:
        directory.request('DELETE', path, {}, {}, '127.0.0.1', 'wrong')
    assert error.value.status == 403
    directory.request('PUT', path, {}, {**body, 'match_info': info}, '127.0.0.1', lobby['owner_token'])
    _, listed = directory.request('GET', '/v2/lobbies',
        dict(compatibility=[VERSION], mode=[mode]), {}, '127.0.0.1', '')
    assert listed['lobbies'][0]['map_name'] == 'Time to Split'
    assert 'owner_token' not in listed['lobbies'][0] and 'peers' not in listed['lobbies'][0]
    for _ in range(capacity-1):
        directory.request('POST', path+'/join', {}, body, '127.0.0.1', '')
    with pytest.raises(LobbyError) as error:
        directory.request('POST', path+'/join', {}, body, '127.0.0.1', '')
    assert error.value.status == 409
    now[0] = 91
    _, listed = directory.request('GET', '/v2/lobbies',
        dict(compatibility=[VERSION], mode=[mode]), {}, '127.0.0.1', '')
    assert listed['lobbies'] == []


def test_tls_pin_and_failed_private_start_do_not_leave_a_listener(tmp_path):
    directory = LocalDirectory(tmp_path, '192.0.2.1', 0)
    try:
        with pytest.raises(ValueError, match='fingerprint'):
            Client(directory.address, '0'*64).request('GET', '/v2/lobbies?compatibility='+VERSION)
        with pytest.raises(DirectoryError) as error:
            Client(directory.address, directory.fingerprint).request('GET', '/v3/lobbies?compatibility='+VERSION)
        assert error.value.status == 404
    finally:
        directory.close()
    directory.close()
    assert directory.poll() == 0
