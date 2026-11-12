"""Player-hosted HTTPS directory, started by the in-game Host a Server action.

Restores the former LocalDirectory/v2 player-hosted path (before267a53f4).
Community relay, migration coordinator, dashboard and service administration stay
separate. Match UDP runs between the player host and guests.
"""
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import socket
import ssl
import subprocess
import threading
import time
from datetime import datetime, timedelta, timezone
from urllib.parse import parse_qs, urlsplit
from online_paths import directory_folder
from online_host import fetch_public_ipv4, local_ipv4_addresses, lan_ipv4

VERSION = re.compile(r'[0-9a-f]{64}\Z')
TTL = 90
MAX_LOBBIES = 256
ONLINE_CAPACITY = {'arcade': 16, 'story': 4}

class LobbyError(Exception):
    def __init__(self, status, message):
        self.status, self.message = status, message


class Directory:
    def __init__(self, clock=time.monotonic, public_address=None):
        self.clock = clock
        self.public_address = str(ipaddress.IPv4Address(public_address)) if public_address else None
        self.lock = threading.Lock()
        self.online = {}

    def expire(self):
        now = self.clock()
        self.online = {key: lobby for key, lobby in self.online.items() if lobby['expires'] > now}
        for lobby in self.online.values():
            lobby['peers'] = {key: peer for key, peer in lobby['peers'].items() if peer['expires'] > now}

    def online_public(self, lobby, remote=''):
        result = {key: lobby[key] for key in ('code', 'room', 'compatibility', 'name', 'address', 'port', 'mode')}
        if remote and ipaddress.ip_address(remote).is_loopback and ipaddress.ip_address(lobby['origin']).is_loopback:
            result['address'] = lobby['origin']
        result['invite_only'] = not lobby['listed']
        result.update(transport='direct', players=1+len(lobby['peers']), capacity=ONLINE_CAPACITY[lobby['mode']])
        result['joinable'] = result['players'] < result['capacity']
        result.update(lobby.get('match_info', {}))
        return result

    @staticmethod
    def update_match_info(lobby, info):
        if info is None:
            return
        if (not isinstance(info, dict) or type(info.get('map_scene')) is not int or
            not 0 <= info['map_scene'] <= 100 or type(info.get('game_type_id')) is not int or
            not 0 <= info['game_type_id'] < 32 or type(info.get('in_progress')) is not bool):
            raise LobbyError(400, 'Invalid match details')
        for key, limit in (('map_name', 63), ('game_type', 31)):
            value = info.get(key)
            if not isinstance(value, str) or not 1 <= len(value) <= limit or any(ord(c) < 32 or ord(c) > 255 for c in value):
                raise LobbyError(400, 'Invalid match details')
        lobby['match_info'] = {key: info[key] for key in ('map_scene', 'game_type_id', 'map_name', 'game_type')}
        lobby['match_info']['phase'] = 'active' if info['in_progress'] else 'lobby'

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
                                    if lobby['compatibility'] == version and lobby['mode'] == mode]}
        if path == '/v2/lobbies' and method == 'POST':
            version = self.version(body.get('compatibility'))
            mode = self.online_mode(body.get('mode', 'arcade'))
            name, port, listed = body.get('name'), body.get('port'), body.get('public', False)
            transport = body.get('transport', 'direct')
            if transport != 'direct':
                raise LobbyError(400, 'Private directories use direct player-hosted traffic')
            if (not isinstance(name, str) or not 1 <= len(name) <= 48 or
                any(ord(c) < 32 or ord(c) > 126 for c in name) or type(port) is not int or
                not 1024 <= port <= 65535 or type(listed) is not bool):
                raise LobbyError(400, 'Invalid lobby name, port, or visibility')
            if len(self.online) >= MAX_LOBBIES or sum(l['origin'] == remote for l in self.online.values()) >= 8:
                raise LobbyError(429, 'Lobby limit reached')
            occupied = set(self.online) | {room['lobby_code'] for room in self.online.values()}
            code = secrets.token_hex(8).upper()
            while code in occupied:
                code = secrets.token_hex(8).upper()
            lobby_code = code
            if not listed:
                lobby_code = secrets.token_hex(8).upper()
                while lobby_code in occupied or lobby_code == code:
                    lobby_code = secrets.token_hex(8).upper()
            token = secrets.token_urlsafe(32)
            advertised = self.public_address if self.public_address and ipaddress.ip_address(remote).is_private else remote
            lobby = dict(code=code, room=secrets.token_hex(16), compatibility=version, name=name, mode=mode,
                         port=port, address=advertised, origin=remote, listed=listed, expires=self.clock()+TTL,
                         owner=hashlib.sha256(token.encode()).digest(), peers={}, next_peer=2,
                         transport=transport, lobby_code=lobby_code)
            self.online[code] = lobby
            return 201, {**self.online_public(lobby, remote), 'owner_token': token, 'lobby_code': lobby['lobby_code']}
        match = re.fullmatch(r'/v2/lobbies/([A-Fa-f0-9]{16})(/join|/peers/(\d+))?', path)
        if not match:
            raise LobbyError(404, 'Unknown endpoint')
        lobby = self.online.get(match[1].upper())
        if not lobby and match[2] == '/join' and method == 'POST':
            # Code-only invitations remain scoped to this connected directory.
            lobby = next((room for room in self.online.values()
                          if hmac.compare_digest(room['lobby_code'], match[1].upper())), None)
        if not lobby:
            raise LobbyError(404, 'Lobby closed or expired')
        if match[2] == '/join' and method == 'POST':
            supplied = body.get('lobby_code', match[1])
            supplied = supplied.upper() if isinstance(supplied, str) and re.fullmatch(r'[A-Fa-f0-9]{16}', supplied) else ''
            if not lobby['listed'] and not hmac.compare_digest(lobby['lobby_code'], supplied):
                raise LobbyError(403, 'Incorrect lobby code')
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
            peer = dict(key=key, expires=self.clock()+TTL)
            lobby['peers'][peer_id] = peer
            return 201, {**self.online_public(lobby, remote), 'peer_id': peer_id, 'peer_key': key}
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
            return 200, {'closed': True}
        if method != 'PUT':
            raise LobbyError(405, 'Method not allowed')
        if self.version(body.get('compatibility')) != lobby['compatibility']:
            raise LobbyError(409, 'Cannot change compatibility during a session')
        self.check_online_mode(lobby, body)
        self.update_match_info(lobby, body.get('match_info'))
        lobby['expires'] = self.clock()+TTL
        return 200, {**self.online_public(lobby, remote),
                     'peers': {str(i): p['key'] for i, p in lobby['peers'].items()}}

    @staticmethod
    def version(value):
        if not isinstance(value, str) or not VERSION.fullmatch(value):
            raise LobbyError(400, 'A valid compatibility identity is required')
        return value

    def request(self, method, path, query, body, remote, owner, local_address=""):
        with self.lock:
            self.expire()
            if not path.startswith('/v2/'):
                raise LobbyError(404, 'Unknown endpoint')
            status, reply = self.online_request(method, path, query, body, remote, owner)
            # The accepted socket's local address identifies a direct LAN route.
            # Never replace a public/WAN response with an internal match address.
            nearby = ipaddress.ip_address(remote).is_loopback or (lan_ipv4(remote) and lan_ipv4(local_address))
            if nearby:
                for item in reply.get('lobbies', [reply]):
                    lobby = self.online.get(item.get('code'))
                    if not lobby:
                        continue
                    origin = lobby['origin']
                    if lan_ipv4(origin):
                        item['address'] = origin
                    elif ipaddress.ip_address(origin).is_loopback and lan_ipv4(local_address):
                        item['address'] = local_address
            if path == '/v2/lobbies' and method == 'GET':
                reply['private_directory'] = True
                reply['invite_only_listing'] = True
            return status, reply


class Handler(BaseHTTPRequestHandler):
    server_version = 'TSFPPrivateDirectory/1'
    protocol_version = 'HTTP/1.1'

    def setup(self):
        super().setup()
        self.connection.settimeout(5)
        if isinstance(self.connection, ssl.SSLSocket):
            self.connection.do_handshake()

    def log_message(self, *args):
        pass  # Invitations and credentials never enter access logs.

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
            status, reply = self.server.directory.request(self.command, url.path, parse_qs(url.query),
                body, str(ipaddress.ip_address(self.client_address[0])), auth[7:] if auth.startswith('Bearer ') else '',
                local_address=self.connection.getsockname()[0])
        except LobbyError as error:
            status, reply = error.status, {'error': error.message}
        except (ValueError, UnicodeError):
            status, reply = 400, {'error': 'Invalid request body'}
        data = json.dumps(reply, separators=(',', ':')).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Connection', 'close')
        self.close_connection = True
        self.end_headers()
        self.wfile.write(data)

    do_GET = do_POST = do_PUT = do_DELETE = handle_request


class Server(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = not hasattr(socket, 'SO_EXCLUSIVEADDRUSE')

    def server_bind(self):
        if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

    def __init__(self, address, directory):
        self.directory = directory
        self.slots = threading.BoundedSemaphore(32)
        super().__init__(address, Handler)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()

    def handle_error(self, request, client_address):
        pass


def openssl_path():
    executable = shutil.which('openssl')
    if executable:
        return executable
    git = shutil.which('git')
    if git:
        executable = Path(git).parent.parent / 'usr/bin/openssl.exe'
        if executable.is_file():
            return str(executable)
    raise ValueError('OpenSSL was not found. Install Git for Windows, then restart the game.')


def certificate(folder):
    # Use the same small persistent certificate pair as Start-OnlineDirectory.ps1.
    for path in (folder.parent, folder, folder/'certificate.pem', folder/'private-key.pem'):
        if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
            raise ValueError('Refusing a redirected directory certificate path')
    folder.mkdir(parents=True, exist_ok=True)
    cert, key = folder/'certificate.pem', folder/'private-key.pem'
    if cert.exists() != key.exists():
        raise ValueError('Directory certificate/key pair is incomplete. Restore the matching pair.')
    if not cert.exists():
        # Exclusive lock protects first-run creation by simultaneous companions.
        lock = folder/'certificate.lock'
        try:
            descriptor = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        except FileExistsError:
            raise ValueError('Another directory is creating its certificate. Try again shortly.') from None
        try:
            os.close(descriptor)
            try:
                from cryptography import x509
                from cryptography.hazmat.primitives import hashes, serialization
                from cryptography.hazmat.primitives.asymmetric import rsa
            except ImportError:
                # Source environments can continue to use Git's OpenSSL.
                subprocess.run([openssl_path(), 'req', '-x509', '-newkey', 'rsa:2048', '-sha256',
                                '-nodes', '-days', '730', '-subj', '/CN=TimeSplitters Player Directory',
                                '-keyout', str(key), '-out', str(cert)],
                               check=True, capture_output=True, timeout=30,
                               creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            else:
                private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
                subject = x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,
                                                      'TimeSplitters Player Directory')])
                now = datetime.now(timezone.utc)
                signed = (x509.CertificateBuilder().subject_name(subject).issuer_name(subject)
                          .public_key(private_key.public_key()).serial_number(x509.random_serial_number())
                          .not_valid_before(now-timedelta(minutes=5)).not_valid_after(now+timedelta(days=730))
                          .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
                          .sign(private_key, hashes.SHA256()))
                key.write_bytes(private_key.private_bytes(serialization.Encoding.PEM,
                                serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
                cert.write_bytes(signed.public_bytes(serialization.Encoding.PEM))
        finally:
            lock.unlink()
    fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest()
    return cert, key, fingerprint


class LocalDirectory:
    direct = True
    def __init__(self, root, public_address, port=30820, *, state_folder=None):
        address = ipaddress.IPv4Address(public_address)
        if address.is_loopback or address.is_unspecified or address.is_multicast:
            raise ValueError('Enter the public IPv4 friends use to reach your router.')
        self.server = Server(('0.0.0.0', port), Directory(public_address=str(address)))
        self.thread = None
        try:
            cert, key, self.fingerprint = certificate(Path(state_folder) if state_folder is not None else directory_folder(root))
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(cert, key)
            self.server.socket = context.wrap_socket(self.server.socket, server_side=True,
                                                     do_handshake_on_connect=False)
            self.address = f'https://127.0.0.1:{self.server.server_port}'
            self.public_url = f'https://{address}:{self.server.server_port}'
            self.lan_addresses = local_ipv4_addresses(str(address))
            self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
            self.thread.start()
        except Exception:
            self.server.server_close()
            raise

    def close(self):
        if self.thread:
            self.server.shutdown()
            self.thread.join(timeout=3)
            self.thread = None
        self.server.server_close()

    def poll(self):
        return None if self.thread and self.thread.is_alive() else 0

    def terminate(self):
        self.close()

    def wait(self, timeout=None):
        if self.thread:
            self.thread.join(timeout)
            if self.thread.is_alive():
                raise subprocess.TimeoutExpired('private directory', timeout)
        return 0


def start_directory(root, request):
    address = request.get('address') or fetch_public_ipv4()
    return LocalDirectory(root, address, request['port'],
                          state_folder=os.environ.get('TSFP_PRIVATE_DIRECTORY_STATE'))
