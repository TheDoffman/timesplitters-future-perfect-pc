"""Three real relayed bridges and v3 HTTPS with native-fixture checkpoints.

The native game engine is covered separately by compiled adapter/codec fixtures;
these socket drivers exercise the companion and authenticated migration service.
"""
import socket
import struct
import subprocess
import time
import zlib
import threading
from types import SimpleNamespace

import pytest

from online_arcade_client import (ArcadeSession, PROGRESS, STATE, FRAGMENT, DOWNLOAD, ACK,
                                  CHUNK, NATIVE_HEADER)
from online_transport import LOCAL, LOCAL_MAGIC, virtual_ip
from test_online import VERSION, game_socket, send_game, receive_game
from test_online_relay import shared_service
from test_arcade_checkpoint import compile_arcade_fixture
from online_client import Session, DirectoryError
from online_arcade_client import list_arcade, join_arcade


@pytest.mark.parametrize('failure', ['once','persistent','forbidden'])
def test_heartbeat_retries_inside_native_lease_with_current_progress(failure):
    from online_arcade_client import NativeArcade
    now=[0.]
    bridge=SimpleNamespace(clock=lambda:now[0],peer_id=1,stopped=threading.Event())
    native=NativeArcade(bridge)
    native.status=dict(epoch=2,host_peer=1,phase='active')
    native.counter=120;native.active=True;native.native_peer=0
    native.offer=b'old checkpoint';native.ack=(1,'00'*32,False)
    native.install=lambda reply:None
    session=SimpleNamespace(bridge=bridge,native=native,closed=threading.Event(),
        compatibility=VERSION,endpoint=lambda:('/member','private'),error='')
    calls=[]
    def request(method,path,body,token,*,timeout):
        calls.append(dict(body));assert timeout==1
        now[0]+=timeout
        if len(calls)==1:native.counter=180
        if failure=='forbidden':raise DirectoryError(403,'denied')
        if failure=='persistent' or len(calls)==1:raise TimeoutError('TLS timeout')
        session.closed.set()
        return dict(host_peer=1)
    session.client=SimpleNamespace(request=request)
    ArcadeSession.heartbeat(session)
    assert calls[0]['epoch']==2 and 'checkpoint' in calls[0]
    assert calls[1]==dict(compatibility=VERSION,epoch=-1,native_counter=180,
                          active=True,native_peer=0)
    assert bridge.stopped.is_set() is (failure!='once')
    assert len(calls)==(4 if failure=='persistent' else 2)


@pytest.mark.parametrize('resumes', [False, True])
def test_round_start_gives_loading_guest_a_lease_without_faking_native_progress(resumes):
    from lobby_server import Directory
    now=[100.]
    service=Directory(clock=lambda:now[0]).arcade
    members={peer:service.member() for peer in (1,2)}
    members[1].update(native_counter=20,active=True,native_peer=0)
    members[2].update(native_counter=10,active=False,native_peer=1)
    room=dict(host_peer=1,members=members,epoch=1,migration=None,closed=False,previous_host=None)
    now[0]=120.  # Neither native engine pumps its frame during level loading.
    members[1]['native_at']=now[0]  # Host's first post-load pulse starts the round.
    service.start_match(room)
    migration=room['migration']
    migration.poll()  # A relayed packet can poll before the guest's next HTTPS RPC.
    assert set(migration.members)=={1,2}
    assert not migration.members[2].can_host
    assert members[2]['native_at']==100. and members[2]['native_counter']==10
    now[0]=120.5
    service.update_native(room,2,dict(native_counter=11 if resumes else 10,
        active=resumes,native_peer=1,epoch=room['epoch']))
    assert migration.members[2].can_host is resumes
    now[0]=124.25
    migration.heartbeat(1,migration.epoch)
    migration.poll()
    assert (2 in migration.members) is resumes


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
        self.backup = True
        self.restored = None

    def send(self, payload):
        bridge = self.active.bridge
        header = LOCAL.pack(LOCAL_MAGIC, bridge.compatibility, bridge.local_token,
            virtual_ip(bridge.peer_id), virtual_ip(bridge.peer_id), 0, 0)
        self.socket.sendto(header+payload, bridge.local.getsockname())

    def step(self):
        self.counter += 1
        self.send(PROGRESS.pack(b'AMPG', self.counter, self.playing, self.active.bridge.peer_id-1))
        for _ in range(128):
            try:
                packet = self.socket.recv(4096)
            except BlockingIOError:
                break
            payload = packet[LOCAL.size:]
            if payload[:4] == b'AMST':
                state = STATE.unpack_from(payload)
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
            if not preparing and not self.backup:
                return
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


@pytest.mark.parametrize('with_relay', [False, True])
def test_browser_can_list_and_join_explicit_legacy_arcade_rooms(tmp_path, with_relay):
    with shared_service(tmp_path/'legacy-directory') as (client, directory, relay):
        if not with_relay:
            directory.relay_address = None
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as available:
            available.bind(('127.0.0.1', 0))
            port = available.getsockname()[1]
        host = Session.create(client, VERSION, 'Explicit legacy room', port, True)
        guest = None
        try:
            assert [room['code'] for room in list_arcade(client, VERSION)['lobbies']] == [host.lobby['code']]
            guest = join_arcade(client, VERSION, host.lobby['code'])
            assert not isinstance(guest, ArcadeSession)
            assert not guest.bridge.relay and guest.bridge.ready.is_set()
        finally:
            if guest:
                guest.close()
            host.close()


@pytest.mark.parametrize('failure', [DirectoryError(403, 'denied'), DirectoryError(409, 'match in progress'),
                                   ValueError('certificate mismatch')])
def test_arcade_join_never_downgrades_authentication_or_match_errors(failure):
    class RejectedDirectory:
        calls = 0
        def request(self, *args, **kwargs):
            self.calls += 1
            raise failure
    directory = RejectedDirectory()
    with pytest.raises(ValueError, match=str(failure)):
        join_arcade(directory, VERSION, 'A'*16)
    assert directory.calls == 1


@pytest.mark.parametrize('host_loss', ['companion_crash', 'engine_stall'])
def test_graceful_then_crash_migration_preserves_checkpoint_and_relays_between_survivors(tmp_path, native_checkpoint, host_loss):
    with shared_service(tmp_path/'directory') as (client, directory, relay):
        sessions, natives, games = [], [], []
        try:
            host = ArcadeSession.open(client, VERSION, name='Migrating arcade')
            sessions.append(host)
            for _ in range(2):
                sessions.append(ArcadeSession.open(client, VERSION, code=host.lobby['code']))
            natives = [NativeDriver(active) for active in sessions]
            # Make peer 2 the sole initial backup so network scheduling cannot
            # change this test's chosen successor. Peer 3 still restores/ACKs
            # preparation, then becomes a backup for the second migration.
            natives[2].backup = False
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
            # Reorder leave before the upload: an older backed-up frame must
            # never be mistaken for the requested final sequence.
            natives[0].send(struct.pack('<4sI', b'AMLV', 2))
            pause = time.monotonic()+.5
            until(natives, lambda: time.monotonic() >= pause)
            assert not host.native.departed
            expected = natives[0].offer(native_checkpoint, 2, 3601, score=45)
            until(natives, lambda: host.native.departed and all(active.native.status['phase'] == 'active' and
                active.native.status['host_peer'] == 2 for active in sessions[1:]) and sessions[1].host)
            assert all(driver.restored == expected for driver in natives[1:])
            assert sessions[1].host and not sessions[2].host
            natives[2].backup = True
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
            # Either the entire host disappears, or its engine stops making
            # progress while its companion still sends authenticated HTTPS.
            crashed = sessions[1]
            if host_loss == 'companion_crash':
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
            # Finish/rematch preserves the elected owner and member identity.
            # Idle lobby metadata cannot replace a nonce under the same epoch.
            previous_match = sessions[2].native.status['match_id']
            natives[2].playing = False
            until(natives[2:], lambda: sessions[2].native.status['phase'] == 'lobby')
            assert sessions[2].native.status['epoch'] == 4
            assert sessions[2].native.status['match_id'] == previous_match
            natives[2].playing = True
            until(natives[2:], lambda: sessions[2].native.status['phase'] == 'active' and
                  sessions[2].native.status['epoch'] == 5)
            assert sessions[2].native.status['host_peer'] == 3
            assert sessions[2].native.status['match_id'] != previous_match
        finally:
            for driver in natives:
                driver.close()
            for game in games:
                game.close()
            for active in reversed(sessions):
                active.close()
        assert not directory.arcade.rooms
