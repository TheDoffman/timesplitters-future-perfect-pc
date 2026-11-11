"""Arcade migration companion: native loopback control and authenticated v3 RPC."""
import base64
import hashlib
import http.client
import ipaddress
import re
import struct
import threading
import time
import zlib

from online_client import Session, DirectoryError
SCHEMA = 'tsfp-arcade-checkpoint-v1'
from online_transport import Bridge, LOCAL, LOCAL_MAGIC, virtual_ip, relay_endpoint

CHUNK = 1200
FRAGMENT = struct.Struct('<4sIIII')
DOWNLOAD = struct.Struct('<4sIIII32s')
ACK = struct.Struct('<4sI32sI')
PROGRESS = struct.Struct('<4sIII')
SLOT_PROGRESS = struct.Struct('<4s7I')
STATE = struct.Struct('<4sIIII4s4s4sI32s16s')
NATIVE_HEADER = struct.Struct('<4I16s4I')
PHASE = {'lobby': 0, 'active': 1, 'preparing': 2, 'closed': 3}


def list_arcade(client, compatibility):
    """Browse migrating and explicitly self-hosted legacy Arcade rooms."""
    try:
        current = client.request('GET', '/v3/lobbies?compatibility='+compatibility)['lobbies']
    except DirectoryError as error:
        if error.status not in (404, 503):
            raise
        current = []
    legacy = client.request('GET', '/v2/lobbies?compatibility='+compatibility+'&mode=arcade')['lobbies']
    return dict(lobbies=current+legacy)


def join_arcade(client, compatibility, code):
    try:
        return ArcadeSession.open(client, compatibility, code=code)
    except DirectoryError as error:
        if error.status not in (404, 410, 503):
            raise
    # Automatic/self-hosted directories keep their original direct transport.
    # TLS, schema, authentication and active-match errors never trigger fallback.
    return Session.join(client, compatibility, code, mode='arcade')


class NativeArcade:
    def __init__(self, bridge):
        self.bridge = bridge
        self.lock = threading.RLock()
        self.endpoint = None
        self.status = None
        self.counter = -1
        self.active = False
        self.native_peer = None
        self.native_slots = None
        self.match_info = None
        self.roster_info = None
        self.roster_at = -10
        self.offer = None
        self.ack = None
        self.leaving = self.departed = False
        self.leave_sequence = None
        self.leaving_at = None
        self.assembly = None
        self.last_control = -10
        self.download_offset = 0

    def install(self, reply):
        with self.lock:
            prior = self.status
            changed = prior is None or prior['epoch'] != reply['epoch'] or prior['host_peer'] != reply['host_peer']
            bridge = self.bridge
            bridge.migration_frozen = reply['phase'] in ('preparing', 'closed')
            if changed:
                bridge.ready.clear()
                bridge.host_sequence = -1
                bridge.pending_pings.clear();bridge.latencies.clear()
                bridge.quality_reset()
                with bridge.lock:
                    bridge.peers.clear()
                self.offer = self.ack = self.assembly = None
                self.roster_info = None
            bridge.host_peer = reply['host_peer'] or reply.get('candidate') or bridge.host_peer
            bridge.host = relay_endpoint(bridge.host_peer)
            bridge.key = bytes.fromhex(reply['peer_key'])
            relay_key = bytes.fromhex(reply['relay_key'])
            if relay_key != bridge.relay_key:
                bridge.relay_key = relay_key
                bridge.relay_received_sequence = -1
                bridge.last_registration = -10
                bridge.relay_ready.clear()
            bridge.update_peers(reply.get('peers', {}))
            snapshot = reply.get('checkpoint')
            if snapshot:
                payload = base64.b64decode(snapshot['payload'], validate=True)
                if not NATIVE_HEADER.size <= len(payload) <= 65536:
                    raise ValueError('Invalid native checkpoint size')
                magic, schema, size, seq, match, epoch, level, tick, phase = NATIVE_HEADER.unpack_from(payload)
                header = f'{SCHEMA}:{reply["match_id"]}:{snapshot["epoch"]}:{snapshot["sequence"]}:{snapshot["tick"]}:'.encode('ascii')
                if (magic != 0x314d4341 or schema != 1 or size != len(payload) or
                    seq != snapshot['sequence'] or tick != snapshot['tick'] or epoch != snapshot['epoch'] or
                    match.hex() != reply['match_id'] or level > 100 or phase != 7 or
                    hashlib.sha256(header+payload).hexdigest() != snapshot['digest']):
                    raise ValueError('Native checkpoint identity/digest mismatch')
                snapshot = dict(snapshot, bytes=payload, crc=zlib.crc32(payload))
            previous_snapshot = prior.get('checkpoint') if prior else None
            if snapshot and (not previous_snapshot or previous_snapshot['digest'] != snapshot['digest']):
                self.download_offset = 0
                self.ack = None
            self.status = dict(reply, checkpoint=snapshot)

    def from_native(self, payload, endpoint):
        with self.lock:
            if len(payload) in (PROGRESS.size, SLOT_PROGRESS.size) and payload[:4] == b'AMPG':
                _, counter, active, native_peer = PROGRESS.unpack_from(payload)
                if active > 1 or native_peer not in (*range(16), 0xffffffff):
                    return
                native_slots = None
                if len(payload) == SLOT_PROGRESS.size:
                    limit, humans, bots, slot = struct.unpack_from('<4I', payload, PROGRESS.size)
                    if limit:
                        if (not 1 <= limit <= 16 or humans & bots or (humans | bots) >> limit or
                            not 0 <= slot < limit or not humans & (1 << slot)):
                            return
                        native_slots = dict(limit=limit, human_mask=humans, bot_mask=bots, local_slot=slot)
                # The game sends this from its own dedicated authenticated socket.
                self.endpoint = endpoint
                if counter > self.counter:
                    self.counter, self.active = counter, bool(active)
                    self.native_peer = native_peer if native_peer < 16 else None
                    self.native_slots = native_slots
                return
            if endpoint != self.endpoint:
                return
            if len(payload) == 8 and payload[:4] == b'AMLV':
                sequence = struct.unpack_from('<I', payload, 4)[0]
                if sequence and (self.leave_sequence is None or sequence == self.leave_sequence):
                    self.leaving = True
                    self.leave_sequence = sequence
                    if self.leaving_at is None:
                        self.leaving_at = self.bridge.clock()
            elif len(payload) == ACK.size and payload[:4] == b'AMAK':
                _, sequence, digest, ready = ACK.unpack(payload)
                snapshot = self.status.get('checkpoint') if self.status else None
                if snapshot and snapshot['sequence'] == sequence and snapshot['digest'] == digest.hex() and ready <= 1:
                    self.ack = (sequence, digest.hex(), bool(ready))
            elif FRAGMENT.size < len(payload) <= FRAGMENT.size+CHUNK and payload[:4] == b'AMCP':
                _, sequence, total, offset, crc = FRAGMENT.unpack_from(payload)
                part = payload[FRAGMENT.size:]
                if not 0 < total <= 65536 or offset % CHUNK or offset >= total or len(part) != min(CHUNK, total-offset):
                    return
                identity = sequence, total, crc
                if self.offer and sequence < NATIVE_HEADER.unpack_from(self.offer)[3]:
                    return
                assembly = self.assembly
                if assembly is None or assembly['identity'] != identity or self.bridge.clock()-assembly['at'] > 3:
                    assembly = dict(identity=identity, data=bytearray(total), received=set(), at=self.bridge.clock())
                    self.assembly = assembly
                if offset in assembly['received'] and assembly['data'][offset:offset+len(part)] != part:
                    self.assembly = None
                    return
                assembly['data'][offset:offset+len(part)] = part
                assembly['received'].add(offset)
                if len(assembly['received']) == (total+CHUNK-1)//CHUNK:
                    data = bytes(assembly['data'])
                    if zlib.crc32(data) == crc:
                        self.offer = data
                    self.assembly = None

    def body(self, compatibility, *, recover=False):
        with self.lock:
            status = self.status
            body = dict(compatibility=compatibility, epoch=status['epoch'],
                        native_counter=self.counter, active=self.active, native_peer=self.native_peer)
            if hasattr(self.bridge, 'quality_snapshot'):
                body['connection_quality'] = self.bridge.quality_snapshot()
            if self.match_info and status['host_peer'] == self.bridge.peer_id:
                body['match_info'] = dict(self.match_info)
            if (self.roster_info and status['host_peer']==self.bridge.peer_id
                    and self.bridge.clock()-self.roster_at<=5):
                body['player_info'] = self.roster_info
            if self.native_slots is not None:
                body['native_slots'] = self.native_slots
            if recover:
                body['epoch']=-1
                return body
            if self.offer and status['host_peer'] == self.bridge.peer_id and status['phase'] == 'active':
                body['checkpoint'] = base64.b64encode(self.offer).decode('ascii')
            if self.ack:
                sequence, digest, ready = self.ack
                body['checkpoint_ack'] = dict(sequence=sequence, digest=digest)
                if ready and status['phase'] == 'preparing':
                    body['ready'] = digest
                elif ready and status.get('joining'):
                    body['join_ready'] = digest
            return body

    def poll(self):
        with self.lock:
            now = self.bridge.clock()
            if not self.endpoint or not self.status or now-self.last_control < .1:
                return
            status = self.status
            snapshot = status.get('checkpoint')
            host = virtual_ip(status['host_peer']) if status['host_peer'] else bytes(4)
            candidate = virtual_ip(status['candidate']) if status.get('candidate') else bytes(4)
            previous = virtual_ip(status['previous_host']) if status.get('previous_host') else bytes(4)
            digest = bytes.fromhex(snapshot['digest']) if snapshot else bytes(32)
            phase = 5 if status.get('joining') and status['phase'] == 'active' else PHASE[status['phase']]
            state = STATE.pack(b'AMST', 1, status['epoch'], 4 if self.departed else phase,
                snapshot['sequence'] if snapshot else 0, host, candidate, previous,
                len(snapshot['bytes']) if snapshot else 0, digest, bytes.fromhex(status['match_id']))
            peers = status.get('native_peers', [])
            if len(peers) > 16:
                raise ValueError('Too many native peers')
            state += struct.pack('<I', len(peers))
            state += b''.join(struct.pack('<I4s', peer['native_peer'], virtual_ip(peer['peer_id'])) for peer in peers)
            state += bytes((16-len(peers))*8)
            self.send(state)
            # At most eight datagrams per poll, with a complete retry after the
            # final fragment. Native ACKs, not UDP delivery, finish the transfer.
            if snapshot and (not self.ack or self.ack[1] != snapshot['digest']):
                for _ in range(8):
                    offset = self.download_offset
                    data = snapshot['bytes']
                    self.send(DOWNLOAD.pack(b'AMFR', snapshot['sequence'], len(data), offset,
                        snapshot['crc'], digest)+data[offset:offset+CHUNK])
                    self.download_offset += CHUNK
                    if self.download_offset >= len(data):
                        self.download_offset = 0
                        break
            self.last_control = now

    def send(self, payload):
        bridge = self.bridge
        packet = LOCAL.pack(LOCAL_MAGIC, bridge.compatibility, bridge.local_token,
                            virtual_ip(bridge.peer_id), virtual_ip(bridge.peer_id), 0, 0)+payload
        try:
            bridge.local.sendto(packet, self.endpoint)
        except OSError:
            bridge.dropped += 1


class ArcadeSession(Session):
    def __init__(self, client, compatibility, lobby, bridge):
        self.native = NativeArcade(bridge)
        self.native.install(lobby)
        bridge.arcade = self.native
        super().__init__(client, compatibility, lobby, lobby['peer_id'] == lobby['host_peer'], bridge)

    @classmethod
    def open(cls, client, compatibility, *, name=None, public=True, code=None):
        if code is None:
            path = '/v3/lobbies'
            body = dict(compatibility=compatibility, name=name, public=public,
                        mode='arcade', migration_schema=SCHEMA)
        else:
            if not isinstance(code, str) or not re.fullmatch('[a-fA-F0-9]{16}', code):
                raise ValueError('Lobby code must contain 16 hexadecimal characters')
            path = '/v3/lobbies/'+code.upper()+'/join'
            body = dict(compatibility=compatibility, migration_schema=SCHEMA)
        lobby = client.request('POST', path, body)
        bridge = None
        try:
            if lobby.get('migration_schema') != SCHEMA or lobby.get('transport') != 'relay':
                raise ValueError('Directory does not support native Arcade migration')
            address = str(ipaddress.IPv4Address(lobby['address']))
            bridge = Bridge(compatibility, lobby['room'], lobby['peer_id'], lobby['peer_key'],
                host=relay_endpoint(lobby['host_peer']), relay=(address, lobby['port']),
                relay_key=lobby['relay_key'], host_peer=lobby['host_peer'])
            active = cls(client, compatibility, lobby, bridge)
            if not bridge.ready.wait(12):
                active.close()
                raise ValueError('Arcade host or relay did not answer')
            return active
        except Exception:
            if bridge:
                bridge.close()
            try:
                client.request('DELETE', '/v3/lobbies/'+lobby['code']+'/members/'+str(lobby['peer_id']),
                               token=lobby['peer_token'])
            except Exception:
                pass
            raise

    def endpoint(self):
        return '/v3/lobbies/'+self.lobby['code']+'/members/'+str(self.bridge.peer_id), self.lobby['peer_token']

    def heartbeat(self):
        try:
            ArcadeSession._heartbeat(self)
        finally:
            self.client.close_persistent()

    def _heartbeat(self):
        last_success=self.bridge.clock()
        while not self.closed.is_set():
            try:
                path, token = self.endpoint()
                if self.native.leaving:
                    # Publish the final frozen native frame before choosing a
                    # successor. Wait for an engine ACK of that exact frame.
                    body = self.native.body(self.compatibility)
                    reply = self.client.request('PUT', path, body, token, timeout=1, persistent=True)
                    self.native.install(reply)
                    last_success=self.bridge.clock()
                    with self.native.lock:
                        wanted = self.native.leave_sequence
                        expired = self.bridge.clock()-self.native.leaving_at >= 6
                    snapshot = reply.get('checkpoint')
                    if reply['players'] == 1 or expired or (snapshot and snapshot['sequence'] == wanted and reply.get('backup_ready')):
                        self.client.request('DELETE', path, token=token, timeout=1, persistent=True)
                        self.native.departed = True
                        return  # Bridge remains alive to deliver departure ACK.
                    self.closed.wait(.1)
                    continue
                reply = self.client.request('PUT', path, self.native.body(self.compatibility), token, timeout=1, persistent=True)
                self.native.install(reply)
                last_success=self.bridge.clock()
                self.host = reply['host_peer'] == self.bridge.peer_id
                self.error = ''
            except Exception as error:
                self.error = str(error)
                try:
                    # A lost RPC response or expired checkpoint ACK must not
                    # prevent recovering the newly fenced epoch and credentials.
                    path, token = self.endpoint()
                    reply = self.client.request('PUT', path,
                        self.native.body(self.compatibility,recover=True), token, timeout=1, persistent=True)
                    self.native.install(reply)
                    last_success=self.bridge.clock()
                    with self.native.lock:
                        self.native.ack = None
                except (OSError,http.client.HTTPException):
                    # A five-second TLS stall outlives the four-second native
                    # lease. Retry short RPCs with real engine progress instead;
                    # never keep a disconnected authority alive indefinitely.
                    if self.bridge.clock()-last_success>=3.5:
                        self.closed.set();self.bridge.stopped.set()
                        return
                except Exception:
                    self.closed.set();self.bridge.stopped.set()
                    return
            self.closed.wait(.25)
