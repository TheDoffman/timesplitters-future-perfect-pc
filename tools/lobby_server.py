"""Self-hosted, dependency-free lobby directory. No Steam or paid service.

This is discovery/signaling infrastructure, not a gameplay server or NAT relay.
By default it listens only on localhost. Non-loopback listening requires a TLS
certificate and key. State and owner credentials expire automatically.
"""
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

VERSION = re.compile(r'[0-9a-f]{64}\Z')
TTL = 90
MAX_LOBBIES = 256


class LobbyError(Exception):
    def __init__(self, status, message):
        self.status, self.message = status, message


class Directory:
    def __init__(self, clock=time.monotonic, public_address=None):
        self.clock = clock
        self.public_address = str(ipaddress.IPv4Address(public_address)) if public_address else None
        self.lock = threading.Lock()
        self.lobbies = {}
        self.online = {}

    def expire(self):
        now = self.clock()
        self.lobbies = {key: value for key, value in self.lobbies.items() if value['expires'] > now}
        self.online = {key: value for key, value in self.online.items() if value['expires'] > now}
        for lobby in self.online.values():
            lobby['peers'] = {key: peer for key, peer in lobby['peers'].items() if peer['expires'] > now}

    @staticmethod
    def online_public(lobby):
        return {**{key: lobby[key] for key in ('code', 'room', 'compatibility', 'name', 'address', 'port')},
                'players': 1 + len(lobby['peers']), 'capacity': 16}

    def online_request(self, method, path, query, body, remote, owner):
        if path == '/v2/lobbies' and method == 'GET':
            version = self.version(query.get('compatibility', [''])[0])
            return 200, {'lobbies': [self.online_public(lobby) for lobby in self.online.values()
                                    if lobby['listed'] and lobby['compatibility'] == version]}
        if path == '/v2/lobbies' and method == 'POST':
            version = self.version(body.get('compatibility'))
            name, port, listed = body.get('name'), body.get('port'), body.get('public', False)
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
            lobby = dict(code=code, room=secrets.token_hex(16), compatibility=version, name=name,
                         port=port, address=advertised, origin=remote, listed=listed, expires=self.clock()+TTL,
                         owner=hashlib.sha256(token.encode()).digest(), peers={}, next_peer=2)
            self.online[code] = lobby
            return 201, {**self.online_public(lobby), 'owner_token': token}
        match = re.fullmatch(r'/v2/lobbies/([A-Fa-f0-9]{16})(/join|/peers/(\d+))?', path)
        if not match:
            raise LobbyError(404, 'Unknown endpoint')
        lobby = self.online.get(match[1].upper())
        if not lobby:
            raise LobbyError(404, 'Lobby closed or expired')
        if match[2] == '/join' and method == 'POST':
            if self.version(body.get('compatibility')) != lobby['compatibility']:
                raise LobbyError(409, 'Incompatible game version or content')
            if len(lobby['peers']) >= 15:
                raise LobbyError(409, 'Lobby full')
            # Never reuse an ID in a room: stale packets cannot become another
            # player's identity after a disconnect. The address range holds 253.
            if lobby['next_peer'] > 254:
                raise LobbyError(409, 'Room exhausted; host a new lobby')
            peer_id = lobby['next_peer']
            lobby['next_peer'] += 1
            key = secrets.token_hex(32)
            lobby['peers'][peer_id] = dict(key=key, expires=self.clock()+TTL)
            return 201, {**self.online_public(lobby), 'peer_id': peer_id, 'peer_key': key}
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
            peer['expires'] = self.clock()+TTL
            return 200, self.online_public(lobby)
        if not owner or not hmac.compare_digest(lobby['owner'], hashlib.sha256(owner.encode()).digest()):
            raise LobbyError(403, 'Invalid lobby owner credential')
        if method == 'DELETE':
            del self.online[lobby['code']]
            return 200, {'closed': True}
        if method != 'PUT':
            raise LobbyError(405, 'Method not allowed')
        if self.version(body.get('compatibility')) != lobby['compatibility']:
            raise LobbyError(409, 'Cannot change compatibility during a session')
        lobby['expires'] = self.clock()+TTL
        return 200, {**self.online_public(lobby),
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


class Handler(BaseHTTPRequestHandler):
    server_version = 'TSFPLobbies/1'

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
            if not 0 <= length <= 4096:
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
        self.send_header('Connection', 'close')
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
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
