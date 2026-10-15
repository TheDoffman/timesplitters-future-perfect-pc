"""Exercise the directory, handshake and host relay with real loopback sockets."""
from pathlib import Path
import socket
import sys
import threading
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from lobby_server import Directory, Server, LobbyError
from online_client import Client, Session
from online_transport import LOCAL, LOCAL_MAGIC, WIRE, WIRE_MAGIC, DATA, virtual_ip
import hmac

VERSION = 'a'*64


def game_socket(bridge, port):
    game = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    game.bind(('127.0.0.1', 0))
    game.settimeout(2)
    game.sendto(LOCAL.pack(LOCAL_MAGIC, bytes.fromhex(VERSION), bridge.local_token,
                          virtual_ip(bridge.peer_id), bytes(4), port, 0), bridge.local.getsockname())
    deadline = time.monotonic()+2
    while port not in bridge.ports and time.monotonic() < deadline:
        time.sleep(.01)
    assert port in bridge.ports
    return game


def send_game(game, bridge, target, payload=b'game packet'):
    game.sendto(LOCAL.pack(LOCAL_MAGIC, bytes.fromhex(VERSION), bridge.local_token,
                          virtual_ip(bridge.peer_id), target, 1000, 1000)+payload, bridge.local.getsockname())


def test_directory_handshake_and_three_pc_relay():
    directory = Directory()
    with Server(('127.0.0.1', 0), directory) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        client = Client(f'http://127.0.0.1:{server.server_port}')
        port_probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        port_probe.bind(('127.0.0.1', 0))
        port = port_probe.getsockname()[1]
        port_probe.close()
        sessions, games = [], []
        try:
            host = Session.create(client, VERSION, 'Test room', port, True)
            sessions.append(host)
            assert client.request('GET', '/v2/lobbies?compatibility='+'b'*64) == {'lobbies': []}
            listing = client.request('GET', '/v2/lobbies?compatibility='+VERSION)['lobbies']
            assert len(listing) == 1 and 'owner_token' not in listing[0] and 'peer_key' not in listing[0]
            with pytest.raises(ValueError, match='Incompatible'):
                Session.join(client, 'b'*64, host.lobby['code'])
            first = Session.join(client, VERSION, host.lobby['code'])
            sessions.append(first)
            second = Session.join(client, VERSION, host.lobby['code'])
            sessions.append(second)
            for session in sessions:
                games.append(game_socket(session.bridge, 1000))
            send_game(games[0], host.bridge, virtual_ip(first.bridge.peer_id))
            packet = games[1].recv(8192)
            assert packet[LOCAL.size:] == b'game packet' and LOCAL.unpack_from(packet)[3] == virtual_ip(1)
            send_game(games[1], first.bridge, virtual_ip(second.bridge.peer_id), b'guest to guest')
            packet = games[2].recv(8192)
            assert packet[LOCAL.size:] == b'guest to guest'
            assert LOCAL.unpack_from(packet)[3] == virtual_ip(first.bridge.peer_id)
            send_game(games[2], second.bridge, b'\xff'*4, b'discovery')
            assert games[0].recv(8192)[LOCAL.size:] == b'discovery'
            assert games[1].recv(8192)[LOCAL.size:] == b'discovery'
            # A guest cannot sign as another guest using its own credential.
            rejected = host.bridge.dropped
            body = WIRE.pack(WIRE_MAGIC, bytes.fromhex(VERSION), host.bridge.room,
                             10000, first.bridge.peer_id, 1, 1000, 1000)+bytes((DATA,))+b'forged'
            second.bridge.wan.sendto(body+hmac.digest(second.bridge.key, body, 'sha256'), second.bridge.host)
            time.sleep(.1)
            assert host.bridge.dropped > rejected
            rejected = host.bridge.dropped
            replay = WIRE.pack(WIRE_MAGIC, bytes.fromhex(VERSION), host.bridge.room,
                               0, first.bridge.peer_id, 1, 1000, 1000)+bytes((DATA,))+b'old sequence'
            replay += hmac.digest(first.bridge.key, replay, 'sha256')
            first.bridge.wan.sendto(replay, first.bridge.host)
            first.bridge.wan.sendto(replay, first.bridge.host)
            time.sleep(.1)
            assert host.bridge.dropped >= rejected+2
            send_game(games[1], first.bridge, virtual_ip(1), b'after rejection')
            assert games[0].recv(8192)[LOCAL.size:] == b'after rejection'
        finally:
            for game in games:
                game.close()
            for session in reversed(sessions):
                session.close()
            server.shutdown()
            thread.join(timeout=3)
        assert not directory.online


def test_directory_public_address_and_private_compatibility():
    directory = Directory(public_address='203.0.113.7')
    _, lobby = directory.request('POST', '/v2/lobbies', {}, dict(
        compatibility=VERSION, name='Private', port=30821, public=False), '127.0.0.1', '')
    assert lobby['address'] == '203.0.113.7'
    assert directory.request('GET', '/v2/lobbies', {'compatibility': [VERSION]}, {}, '', '')[1] == {'lobbies': []}
    with pytest.raises(LobbyError) as failure:
        directory.request('POST', '/v2/lobbies/'+lobby['code']+'/join', {}, dict(compatibility='b'*64), '', '')
    assert failure.value.status == 409
    assert not directory.online[lobby['code']]['peers']
    _, peer = directory.request('POST', '/v2/lobbies/'+lobby['code']+'/join', {},
                                dict(compatibility=VERSION), '', '')
    with pytest.raises(LobbyError) as failure:
        directory.request('PUT', '/v2/lobbies/'+lobby['code']+'/peers/'+str(peer['peer_id']),
                          {}, dict(compatibility=VERSION), '', '\u00e9')
    assert failure.value.status == 403


def test_remote_plaintext_and_invalid_pin_rejected():
    with pytest.raises(ValueError, match='HTTPS'):
        Client('http://203.0.113.7:30820')
    with pytest.raises(ValueError, match='fingerprint'):
        Client('https://example.test:30820', 'short')
