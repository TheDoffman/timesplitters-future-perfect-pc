"""HTTPS directory client and the lifetime of a player-hosted online session."""

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

import hashlib
import http.client
import ipaddress
import json
import ssl
import threading
import time
from urllib.parse import urlsplit

from online_transport import Bridge


class Client:
    def __init__(self, address, fingerprint=''):
        self.url = urlsplit(address.rstrip('/'))
        if (self.url.scheme not in ('http', 'https') or not self.url.hostname or
            self.url.username or self.url.password or self.url.path not in ('', '/') or
            self.url.query or self.url.fragment):
            raise ValueError('Use a directory address such as https://hostname:30820')
        if self.url.scheme == 'http' and self.url.hostname not in ('127.0.0.1', 'localhost'):
            raise ValueError('HTTP is allowed only on localhost. Use HTTPS for other PCs.')
        self.fingerprint = fingerprint.replace(':', '').replace(' ', '').lower()
        if self.fingerprint and (len(self.fingerprint) != 64 or any(c not in '0123456789abcdef' for c in self.fingerprint)):
            raise ValueError('Certificate fingerprint must be 64 hexadecimal digits')

    def request(self, method, path, body=None, token=None):
        if self.url.scheme == 'https':
            context = ssl.create_default_context()
            if self.fingerprint:
                # Verify the pinned certificate before sending any credentials.
                context.check_hostname = False
                context.verify_mode = ssl.CERT_NONE
            connection = http.client.HTTPSConnection(self.url.hostname, self.url.port or 443, timeout=5, context=context)
        else:
            connection = http.client.HTTPConnection(self.url.hostname, self.url.port or 80, timeout=5)
        try:
            connection.connect()
            if self.fingerprint and (self.url.scheme != 'https' or
                hashlib.sha256(connection.sock.getpeercert(binary_form=True)).hexdigest() != self.fingerprint):
                raise ValueError('Directory certificate does not match the supplied fingerprint')
            headers = {'Content-Type': 'application/json'}
            if token:
                headers['Authorization'] = 'Bearer '+token
            connection.request(method, path, json.dumps(body).encode() if body is not None else None, headers)
            response = connection.getresponse()
            data = response.read(65537)
            if len(data) > 65536:
                raise ValueError('Directory response too large')
            result = json.loads(data)
            if not 200 <= response.status < 300:
                raise ValueError(result.get('error', f'Directory error {response.status}'))
            return result
        finally:
            connection.close()


class Session:
    def __init__(self, client, compatibility, lobby, host, bridge, mode='arcade'):
        self.client, self.compatibility, self.lobby = client, compatibility, lobby
        self.mode = mode
        self.host, self.bridge = host, bridge
        self.closed = threading.Event()
        self.error = ''
        self.local_directory = None
        self.thread = threading.Thread(target=bridge.run, daemon=True)
        self.thread.start()
        self.worker = threading.Thread(target=self.heartbeat, daemon=True)
        self.worker.start()

    @staticmethod
    def validate_mode(mode):
        if mode not in ('arcade', 'story'):
            raise ValueError('Unknown Online mode')

    @staticmethod
    def validate_lobby(lobby, mode):
        # Old directories can still serve Arcade, but cannot enforce Story limits.
        if (lobby.get('mode', 'arcade') != mode or
            (mode == 'story' and lobby.get('capacity') != 4)):
            raise ValueError('Directory does not support the requested Online mode')

    @classmethod
    def create(cls, client, compatibility, name, port, public, mode='arcade'):
        cls.validate_mode(mode)
        # Bind before advertising: a failed port bind must not leave a ghost lobby.
        bridge = Bridge(compatibility, '00'*16, bind=('0.0.0.0', port))
        lobby = None
        try:
            lobby = client.request('POST', '/v2/lobbies', dict(
                compatibility=compatibility, name=name, port=port, public=public, mode=mode))
            cls.validate_lobby(lobby, mode)
            bridge.room = bytes.fromhex(lobby['room'])
            return cls(client, compatibility, lobby, True, bridge, mode)
        except Exception:
            bridge.close()
            if lobby:
                try:
                    client.request('DELETE', '/v2/lobbies/'+lobby['code'], token=lobby['owner_token'])
                except Exception:
                    pass  # The directory also expires failed registrations.
            raise

    @classmethod
    def join(cls, client, compatibility, code, mode='arcade'):
        cls.validate_mode(mode)
        if len(code) != 16 or any(c not in '0123456789abcdefABCDEF' for c in code):
            raise ValueError('Lobby code must contain 16 hexadecimal characters')
        path = '/v2/lobbies/'+code.upper()
        lobby = client.request('POST', path+'/join', dict(compatibility=compatibility, mode=mode))
        bridge = None
        try:
            cls.validate_lobby(lobby, mode)
            address = str(ipaddress.IPv4Address(lobby['address']))
            bridge = Bridge(compatibility, lobby['room'], lobby['peer_id'], lobby['peer_key'],
                            host=(address, lobby['port']))
            session = cls(client, compatibility, lobby, False, bridge, mode)
        except Exception:
            if bridge:
                bridge.close()
            try:
                client.request('DELETE', path+'/peers/'+str(lobby['peer_id']), token=lobby['peer_key'])
            except Exception:
                pass
            raise
        if not bridge.ready.wait(12):
            session.close()
            raise ValueError('Host did not answer. Check the host UDP port forwarding and firewall.')
        return session

    def endpoint(self):
        path = '/v2/lobbies/'+self.lobby['code']
        if self.host:
            return path, self.lobby['owner_token']
        return path+'/peers/'+str(self.bridge.peer_id), self.lobby['peer_key']

    def heartbeat(self):
        last_success = time.monotonic()
        while not self.closed.is_set():
            try:
                path, token = self.endpoint()
                reply = self.client.request('PUT', path, dict(compatibility=self.compatibility, mode=self.mode), token)
                self.validate_lobby(reply, self.mode)
                if self.host:
                    self.bridge.update_peers(reply['peers'])
                last_success = time.monotonic()
                self.error = ''
            except Exception as error:
                self.error = str(error)
                if time.monotonic()-last_success > 60:
                    self.closed.set()
                    self.bridge.stopped.set()
                    break
            self.closed.wait(2 if self.host else 20)

    def close(self):
        self.closed.set()
        self.bridge.stopped.set()
        self.worker.join(timeout=6)
        self.thread.join(timeout=2)
        self.bridge.close()
        try:
            path, token = self.endpoint()
            self.client.request('DELETE', path, token=token)
        except Exception:
            pass  # Registrations also expire when a host/directory disappears.
        if self.local_directory:
            self.local_directory.close()
            self.local_directory = None
