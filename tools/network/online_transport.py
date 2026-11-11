"""System Link overlay: virtual game addresses, one host UDP port, authenticated relay.

Guests use a direct host connection or an outbound-only shared relay. The first
player remains the game host and routes guest-to-guest traffic in either case.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
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

import hmac
from collections import deque
import ipaddress
import secrets
import selectors
import socket
import struct
import threading
import time

LOCAL = struct.Struct('!8s32s16s4s4sHH')
WIRE = struct.Struct('!8s32s16sQBBHH')
LOCAL_MAGIC = b'TSFB\x01\0\0\0'
WIRE_MAGIC = b'TSFO\x01\0\0\0'
MAX_PAYLOAD = 4096
HELLO, ACK, DATA, PING, PONG = 1, 2, 3, 4, 5
METRICS = struct.Struct('!4sIH')
# The relay authenticates this outer envelope separately from the existing
# host/guest packet. It never needs to change or interpret the game payload.
RELAY = struct.Struct('!8s16sQBBB')
RELAY_MAGIC = b'TSFR\x01\0\0\0'
REGISTER, REGISTERED, FORWARD = 1, 2, 3
MAX_RELAY_PACKET = RELAY.size + WIRE.size + 65 + MAX_PAYLOAD


def relay_packet(room, sequence, source, target, kind, key, payload=b''):
    body = RELAY.pack(RELAY_MAGIC, room, sequence, source, target, kind) + payload
    return body + hmac.digest(key, body, 'sha256')


def relay_endpoint(peer):
    # Internal addresses used only after authenticated relay decapsulation.
    return (str(ipaddress.IPv4Address(virtual_ip(peer))), 0)


def virtual_ip(peer):
    if not 1 <= peer <= 254:
        raise ValueError('Invalid peer ID')
    return bytes((10, 254, 0, peer))


def destination(ip):
    if ip == b'\xff'*4:
        return 0
    if ip[:3] != b'\x0a\xfe\x00' or not 1 <= ip[3] <= 254:
        raise ValueError('Destination outside this room')
    return ip[3]


class Bridge:
    def __init__(self, compatibility, room, peer_id=1, key=None, host=None,
                 bind=('0.0.0.0', 0), clock=time.monotonic, relay=None, relay_key=None, host_peer=1):
        self.compatibility = bytes.fromhex(compatibility)
        self.room = bytes.fromhex(room)
        if len(self.compatibility) != 32 or len(self.room) != 16:
            raise ValueError('Invalid room identity')
        self.peer_id, self.clock = peer_id, clock
        self.host_peer = host_peer
        virtual_ip(host_peer)
        self.arcade = None
        self.migration_frozen = False
        virtual_ip(peer_id)
        self.key = bytes.fromhex(key) if key else None
        if peer_id != host_peer and (not self.key or len(self.key) != 32 or not host):
            raise ValueError('Guest requires host and peer credential')
        self.host = host
        self.relay = relay
        self.relay_key = bytes.fromhex(relay_key) if relay_key else None
        if relay and (not self.relay_key or len(self.relay_key) != 32):
            raise ValueError('Relay requires a private peer credential')
        self.relay_sequence = 0
        self.relay_received_sequence = -1
        self.last_registration = -10
        self.relay_ready = threading.Event()
        self.relay_seen = self.clock()
        self.local_token = secrets.token_bytes(16)
        self.local = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.local.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 262144)
        self.local.bind(('127.0.0.1', 0))
        self.wan = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.wan.bind(bind)
        self.local.setblocking(False)
        self.wan.setblocking(False)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.local, selectors.EVENT_READ, self.from_game)
        self.selector.register(self.wan, selectors.EVENT_READ, self.from_network)
        self.ports, self.peers = {}, {}
        self.lock = threading.Lock()
        self.ready = threading.Event()
        if peer_id == host_peer and not relay:
            self.ready.set()
        self.stopped = threading.Event()
        self.sequence = 0
        self.host_sequence = -1
        self.last_hello = -10
        self.received = self.sent = self.dropped = 0
        self.last_ping = self.last_metrics = -10
        self.pending_pings, self.latencies = {}, {}
        self.quality_lock = threading.Lock()
        self.quality = {}

    def quality_record(self, peer, milliseconds=None):
        now = self.clock()
        with self.quality_lock:
            sample = self.quality.setdefault(peer, dict(events=deque(maxlen=128), rtt=None, at=None))
            sample['events'].append((now, milliseconds is None))
            if milliseconds is not None:
                sample.update(rtt=round(milliseconds, 2), at=now)

    def quality_snapshot(self):
        """Authenticated host/guest RTT and completed probe outcomes, not game loss."""
        now = self.clock()
        peers = []
        with self.quality_lock:
            for peer, sample in self.quality.items():
                events = [lost for at, lost in sample['events'] if now-at <= 60]
                age = now-sample['at'] if sample['at'] is not None else None
                if not events:
                    continue
                peers.append(dict(peer=peer, rtt_ms=sample['rtt'] if age is not None and age <= 5 else None,
                                  rtt_age=round(age, 2) if age is not None else None,
                                  samples=len(events), missed=sum(events)))
        return dict(schema=1, window_seconds=60, peers=peers)

    def quality_reset(self, peer=None):
        with self.quality_lock:
            if peer is None:
                self.quality.clear()
            else:
                self.quality.pop(peer, None)

    def latency_snapshot(self):
        now = self.clock()
        values = [milliseconds for milliseconds, sampled in list(self.latencies.values()) if now-sampled <= 5]
        return (max(values), len(values)) if values else (-1, 0)

    def latency_control(self, kind, source, target, payload, endpoint, key):
        if kind not in (PING, PONG):
            return False
        if target != self.peer_id or len(payload) != 8:
            return True
        if kind == PING:
            self.send_wire(endpoint, key, PONG, self.peer_id, source, payload=payload)
        else:
            pending = self.pending_pings.get(source)
            if pending and hmac.compare_digest(payload, pending[0]):
                milliseconds = (self.clock()-pending[1])*1000
                self.latencies[source] = (milliseconds, self.clock())
                self.quality_record(source, milliseconds)
                del self.pending_pings[source]
        return True

    @property
    def config(self):
        return f'{self.local.getsockname()[1]} {self.local_token.hex()} {ipaddress.IPv4Address(virtual_ip(self.peer_id))}\n'

    def update_peers(self, members):
        # A peer's credential is delivered only to that peer and the host over
        # TLS. Another guest cannot sign packets for it or learn its key.
        with self.lock:
            wanted = {int(i): bytes.fromhex(key) for i, key in members.items()}
            for i in list(self.peers):
                if i not in wanted:
                    del self.peers[i]
                    self.latencies.pop(i, None)
                    self.pending_pings.pop(i, None)
                    self.quality_reset(i)
            for i, key in wanted.items():
                if not 1 <= i <= 254 or i == self.peer_id or len(key) != 32:
                    raise ValueError('Invalid directory roster')
                if i not in self.peers or self.peers[i]['key'] != key:
                    self.peers[i] = dict(key=key, endpoint=None, sequence=-1, seen=0)
                    self.latencies.pop(i, None)
                    self.pending_pings.pop(i, None)
                    self.quality_reset(i)

    def send_wire(self, endpoint, key, kind, source=0, target=0, sport=0, dport=0, payload=b''):
        self.sequence += 1
        body = WIRE.pack(WIRE_MAGIC, self.compatibility, self.room, self.sequence,
                         source, target, sport, dport) + bytes((kind,)) + payload
        try:
            packet = body + hmac.digest(key, body, 'sha256')
            if self.relay:
                peer = destination(ipaddress.IPv4Address(endpoint[0]).packed)
                self.send_relay(FORWARD, peer, packet)
            else:
                self.wan.sendto(packet, endpoint)
            self.sent += 1
        except OSError:
            self.dropped += 1

    def deliver(self, source, sport, dport, payload):
        endpoint = self.ports.get(dport)
        if endpoint:
            body = LOCAL.pack(LOCAL_MAGIC, self.compatibility, self.local_token,
                              virtual_ip(source), virtual_ip(self.peer_id), sport, dport) + payload
            self.local.sendto(body, endpoint)

    def route(self, source, target, sport, dport, payload):
        if target in (0, self.peer_id) and source != self.peer_id:
            self.deliver(source, sport, dport, payload)
        with self.lock:
            for peer_id, peer in self.peers.items():
                if peer_id != source and target in (0, peer_id) and peer['endpoint']:
                    self.send_wire(peer['endpoint'], peer['key'], DATA, source, target, sport, dport, payload)

    def from_game(self, packet, endpoint):
        if endpoint[0] != '127.0.0.1' or not LOCAL.size <= len(packet) <= LOCAL.size+MAX_PAYLOAD:
            return
        magic, version, token, source, target, sport, dport = LOCAL.unpack_from(packet)
        if (magic != LOCAL_MAGIC or version != self.compatibility or
            not hmac.compare_digest(token, self.local_token) or source != virtual_ip(self.peer_id)):
            self.dropped += 1
            return
        if sport == 0 and dport == 0 and self.arcade is not None:
            self.arcade.from_native(packet[LOCAL.size:], endpoint)
            return
        # Registration is a zero-destination-port packet; only game sockets
        # bound to the known System Link ports can register or send data.
        if sport not in (1000, 3000, 9999):
            return
        self.ports[sport] = endpoint
        if dport == 0:
            return
        if dport not in (1000, 3000, 9999):
            return
        try:
            target = destination(target)
        except ValueError:
            return
        payload = packet[LOCAL.size:]
        if self.migration_frozen:
            return
        if self.peer_id == self.host_peer:
            self.route(self.peer_id, target, sport, dport, payload)
        else:
            self.send_wire(self.host, self.key, DATA, self.peer_id, target, sport, dport, payload)

    def send_relay(self, kind, target=0, payload=b''):
        self.relay_sequence += 1
        self.wan.sendto(relay_packet(self.room, self.relay_sequence, self.peer_id,
                                    target, kind, self.relay_key, payload), self.relay)

    def from_relay(self, packet, endpoint):
        if endpoint != self.relay or not RELAY.size+32 <= len(packet) <= MAX_RELAY_PACKET:
            self.dropped += 1
            return
        magic, room, sequence, source, target, kind = RELAY.unpack_from(packet)
        if (magic != RELAY_MAGIC or room != self.room or target != self.peer_id or
            sequence <= self.relay_received_sequence or
            not hmac.compare_digest(hmac.digest(self.relay_key, packet[:-32], 'sha256'), packet[-32:])):
            self.dropped += 1
            return
        if kind == REGISTERED and source == 0 and len(packet) == RELAY.size+32:
            self.relay_received_sequence = sequence
            self.relay_seen = self.clock()
            self.relay_ready.set()
            if self.peer_id == self.host_peer:
                self.ready.set()
        elif kind == FORWARD and 1 <= source <= 254 and (source == self.host_peer or self.peer_id == self.host_peer):
            self.relay_received_sequence = sequence
            self.relay_seen = self.clock()
            self.from_wire(packet[RELAY.size:-32], relay_endpoint(source))

    def from_network(self, packet, endpoint):
        if self.relay:
            self.from_relay(packet, endpoint)
        else:
            self.from_wire(packet, endpoint)

    def from_wire(self, packet, endpoint):
        if self.migration_frozen:
            return
        if not WIRE.size+33 <= len(packet) <= WIRE.size+33+MAX_PAYLOAD:
            return
        magic, version, room, sequence, source, target, sport, dport = WIRE.unpack_from(packet)
        if magic != WIRE_MAGIC or version != self.compatibility or room != self.room:
            self.dropped += 1
            return
        body, signature = packet[:-32], packet[-32:]
        kind = packet[WIRE.size]
        if self.peer_id == self.host_peer:
            with self.lock:
                peer = self.peers.get(source)
                if (not peer or sequence <= peer['sequence'] or
                    not hmac.compare_digest(hmac.digest(peer['key'], body, 'sha256'), signature)):
                    self.dropped += 1
                    return
                if kind != HELLO and endpoint != peer['endpoint']:
                    return
                peer.update(sequence=sequence, seen=self.clock())
                if kind == HELLO:
                    peer['endpoint'] = endpoint
                    self.send_wire(endpoint, peer['key'], ACK, self.peer_id, source)
                    return
                if self.latency_control(kind, source, target, packet[WIRE.size+1:-32], endpoint, peer['key']):
                    return
        else:
            if (endpoint != self.host or sequence <= self.host_sequence or
                not hmac.compare_digest(hmac.digest(self.key, body, 'sha256'), signature)):
                self.dropped += 1
                return
            self.host_sequence = sequence
            if kind == ACK and source == self.host_peer and target == self.peer_id:
                self.ready.set()
                return
            if source == self.host_peer and self.latency_control(kind, source, target, packet[WIRE.size+1:-32], endpoint, self.key):
                return
        if kind != DATA or dport not in (1000, 3000, 9999) or sport not in (1000, 3000, 9999):
            return
        if not 1 <= source <= 254:
            return
        self.received += 1
        payload = packet[WIRE.size+1:-32]
        if self.peer_id == self.host_peer:
            self.route(source, target, sport, dport, payload)
        elif target in (0, self.peer_id):
            self.deliver(source, sport, dport, payload)

    def poll(self, timeout=.05):
        now = self.clock()
        if self.arcade is not None:
            self.arcade.poll()
        if self.relay and now-self.last_registration >= 1:
            try:
                self.send_relay(REGISTER)
            except OSError:
                self.dropped += 1
            self.last_registration = now
        if not self.migration_frozen and self.peer_id != self.host_peer and now-self.last_hello >= 1:
            self.send_wire(self.host, self.key, HELLO, self.peer_id, self.host_peer)
            self.last_hello = now
        if not self.migration_frozen and now-self.last_ping >= 1:
            with self.lock:
                targets = [(i, peer['endpoint'], peer['key']) for i, peer in self.peers.items()
                           if peer['endpoint'] and now-peer['seen'] <= 5] if self.peer_id == self.host_peer else (
                               [(self.host_peer, self.host, self.key)] if self.ready.is_set() else [])
                for peer, endpoint, key in targets:
                    pending = self.pending_pings.get(peer)
                    if pending and now-pending[1] <= 5:
                        continue
                    if pending:
                        self.quality_record(peer)
                    nonce = secrets.token_bytes(8)
                    self.pending_pings[peer] = (nonce, now)
                    self.send_wire(endpoint, key, PING, self.peer_id, peer, payload=nonce)
            self.last_ping = now
        if now-self.last_metrics >= 1 and self.ports:
            milliseconds, count = self.latency_snapshot()
            metrics = METRICS.pack(b'RTT1', min(int(milliseconds*1000), 0xfffffffe) if milliseconds >= 0 else 0xffffffff, count)
            packet = LOCAL.pack(LOCAL_MAGIC, self.compatibility, self.local_token,
                                virtual_ip(self.peer_id), virtual_ip(self.peer_id), 0, 0) + metrics
            for endpoint in set(self.ports.values()):
                self.local.sendto(packet, endpoint)
            self.last_metrics = now
        for selected, _ in self.selector.select(timeout):
            for _ in range(64):
                try:
                    packet, endpoint = selected.fileobj.recvfrom(8192)
                    selected.data(packet, endpoint)
                except BlockingIOError:
                    break
                except OSError:
                    self.dropped += 1
                    break

    def run(self):
        while not self.stopped.is_set():
            self.poll()

    def close(self):
        self.stopped.set()
        self.selector.close()
        self.local.close()
        self.wan.close()
