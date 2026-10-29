"""Exercise the directory, handshake and host relay with real loopback sockets."""


from pathlib import Path
import socket
import sys
import threading
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from lobby_server import Directory, Server, LobbyError
from online_client import Client, Session
from online_transport import LOCAL, LOCAL_MAGIC, WIRE, WIRE_MAGIC, DATA, virtual_ip
import hmac

VERSION = 'a'*64


def receive_game(game):
    # The native bridge consumes authenticated telemetry before title traffic.
    while True:
        packet = game.recv(8192)
        if LOCAL.unpack_from(packet)[-1] != 0:
            return packet


def test_latency_requires_authenticated_matching_nonce_and_expires():
    from online_transport import Bridge, PONG
    clock = [100.0]
    host = ('127.0.0.1', 30821)
    key = bytes.fromhex('b'*64)
    bridge = Bridge(VERSION, 'c'*32, 2, key.hex(), host, clock=lambda: clock[0])
    nonce = b'12345678'
    try:
        bridge.pending_pings[1] = (nonce, clock[0])
        clock[0] += 1.25
        bridge.ready.set()
        bridge.last_hello = clock[0]
        bridge.poll(0)
        assert bridge.pending_pings[1][0] == nonce  # Slow replies survive the next probe interval.
        def reply(sequence, payload, signing_key=key):
            body = WIRE.pack(WIRE_MAGIC, bytes.fromhex(VERSION), bridge.room, sequence, 1, 2, 0, 0)+bytes((PONG,))+payload
            return body+hmac.digest(signing_key, body, 'sha256')
        bridge.from_network(reply(1, nonce, bytes(32)), host)
        assert bridge.latency_snapshot() == (-1, 0)
        bridge.from_network(reply(2, b'wrong123'), host)
        assert bridge.latency_snapshot() == (-1, 0)
        bridge.from_network(reply(3, nonce), host)
        milliseconds, peers = bridge.latency_snapshot()
        assert peers == 1 and milliseconds == pytest.approx(1250)
        clock[0] += 6
        bridge.from_network(reply(3, nonce), host)  # Replay cannot refresh a stale sample.
        assert bridge.latency_snapshot() == (-1, 0)
        bridge.from_network(reply(4, nonce), host)  # Unsolicited pong cannot refresh it either.
        assert bridge.latency_snapshot() == (-1, 0)
    finally:
        bridge.close()


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
            deadline = time.monotonic()+4
            # Each guest seeing the host does not mean the host has received
            # both authenticated replies yet. Wait for the condition below.
            while (host.bridge.latency_snapshot()[1] < 2 or
                   any(session.bridge.latency_snapshot()[1] == 0 for session in sessions)) and time.monotonic()<deadline:
                time.sleep(.01)
            assert all(session.bridge.latency_snapshot()[0] >= 0 for session in sessions)
            assert host.bridge.latency_snapshot()[1] == 2
            send_game(games[0], host.bridge, virtual_ip(first.bridge.peer_id))
            packet = receive_game(games[1])
            assert packet[LOCAL.size:] == b'game packet' and LOCAL.unpack_from(packet)[3] == virtual_ip(1)
            send_game(games[1], first.bridge, virtual_ip(second.bridge.peer_id), b'guest to guest')
            packet = receive_game(games[2])
            assert packet[LOCAL.size:] == b'guest to guest'
            assert LOCAL.unpack_from(packet)[3] == virtual_ip(first.bridge.peer_id)
            send_game(games[2], second.bridge, b'\xff'*4, b'discovery')
            assert receive_game(games[0])[LOCAL.size:] == b'discovery'
            assert receive_game(games[1])[LOCAL.size:] == b'discovery'
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
            assert receive_game(games[0])[LOCAL.size:] == b'after rejection'
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
