"""Three real relayed bridges and v3 HTTPS with native-fixture checkpoints.

The native game engine is covered separately by compiled adapter/codec fixtures;
these socket drivers exercise the companion and authenticated migration service.
"""
import socket
import struct
import subprocess
import time
import zlib

import pytest

from online_arcade_client import (ArcadeSession, PROGRESS, STATE, FRAGMENT, DOWNLOAD, ACK,
                                  CHUNK, NATIVE_HEADER)
from online_transport import LOCAL, LOCAL_MAGIC, virtual_ip
from test_online import VERSION, game_socket, send_game, receive_game
from test_online_relay import shared_service
from test_arcade_checkpoint import compile_arcade_fixture


class NativeDriver:
    def __init__(self, active):
        self.active = active
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1', 0))
        self.socket.setblocking(False)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 262144)
        self.counter = 0
        self.playing = False
        self.state = None
        self.data = None
        self.received = set()
        self.snapshot = None
        self.digest = None
        self.ready = True
        self.restored = None

    def send(self, payload):
        bridge = self.active.bridge
        header = LOCAL.pack(LOCAL_MAGIC, bridge.compatibility, bridge.local_token,
            virtual_ip(bridge.peer_id), virtual_ip(bridge.peer_id), 0, 0)
        self.socket.sendto(header+payload, bridge.local.getsockname())

    def step(self):
        self.counter += 1
        self.send(PROGRESS.pack(b'AMPG', self.counter, self.playing))
        for _ in range(128):
            try:
                packet = self.socket.recv(4096)
            except BlockingIOError:
                break
            payload = packet[LOCAL.size:]
            if payload[:4] == b'AMST':
                state = STATE.unpack(payload)
                if self.state and state[2] < self.state[2]:
                    continue
                self.state = state
            elif payload[:4] == b'AMFR':
                _, sequence, total, offset, crc, digest = DOWNLOAD.unpack_from(payload)
                identity = sequence, total, crc, digest
                if self.digest != identity:
                    self.digest = identity;self.data = bytearray(total);self.received.clear()
                    self.snapshot = None
                part = payload[DOWNLOAD.size:]
                self.data[offset:offset+len(part)] = part
                self.received.add(offset)
                if len(self.received) == (total+CHUNK-1)//CHUNK:
                    assert zlib.crc32(self.data) == crc
                    self.snapshot = bytes(self.data)
        if self.state and self.digest and self.snapshot and self.digest[0] == self.state[4]:
            preparing = self.state[3] == 2
            if preparing and self.ready:
                self.restored = self.snapshot
            self.send(ACK.pack(b'AMAK', self.digest[0], self.digest[3], preparing and self.ready))

    def offer(self, template, sequence, tick, score=None):
        reply = self.active.native.status
        payload = bytearray(template)
        struct.pack_into('<I', payload, 12, sequence)
        payload[16:32] = bytes.fromhex(reply['match_id'])
        struct.pack_into('<I', payload, 32, reply['epoch'])
        struct.pack_into('<I', payload, 40, tick)
        # Native fixture exports the exact C ABI. Header/config/score fields are
        # fixed 32-bit aligned; bot records contain no pointers or 64-bit fields.
        bot_base = 48+160+16*84+16*4+16*4+4
        stride = (len(payload)-bot_base)//10
        assert bot_base+stride*10 == len(payload)
        for bot in range(6):
            struct.pack_into('<I', payload, bot_base+bot*stride+4, tick)
        if score is not None:
            struct.pack_into('<i', payload, 48+160+16*84, score)
        crc = zlib.crc32(payload)
        for offset in range(0, len(payload), CHUNK):
            self.send(FRAGMENT.pack(b'AMCP', sequence, len(payload), offset, crc)+payload[offset:offset+CHUNK])
        return bytes(payload)

    def close(self):
        self.socket.close()


def until(drivers, condition, seconds=12):
    deadline = time.monotonic()+seconds
    while time.monotonic() < deadline:
        for driver in drivers:
            driver.step()
        if condition():
            return
        time.sleep(.01)
    assert condition(), [(driver.active.error, driver.active.native.status.get('phase'),
                          driver.active.native.status.get('epoch')) for driver in drivers]


@pytest.fixture
def native_checkpoint(tmp_path):
    executable = compile_arcade_fixture(tmp_path)
    output = tmp_path/'native-checkpoint.bin'
    subprocess.run([str(executable), str(output)], check=True, capture_output=True, timeout=10)
    return output.read_bytes()


def test_graceful_then_crash_migration_preserves_checkpoint_and_relays_between_survivors(tmp_path, native_checkpoint):
    with shared_service(tmp_path/'directory') as (client, directory, relay):
        sessions, natives, games = [], [], []
        try:
            host = ArcadeSession.open(client, VERSION, name='Migrating arcade')
            sessions.append(host)
            for _ in range(2):
                sessions.append(ArcadeSession.open(client, VERSION, code=host.lobby['code']))
            natives = [NativeDriver(active) for active in sessions]
            games = [game_socket(active.bridge, 1000) for active in sessions]
            until(natives, lambda: all(driver.state is not None for driver in natives))
            for driver in natives:
                driver.playing = True
            until(natives, lambda: all(active.native.status['phase'] == 'active' and
                active.native.status['epoch'] == 2 for active in sessions))
            expected = natives[0].offer(native_checkpoint, 1, 3600, score=44)
            until(natives, lambda: all(driver.snapshot == expected for driver in natives) and
                  host.native.status.get('backup_ready'))
            # A final, frozen capture is acknowledged before the host departs.
            expected = natives[0].offer(native_checkpoint, 2, 3601, score=45)
            natives[0].send(b'AMLV')
            until(natives, lambda: host.native.departed and all(active.native.status['phase'] == 'active' and
                active.native.status['host_peer'] == 2 for active in sessions[1:]) and sessions[1].host)
            assert all(driver.restored == expected for driver in natives[1:])
            assert sessions[1].host and not sessions[2].host
            room = directory.arcade.rooms[host.lobby['code']]
            assert set(room['members']) == {2, 3} and room['epoch'] == 3
            until(natives[1:], lambda: all(active.bridge.ready.is_set() and
                  active.bridge.relay_ready.is_set() for active in sessions[1:]))
            send_game(games[2], sessions[2].bridge, virtual_ip(2), b'after first host leaves')
            assert receive_game(games[1])[LOCAL.size:] == b'after first host leaves'
            send_game(games[1], sessions[1].bridge, virtual_ip(3), b'new host reply')
            assert receive_game(games[2])[LOCAL.size:] == b'new host reply'
            survivors = natives[1:]
            expected = natives[1].offer(native_checkpoint, 3, 3660, score=46)
            until(survivors, lambda: all(driver.snapshot == expected for driver in survivors) and
                sessions[1].native.status.get('backup_ready'))
            # Abrupt loss: stop the companion without sending DELETE.
            crashed = sessions[1]
            crashed.closed.set();crashed.bridge.stopped.set()
            crashed.worker.join(timeout=6);crashed.thread.join(timeout=2);crashed.bridge.close()
            until(natives[2:], lambda: sessions[2].native.status['host_peer'] == 3 and
                sessions[2].native.status['phase'] == 'active', seconds=12)
            assert natives[2].restored == expected
            assert room['epoch'] == 4 and set(room['members']) == {3}
            assert NATIVE_HEADER.unpack_from(natives[2].restored)[7] == 3660
            assert struct.unpack_from('<i', natives[2].restored, 48+160+16*84)[0] == 46
            assert len(natives[2].restored) == len(native_checkpoint)
            # The old owner's bearer cannot control the continuing room.
            path, token = host.endpoint()
            with pytest.raises(ValueError, match='credential'):
                client.request('PUT', path, dict(compatibility=VERSION, epoch=4), token)
            listing = client.request('GET', '/v3/lobbies?compatibility='+VERSION)['lobbies']
            assert len(listing) == 1 and listing[0]['players'] == 1
            assert not {'peer_key', 'peer_token', 'relay_key', 'checkpoint', 'peers'} & listing[0].keys()
        finally:
            for driver in natives:
                driver.close()
            for game in games:
                game.close()
            for active in reversed(sessions):
                active.close()
        assert not directory.arcade.rooms
