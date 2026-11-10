"""Shared relay acceptance with real HTTPS/UDP and isolated game-side sockets."""
from contextlib import contextmanager
import hmac
import json
import os
from pathlib import Path
import subprocess
import sys
import socket
import ssl
import threading
import time

import pytest

from lobby_server import Directory, Server, RelayServer, LobbyError, MAX_RELAY_PACKET
from online_client import Client, Session
from online_host import certificate, invitation, parse_invitation
from online_transport import (LOCAL, WIRE, WIRE_MAGIC, DATA, RELAY, REGISTER,
                              FORWARD, relay_packet, virtual_ip)
from test_online import game_socket, send_game, receive_game, VERSION


@contextmanager
def shared_service(tmp_path):
    directory = Directory()
    relay = RelayServer(directory)
    directory.relay_address = relay.socket.getsockname()
    with Server(('127.0.0.1', 0), directory) as server:
        cert, key, pin = certificate(tmp_path/'certificate')
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(cert, key)
        server.socket = context.wrap_socket(server.socket, server_side=True, do_handshake_on_connect=False)
        threads = [threading.Thread(target=target, daemon=True)
                   for target in (relay.run, server.serve_forever)]
        for thread in threads:
            thread.start()
        try:
            yield Client(f'https://127.0.0.1:{server.server_port}', pin), directory, relay
        finally:
            relay.close()
            server.shutdown()
            for thread in threads:
                thread.join(timeout=3)
                assert not thread.is_alive()


class OutboundOnly:
    """Fail the test if a player sends directly to any other player's socket."""
    def __init__(self, sock, allowed):
        self.socket, self.allowed = sock, allowed

    def sendto(self, packet, endpoint):
        assert endpoint == self.allowed
        return self.socket.sendto(packet, endpoint)

    def __getattr__(self, name):
        return getattr(self.socket, name)


@pytest.mark.parametrize('mode', ['arcade', 'story'])
def test_player_host_and_two_guests_use_only_shared_relay(tmp_path, mode):
    with shared_service(tmp_path) as (client, directory, relay):
        sessions, games = [], []
        # The legacy host port is occupied: relayed hosting must ignore it.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
            occupied.bind(('0.0.0.0', 0))
            try:
                host = Session.create(client, VERSION, 'Shared relay', occupied.getsockname()[1], True,
                                      mode=mode, transport='relay')
                sessions.append(host)
                assert host.bridge.peer_id == 1 and host.host
                assert host.bridge.ready.is_set() and host.bridge.relay_ready.is_set()
                assert host.lobby['port'] == relay.socket.getsockname()[1]
                assert host.bridge.wan.getsockname()[1] != occupied.getsockname()[1]
                listing = client.request('GET', '/v2/lobbies?compatibility='+VERSION+'&mode='+mode)['lobbies']
                assert [l['code'] for l in listing] == [host.lobby['code']]
                assert not {'owner_token', 'relay_key', 'peer_key', 'origin'} & listing[0].keys()
                assert parse_invitation(invitation(client.url.geturl(), client.fingerprint,
                                                  host.lobby['code']))[2] == host.lobby['code']
                sessions.extend(Session.join(client, VERSION, host.lobby['code'], mode=mode) for _ in range(2))
                for active in sessions:
                    active.bridge.wan = OutboundOnly(active.bridge.wan, directory.relay_address)
                    games.append(game_socket(active.bridge, 1000))
                send_game(games[1], sessions[1].bridge, virtual_ip(1), b'guest to player host')
                assert receive_game(games[0])[LOCAL.size:] == b'guest to player host'
                send_game(games[0], host.bridge, virtual_ip(2), b'host simulation update')
                assert receive_game(games[1])[LOCAL.size:] == b'host simulation update'
                before = relay.forwarded
                send_game(games[1], sessions[1].bridge, virtual_ip(3), b'via the player host')
                assert receive_game(games[2])[LOCAL.size:] == b'via the player host'
                assert relay.forwarded >= before+2
                send_game(games[2], sessions[2].bridge, b'\xff'*4, b'discovery')
                assert receive_game(games[0])[LOCAL.size:] == b'discovery'
                assert receive_game(games[1])[LOCAL.size:] == b'discovery'
                # Even a valid relay credential cannot forge the game host's signature.
                guest = sessions[1].bridge
                rejected = host.bridge.dropped
                body = WIRE.pack(WIRE_MAGIC, bytes.fromhex(VERSION), host.bridge.room,
                                 10000, 3, 1, 1000, 1000)+bytes((DATA,))+b'forged guest'
                guest.send_relay(FORWARD, 1, body+hmac.digest(guest.key, body, 'sha256'))
                deadline = time.monotonic()+2
                while host.bridge.dropped == rejected and time.monotonic() < deadline:
                    time.sleep(.01)
                assert host.bridge.dropped > rejected
                send_game(games[1], guest, virtual_ip(1), b'after rejection')
                assert receive_game(games[0])[LOCAL.size:] == b'after rejection'
                # The latency display still measures the full path to the game host.
                deadline = time.monotonic()+4
                while any(s.bridge.latency_snapshot()[1] == 0 for s in sessions) and time.monotonic() < deadline:
                    time.sleep(.01)
                assert all(s.bridge.latency_snapshot()[0] >= 0 for s in sessions)
                path, token = sessions[1].endpoint()
                roster = client.request('PUT', '/v2/lobbies/'+host.lobby['code'],
                                        dict(compatibility=VERSION, mode=mode), host.lobby['owner_token'])
                assert 'relay_key' not in roster and 'relay_key' not in str(roster['peers'])
                assert client.request('PUT', path, dict(compatibility=VERSION, mode=mode), token)['transport'] == 'relay'
            finally:
                for game in games:
                    game.close()
                for active in reversed(sessions):
                    active.close()
        assert not directory.online


def test_relay_credentials_replay_room_isolation_revocation_and_limits():
    now = [0.0]
    directory = Directory(clock=lambda: now[0], relay_address=('127.0.0.1', 30821))
    relay = RelayServer(directory, bytes_per_second=MAX_RELAY_PACKET,
                        total_bytes_per_second=MAX_RELAY_PACKET*10)
    sockets = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(3)]
    for sock in sockets:
        sock.bind(('127.0.0.1', 0))
        sock.settimeout(.2)
    try:
        def create():
            return directory.request('POST', '/v2/lobbies', {}, dict(
                compatibility=VERSION, name='Test', port=30821, transport='relay'), '127.0.0.1', '')[1]
        lobby, other = create(), create()
        path = '/v2/lobbies/'+lobby['code']
        peer = directory.request('POST', path+'/join', {}, dict(compatibility=VERSION), '', '')[1]
        room = bytes.fromhex(lobby['room'])
        host_key, peer_key = (bytes.fromhex(value['relay_key']) for value in (lobby, peer))
        def packet(seq, source=2, target=1, kind=FORWARD, key=peer_key, payload=b'data', room_id=room):
            return relay_packet(room_id, seq, source, target, kind, key, payload)
        for source, key, sock in [(1, host_key, sockets[0]), (2, peer_key, sockets[1])]:
            relay.handle(packet(1, source, 0, REGISTER, key, b''), sock.getsockname())
            ack = sock.recv(8192)
            assert RELAY.unpack_from(ack)[-1] == 2
        def rejected(data, endpoint=sockets[1].getsockname()):
            before = relay.dropped
            relay.handle(data, endpoint)
            assert relay.dropped == before+1
        rejected(packet(2, key=bytes(32)))
        rejected(packet(2, source=1))
        rejected(packet(2, target=3))  # Never allow guest-to-guest bypass of the host.
        rejected(packet(2, room_id=bytes.fromhex(other['room'])))
        rejected(packet(2), sockets[2].getsockname())  # No unregistered endpoint changes.
        data = packet(2)
        relay.handle(data, sockets[1].getsockname())
        assert sockets[0].recv(8192)[RELAY.size:-32] == b'data'
        rejected(data)  # Exact replay.
        rejected(packet(3, payload=b'x'*(MAX_RELAY_PACKET+1)))
        for seq in range(3, 20):
            relay.handle(packet(seq, payload=b'x'*1024), sockets[1].getsockname())
        assert relay.forwarded < 18  # Sender cannot exceed its token bucket.
        now[0] += 1
        # Authenticated re-registration can recover a changed NAT mapping.
        relay.handle(packet(30, target=0, kind=REGISTER, payload=b''), sockets[2].getsockname())
        sockets[2].recv(8192)
        directory.request('DELETE', path+'/peers/2', {}, {}, '', peer['peer_key'])
        rejected(packet(31), sockets[2].getsockname())
        directory.request('DELETE', path, {}, {}, '', lobby['owner_token'])
        rejected(packet(32, source=1, key=host_key), sockets[0].getsockname())
        now[0] = 91
        rejected(relay_packet(bytes.fromhex(other['room']), 1, 1, 0, REGISTER,
                              bytes.fromhex(other['relay_key'])), sockets[0].getsockname())
    finally:
        relay.close()
        for sock in sockets:
            sock.close()


def test_relay_required_never_silently_falls_back(tmp_path):
    directory = Directory()
    with pytest.raises(LobbyError) as failure:
        directory.request('POST', '/v2/lobbies', {}, dict(
            compatibility=VERSION, name='Test', port=30821, transport='relay'), '127.0.0.1', '')
    assert failure.value.status == 503 and not directory.online
    class OldDirectory:
        deleted = False
        def request(self, method, path, body=None, token=None):
            if method == 'DELETE':
                self.deleted = True
                return {}
            return dict(code='A'*16, room='0'*32, owner_token='owner', mode='arcade', capacity=16)
    client = OldDirectory()
    with pytest.raises(ValueError, match='does not support the shared relay'):
        Session.create(client, VERSION, 'Test', 30821, True, transport='relay')
    assert client.deleted


def test_lan_and_public_clients_receive_reachable_relay_addresses():
    directory = Directory(relay_address=('203.0.113.10', 30821), relay_lan_address='192.168.0.27')
    _, lobby = directory.request('POST', '/v2/lobbies', {}, dict(
        compatibility=VERSION, name='Test', port=30821, public=True, transport='relay'), '192.168.0.20', '')
    assert lobby['address'] == '192.168.0.27'
    for remote, expected in [('192.168.0.30', '192.168.0.27'), ('8.8.8.8', '203.0.113.10')]:
        _, listing = directory.request('GET', '/v2/lobbies', dict(compatibility=[VERSION]), {}, remote, '')
        assert listing['lobbies'][0]['address'] == expected
        _, peer = directory.request('POST', '/v2/lobbies/'+lobby['code']+'/join', {},
                                    dict(compatibility=VERSION), remote, '')
        assert peer['address'] == expected


def test_service_defaults_and_public_ca_invitation(tmp_path, monkeypatch):
    import online_paths
    monkeypatch.setattr(online_paths, 'TOOLS', tmp_path)
    monkeypatch.delenv('TSFP_ONLINE_DIRECTORY', raising=False)
    monkeypatch.delenv('TSFP_ONLINE_FINGERPRINT', raising=False)
    (tmp_path/'online-service.json').write_text('{"directory":"https://example.test:30820","fingerprint":""}')
    assert online_paths.service_defaults()['directory'] == 'https://example.test:30820'
    assert parse_invitation(invitation('https://example.test:30820', '', 'A'*16)) == (
        'https://example.test:30820', '', 'A'*16)
    monkeypatch.setenv('TSFP_ONLINE_DIRECTORY', 'https://other.test')
    assert online_paths.service_defaults()['directory'] == 'https://other.test'


def test_installer_prepares_a_working_isolated_server_and_preserves_certificate(tmp_path):
    from install_online_service import prepare
    with socket.socket() as tcp, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        tcp.bind(('127.0.0.1', 0))
        udp.bind(('127.0.0.1', 0))
        directory_port, relay_port = tcp.getsockname()[1], udp.getsockname()[1]
    root, plist = prepare(tmp_path/'service', '127.0.0.1', directory_port=directory_port, relay_port=relay_port)
    public = json.loads((root/'client-service.json').read_text())
    original_key = (root/'certificate/private-key.pem').read_bytes()
    _, upgraded = prepare(root, '127.0.0.1', directory_port=directory_port, relay_port=relay_port)
    assert upgraded == plist
    assert original_key == (root/'certificate/private-key.pem').read_bytes()
    if os.name == 'posix':
        assert (root/'certificate/private-key.pem').stat().st_mode & 0o777 == 0o600
    process = subprocess.Popen(plist['ProgramArguments'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        client = Client(public['directory'], public['fingerprint'])
        deadline = time.monotonic()+5
        while True:
            try:
                assert client.request('GET', '/v2/lobbies?compatibility='+VERSION) == {'lobbies': []}
                break
            except OSError:
                if process.poll() is not None:
                    stdout, stderr = process.communicate()
                    pytest.fail(f'Installed server exited: {stdout!r} {stderr!r}')
                if time.monotonic() >= deadline:
                    raise
                time.sleep(.05)
        host = Session.create(client, VERSION, 'Installed server', 30821, True, transport='relay')
        try:
            assert host.bridge.ready.is_set()
        finally:
            host.close()
    finally:
        process.terminate()
        process.communicate(timeout=5)
    occupied = tmp_path/'unrelated'
    occupied.mkdir()
    (occupied/'keep.txt').write_text('preserve')
    with pytest.raises(ValueError, match='not an existing TimeSplitters'):
        prepare(occupied, '127.0.0.1')
    assert (occupied/'keep.txt').read_text() == 'preserve'
