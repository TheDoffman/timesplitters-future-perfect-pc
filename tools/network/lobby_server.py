"""Self-hosted, dependency-free lobby directory. No Steam or paid service.

Optional UDP relay support keeps player hosts reachable without port forwarding.
By default it listens only on localhost. Non-loopback listening requires a TLS
certificate and key. State and owner credentials expire automatically.
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

import argparse
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import re
import secrets
import socket
import ssl
import threading
import time
from urllib.parse import parse_qs, urlsplit

from online_transport import (RELAY, RELAY_MAGIC, REGISTER, REGISTERED, FORWARD,
                              MAX_RELAY_PACKET, relay_packet)
from online_arcade_service import ArcadeService, ArcadeServiceError

VERSION = re.compile(r'[0-9a-f]{64}\Z')
TTL = 90
MAX_LOBBIES = 256
ONLINE_CAPACITY = {'arcade': 16, 'story': 4}


class LobbyError(Exception):
    def __init__(self, status, message):
        self.status, self.message = status, message


class Directory:
    def __init__(self, clock=time.monotonic, public_address=None, relay_address=None, relay_lan_address=None):
        self.clock = clock
        self.public_address = str(ipaddress.IPv4Address(public_address)) if public_address else None
        self.lock = threading.Lock()
        self.lobbies = {}
        self.online = {}
        self.relay_address = relay_address
        self.relay_lan_address = relay_lan_address
        self.rooms = {}
        self.arcade = ArcadeService(self)

    def expire(self):
        now = self.clock()
        self.lobbies = {key: value for key, value in self.lobbies.items() if value['expires'] > now}
        self.online = {key: value for key, value in self.online.items() if value['expires'] > now}
        for lobby in self.online.values():
            lobby['peers'] = {key: peer for key, peer in lobby['peers'].items() if peer['expires'] > now}
        self.rooms = {lobby['room']: lobby for lobby in self.online.values() if lobby['transport'] == 'relay'}

    def online_public(self, lobby, remote=''):
        result = {**{key: lobby[key] for key in ('code', 'room', 'compatibility', 'name', 'address', 'port', 'mode')},
                'transport': lobby['transport'],
                'players': 1 + len(lobby['peers']), 'capacity': ONLINE_CAPACITY[lobby['mode']]}
        if (lobby['transport'] == 'relay' and self.relay_lan_address and remote and
            ipaddress.ip_address(remote).is_private):
            result['address'] = self.relay_lan_address
        return result

    @staticmethod
    def relay_member():
        return dict(relay_key=secrets.token_hex(32), endpoint=None, sequence=-1,
                    seen=0, budget=None, budget_at=0)

    @staticmethod
    def online_mode(value):
        if not isinstance(value, str) or value not in ONLINE_CAPACITY:
            raise LobbyError(400, 'Unknown Online mode')
        return value

    def check_online_mode(self, lobby, body):
        if self.online_mode(body.get('mode', 'arcade')) != lobby['mode']:
            raise LobbyError(409, 'Incompatible Online mode')

    def online_request(self, method, path, query, body, remote, owner):
        if path == '/v2/lobbies' and method == 'GET':
            version = self.version(query.get('compatibility', [''])[0])
            mode = self.online_mode(query.get('mode', ['arcade'])[0])
            return 200, {'lobbies': [self.online_public(lobby, remote) for lobby in self.online.values()
                                    if lobby['listed'] and lobby['compatibility'] == version and lobby['mode'] == mode]}
        if path == '/v2/lobbies' and method == 'POST':
            version = self.version(body.get('compatibility'))
            mode = self.online_mode(body.get('mode', 'arcade'))
            name, port, listed = body.get('name'), body.get('port'), body.get('public', False)
            transport = body.get('transport', 'direct')
            if transport not in ('direct', 'relay', 'auto'):
                raise LobbyError(400, 'Unknown Online transport')
            if transport == 'auto':
                transport = 'relay' if self.relay_address else 'direct'
            if transport == 'relay' and not self.relay_address:
                raise LobbyError(503, 'This directory has no shared relay. Ask the server owner to enable it.')
            if (not isinstance(name, str) or not 1 <= len(name) <= 48 or
                any(ord(c) < 32 or ord(c) > 126 for c in name) or type(port) is not int or
                not 1024 <= port <= 65535 or type(listed) is not bool):
                raise LobbyError(400, 'Invalid lobby name, port, or visibility')
            if len(self.online) >= MAX_LOBBIES or sum(l['origin'] == remote for l in self.online.values()) >= 8:
                raise LobbyError(429, 'Lobby limit reached')
            code = secrets.token_hex(8).upper()
            while code in self.online:
                code = secrets.token_hex(8).upper()
            token = secrets.token_urlsafe(32)
            advertised = self.public_address if self.public_address and ipaddress.ip_address(remote).is_private else remote
            if transport == 'relay':
                advertised, port = self.relay_address
            lobby = dict(code=code, room=secrets.token_hex(16), compatibility=version, name=name, mode=mode,
                         port=port, address=advertised, origin=remote, listed=listed, expires=self.clock()+TTL,
                         owner=hashlib.sha256(token.encode()).digest(), peers={}, next_peer=2,
                         transport=transport, relay_host=self.relay_member())
            self.online[code] = lobby
            if transport == 'relay':
                self.rooms[lobby['room']] = lobby
            return 201, {**self.online_public(lobby, remote), 'owner_token': token,
                         **({'relay_key': lobby['relay_host']['relay_key']} if transport == 'relay' else {})}
        match = re.fullmatch(r'/v2/lobbies/([A-Fa-f0-9]{16})(/join|/peers/(\d+))?', path)
        if not match:
            raise LobbyError(404, 'Unknown endpoint')
        lobby = self.online.get(match[1].upper())
        if not lobby:
            raise LobbyError(404, 'Lobby closed or expired')
        if match[2] == '/join' and method == 'POST':
            if self.version(body.get('compatibility')) != lobby['compatibility']:
                raise LobbyError(409, 'Incompatible game version or content')
            self.check_online_mode(lobby, body)
            if 1 + len(lobby['peers']) >= ONLINE_CAPACITY[lobby['mode']]:
                raise LobbyError(409, 'Lobby full')
            # Never reuse an ID in a room: stale packets cannot become another
            # player's identity after a disconnect. The address range holds 253.
            if lobby['next_peer'] > 254:
                raise LobbyError(409, 'Room exhausted; host a new lobby')
            peer_id = lobby['next_peer']
            lobby['next_peer'] += 1
            key = secrets.token_hex(32)
            peer = dict(key=key, expires=self.clock()+TTL, **self.relay_member())
            lobby['peers'][peer_id] = peer
            return 201, {**self.online_public(lobby, remote), 'peer_id': peer_id, 'peer_key': key,
                         **({'relay_key': peer['relay_key']} if lobby['transport'] == 'relay' else {})}
        if match[3]:
            peer_id = int(match[3])
            peer = lobby['peers'].get(peer_id)
            if not peer or not hmac.compare_digest(peer['key'].encode(), owner.encode()):
                raise LobbyError(403, 'Invalid peer credential')
            if method == 'DELETE':
                del lobby['peers'][peer_id]
                return 200, {'closed': True}
            if method != 'PUT':
                raise LobbyError(405, 'Method not allowed')
            if self.version(body.get('compatibility')) != lobby['compatibility']:
                raise LobbyError(409, 'Incompatible game version or content')
            self.check_online_mode(lobby, body)
            peer['expires'] = self.clock()+TTL
            return 200, self.online_public(lobby, remote)
        if not owner or not hmac.compare_digest(lobby['owner'], hashlib.sha256(owner.encode()).digest()):
            raise LobbyError(403, 'Invalid lobby owner credential')
        if method == 'DELETE':
            del self.online[lobby['code']]
            self.rooms.pop(lobby['room'], None)
            return 200, {'closed': True}
        if method != 'PUT':
            raise LobbyError(405, 'Method not allowed')
        if self.version(body.get('compatibility')) != lobby['compatibility']:
            raise LobbyError(409, 'Cannot change compatibility during a session')
        self.check_online_mode(lobby, body)
        lobby['expires'] = self.clock()+TTL
        return 200, {**self.online_public(lobby, remote),
                     'peers': {str(i): p['key'] for i, p in lobby['peers'].items()}}

    @staticmethod
    def version(value):
        if not isinstance(value, str) or not VERSION.fullmatch(value):
            raise LobbyError(400, 'A valid compatibility identity is required')
        return value

    @staticmethod
    def public(lobby):
        return {key: lobby[key] for key in ('code', 'compatibility', 'name', 'address', 'port', 'players', 'capacity')}

    def request(self, method, path, query, body, remote, owner):
        with self.lock:
            self.expire()
            if path.startswith('/v3/'):
                try:
                    return self.arcade.request(method, path, query, body, remote, owner)
                except ArcadeServiceError as error:
                    raise LobbyError(error.status, error.message) from error
            if path.startswith('/v2/'):
                return self.online_request(method, path, query, body, remote, owner)
            if path == '/v1/lobbies' and method == 'GET':
                version = self.version(query.get('compatibility', [''])[0])
                return 200, {'lobbies': [self.public(lobby) for lobby in self.lobbies.values()
                                        if lobby['listed'] and lobby['compatibility'] == version]}
            if path == '/v1/lobbies' and method == 'POST':
                version = self.version(body.get('compatibility'))
                name, port = body.get('name'), body.get('port')
                listed = body.get('public', False)
                if (not isinstance(name, str) or not 1 <= len(name) <= 48 or
                    any(ord(c) < 32 or ord(c) > 126 for c in name) or
                    type(port) is not int or not 1024 <= port <= 65534 or type(listed) is not bool):
                    raise LobbyError(400, 'Invalid lobby name, port, or visibility')
                if len(self.lobbies) >= MAX_LOBBIES or sum(v['address'] == remote for v in self.lobbies.values()) >= 8:
                    raise LobbyError(429, 'Lobby limit reached; retry after old lobbies expire')
                code = secrets.token_hex(8).upper()
                while code in self.lobbies:
                    code = secrets.token_hex(8).upper()
                token = secrets.token_urlsafe(32)
                lobby = dict(code=code, compatibility=version, name=name, address=remote, port=port,
                             players=1, capacity=16, listed=listed, expires=self.clock() + TTL,
                             owner=hashlib.sha256(token.encode()).digest())
                self.lobbies[code] = lobby
                return 201, {**self.public(lobby), 'owner_token': token, 'heartbeat_seconds': 30}
            match = re.fullmatch(r'/v1/lobbies/([A-Fa-f0-9]{16})', path)
            if not match:
                raise LobbyError(404, 'Unknown endpoint')
            lobby = self.lobbies.get(match[1].upper())
            if not lobby:
                raise LobbyError(404, 'Lobby closed or expired')
            if method == 'GET':
                version = self.version(query.get('compatibility', [''])[0])
                if version != lobby['compatibility']:
                    raise LobbyError(409, 'Incompatible game version or content')
                if lobby['players'] >= lobby['capacity']:
                    raise LobbyError(409, 'Lobby full')
                return 200, self.public(lobby)
            if method not in ('PUT', 'DELETE'):
                raise LobbyError(405, 'Method not allowed')
            if not owner or not hmac.compare_digest(lobby['owner'], hashlib.sha256(owner.encode()).digest()):
                raise LobbyError(403, 'Invalid lobby owner credential')
            if method == 'DELETE':
                del self.lobbies[lobby['code']]
                return 200, {'closed': True}
            if self.version(body.get('compatibility')) != lobby['compatibility']:
                raise LobbyError(409, 'Cannot change compatibility during a session')
            players, capacity = body.get('players'), body.get('capacity')
            if type(players) is not int or type(capacity) is not int or not 1 <= players <= capacity <= 16:
                raise LobbyError(400, 'Invalid player count')
            lobby.update(players=players, capacity=capacity, expires=self.clock() + TTL)
            return 200, self.public(lobby)


class RelayServer:
    """Bounded host/guest forwarding, authorized by the HTTPS directory roster.

    Only REGISTER learns an endpoint. Clients cannot name arbitrary forwarding
    addresses or send between rooms. Revocation/expiry is checked on every packet.
    """
    def __init__(self, directory, bind=('127.0.0.1', 0), *,
                 bytes_per_second=2*1024*1024, total_bytes_per_second=16*1024*1024):
        if bytes_per_second < MAX_RELAY_PACKET or total_bytes_per_second < MAX_RELAY_PACKET:
            raise ValueError('Relay bandwidth limits must allow at least one maximum packet')
        self.directory = directory
        self.limit, self.total_limit = bytes_per_second, total_bytes_per_second
        self.budget, self.budget_at = total_bytes_per_second, directory.clock()
        self.sequence = 0
        self.forwarded = self.dropped = 0
        self.stopped = threading.Event()
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            self.socket.bind(bind)
            self.socket.settimeout(.1)
        except Exception:
            self.socket.close()
            raise

    def handle(self, packet, endpoint):
        if not RELAY.size+32 <= len(packet) <= MAX_RELAY_PACKET:
            self.dropped += 1
            return
        magic, room, sequence, source, target, kind = RELAY.unpack_from(packet)
        if magic != RELAY_MAGIC or not 1 <= source <= 254:
            self.dropped += 1
            return
        with self.directory.lock:
            now = self.directory.clock()
            arcade = self.directory.arcade.relay_room(room)
            lobby = self.directory.rooms.get(room.hex())
            if arcade:
                member = arcade['members'].get(source)
                # Preparation has no gameplay authority. Only authenticated
                # registration/status/checkpoint traffic can proceed then.
                active_host = arcade['host_peer']
                if kind == FORWARD and active_host is None:
                    self.dropped += 1
                    return
            elif (not lobby or self.directory.online.get(lobby['code']) is not lobby or
                lobby['expires'] <= now):
                self.dropped += 1
                return
            else:
                member = lobby['relay_host'] if source == 1 else lobby['peers'].get(source)
                active_host = 1
            if not member or member.get('expires', now+1) <= now:
                self.dropped += 1
                return
            key = bytes.fromhex(member['relay_key'])
            if (sequence <= member['sequence'] or
                not hmac.compare_digest(hmac.digest(key, packet[:-32], 'sha256'), packet[-32:])):
                self.dropped += 1
                return
            payload = packet[RELAY.size:-32]
            if kind == REGISTER:
                if target != 0 or payload:
                    self.dropped += 1
                    return
                recipient, recipient_id, reply_kind = member, source, REGISTERED
                reply_source = 0
            elif kind == FORWARD:
                if (endpoint != member['endpoint'] or now-member['seen'] > 5 or not payload or
                    not 1 <= target <= 254 or source == target or
                    (source != active_host and target != active_host)):
                    self.dropped += 1
                    return
                recipient = (arcade['members'].get(target) if arcade else
                             lobby['relay_host'] if target == 1 else lobby['peers'].get(target))
                if (not recipient or not recipient['endpoint'] or now-recipient['seen'] > 5 or
                    recipient.get('expires', now+1) <= now):
                    self.dropped += 1
                    return
                recipient_id, reply_kind, reply_source = target, FORWARD, source
            else:
                self.dropped += 1
                return
            # Token buckets bound both individual credentials and total egress.
            member['budget'] = (self.limit if member['budget'] is None else
                                min(self.limit, member['budget'] + max(0, now-member['budget_at'])*self.limit))
            member['budget_at'] = now
            self.budget = min(self.total_limit, self.budget + max(0, now-self.budget_at)*self.total_limit)
            self.budget_at = now
            cost = len(packet)
            if min(member['budget'], self.budget) < cost:
                self.dropped += 1
                return
            member['budget'] -= cost
            self.budget -= cost
            member['sequence'] = sequence
            if kind == REGISTER:
                member.update(endpoint=endpoint, seen=now)
            self.sequence += 1
            reply = relay_packet(room, self.sequence, reply_source, recipient_id, reply_kind,
                                 bytes.fromhex(recipient['relay_key']), payload)
            try:
                self.socket.sendto(reply, recipient['endpoint'])
                if kind == FORWARD:
                    self.forwarded += 1
            except OSError:
                self.dropped += 1

    def run(self):
        while not self.stopped.is_set():
            try:
                packet, endpoint = self.socket.recvfrom(MAX_RELAY_PACKET+1)
                self.handle(packet, endpoint)
            except socket.timeout:
                continue
            except OSError:
                if not self.stopped.is_set():
                    self.dropped += 1

    def close(self):
        self.stopped.set()
        self.socket.close()


class Handler(BaseHTTPRequestHandler):
    server_version = 'TSFPLobbies/1'
    protocol_version = 'HTTP/1.1'

    def setup(self):
        super().setup()
        self.connection.settimeout(5)
        if isinstance(self.connection, ssl.SSLSocket):
            self.connection.do_handshake()

    def log_message(self, *args):
        pass  # Do not log private lobby codes or credentials.

    def handle_request(self):
        try:
            url = urlsplit(self.path)
            if len(self.path) > 2048 or self.headers.get('Transfer-Encoding'):
                raise LobbyError(400, 'Invalid request')
            length = int(self.headers.get('Content-Length', '0'))
            limit = 131072 if url.path.startswith('/v3/') else 4096
            if not 0 <= length <= limit:
                raise LobbyError(413, 'Request too large')
            body = json.loads(self.rfile.read(length)) if length else {}
            if not isinstance(body, dict):
                raise LobbyError(400, 'Expected a JSON object')
            auth = self.headers.get('Authorization', '')
            owner = auth[7:] if auth.startswith('Bearer ') else ''
            # Never trust forwarded-IP headers from an arbitrary client.
            remote = str(ipaddress.ip_address(self.client_address[0]))
            status, result = self.server.directory.request(
                self.command, url.path, parse_qs(url.query), body, remote, owner)
        except LobbyError as error:
            status, result = error.status, {'error': error.message}
        except (ValueError, UnicodeError):
            status, result = 400, {'error': 'Invalid request body'}
        data = json.dumps(result, separators=(',', ':')).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.close_connection=self.headers.get('Connection','').lower()!='keep-alive'
        self.send_header('Connection', 'close' if self.close_connection else 'keep-alive')
        self.end_headers()
        self.wfile.write(data)

    do_GET = do_POST = do_PUT = do_DELETE = handle_request


class Server(ThreadingHTTPServer):
    daemon_threads = True
    # Windows SO_REUSEADDR permits a second listener to hijack the port.
    allow_reuse_address = not hasattr(socket, 'SO_EXCLUSIVEADDRUSE')

    def server_bind(self):
        if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

    def __init__(self, address, directory=None):
        super().__init__(address, Handler)
        self.directory = directory or Directory()
        self.slots = threading.BoundedSemaphore(32)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except Exception:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            try:
                self.finish_request(request, client_address)
            except (ConnectionError, TimeoutError, ssl.SSLError):
                pass  # Disconnected clients and failed TLS handshakes are routine.
            except Exception:
                self.handle_error(request, client_address)
            finally:
                self.shutdown_request(request)
        finally:
            self.slots.release()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bind', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=30820)
    parser.add_argument('--cert', help='TLS certificate chain in PEM format')
    parser.add_argument('--key', help='TLS private key in PEM format')
    parser.add_argument('--public-address', help='Public IPv4 for game hosts registering from this directory PC or its LAN')
    parser.add_argument('--relay-port', type=int, help='Enable the shared UDP relay on this port (usually 30821)')
    parser.add_argument('--relay-address', help='Public numeric IPv4 clients use to reach the relay')
    parser.add_argument('--relay-lan-address', help='Optional numeric LAN IPv4 for clients on this server network')
    parser.add_argument('--relay-bytes-per-second', type=int, default=2*1024*1024,
                        help='Per-player relay bandwidth cap; default 2 MiB/s')
    parser.add_argument('--relay-total-bytes-per-second', type=int, default=16*1024*1024,
                        help='Total relay bandwidth cap; default 16 MiB/s')
    args = parser.parse_args()
    if bool(args.cert) != bool(args.key):
        parser.error('--cert and --key must be supplied together')
    try:
        bind_address = ipaddress.ip_address(args.bind)
        loopback = bind_address.is_loopback
    except ValueError:
        parser.error('--bind must be a numeric IP address')
    if bind_address.version != 4 or not 1 <= args.port <= 65535:
        parser.error('Use an IPv4 bind address and a port from 1 to 65535')
    if not loopback and not args.cert:
        parser.error('Non-loopback listening requires --cert and --key (HTTPS)')
    if bool(args.relay_port) != bool(args.relay_address):
        parser.error('--relay-port and --relay-address must be supplied together')
    if args.relay_port is not None:
        try:
            relay_ip = ipaddress.IPv4Address(args.relay_address)
        except ipaddress.AddressValueError:
            parser.error('--relay-address must be a numeric IPv4 address')
        if (not 1 <= args.relay_port <= 65535 or relay_ip.is_unspecified or relay_ip.is_multicast or
            relay_ip.is_loopback and not loopback):
            parser.error('Use a reachable relay IPv4 and a port from 1 to 65535')
        if min(args.relay_bytes_per_second, args.relay_total_bytes_per_second) < MAX_RELAY_PACKET:
            parser.error('Relay bandwidth caps must allow at least one maximum packet')
    if args.relay_lan_address:
        try:
            lan_ip = ipaddress.IPv4Address(args.relay_lan_address)
        except ipaddress.AddressValueError:
            parser.error('--relay-lan-address must be a numeric IPv4 address')
        if not args.relay_port or not lan_ip.is_private or lan_ip.is_unspecified or lan_ip.is_multicast:
            parser.error('--relay-lan-address requires an enabled relay and a private LAN IPv4')
    context = None
    if args.cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(args.cert, args.key)
    with Server((args.bind, args.port), Directory(public_address=args.public_address)) as server:
        if context:
            server.socket = context.wrap_socket(server.socket, server_side=True, do_handshake_on_connect=False)
        scheme = 'https' if context else 'http'
        print(f'Lobby directory listening on {scheme}://{args.bind}:{server.server_port}; Ctrl+C to stop', flush=True)
        relay = None
        relay_thread = None
        try:
            if args.relay_port:
                relay = RelayServer(server.directory, (args.bind, args.relay_port),
                                    bytes_per_second=args.relay_bytes_per_second,
                                    total_bytes_per_second=args.relay_total_bytes_per_second)
                server.directory.relay_address = (str(relay_ip), args.relay_port)
                server.directory.relay_lan_address = args.relay_lan_address
                relay_thread = threading.Thread(target=relay.run, daemon=True)
                relay_thread.start()
                print(f'Player-hosted relay listening on UDP {args.bind}:{args.relay_port}', flush=True)
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            if relay:
                relay.close()
            if relay_thread:
                relay_thread.join(timeout=2)


if __name__ == '__main__':
    main()
