"""In-process HTTPS directory and shareable invitations for one-click hosting."""
import base64
import hashlib
import http.client
import ipaddress
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import threading

from lobby_server import Directory, Server
from online_client import Client, Session


def fetch_public_ipv4():
    """IPv4-only HTTPS endpoint; no lobby, credentials, or game data are sent."""
    connection = http.client.HTTPSConnection('api.ipify.org', timeout=5)
    try:
        connection.request('GET', '/?format=json')
        response = connection.getresponse()
        payload = response.read(1025)
        if response.status != 200 or len(payload) > 1024:
            raise ValueError('Unexpected IP lookup response')
        address = ipaddress.IPv4Address(json.loads(payload)['ip'])
        if not address.is_global:
            raise ValueError('IP lookup did not return a public IPv4')
        return str(address)
    except (OSError, http.client.HTTPException, ValueError, KeyError, TypeError) as error:
        raise ValueError('Could not detect your public IPv4. Try Detect again, or disable automatic '
                         'IP detection and enter it manually.') from error
    finally:
        connection.close()


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
            subprocess.run([openssl_path(), 'req', '-x509', '-newkey', 'rsa:2048', '-sha256',
                            '-nodes', '-days', '730', '-subj', '/CN=TimeSplitters Player Directory',
                            '-keyout', str(key), '-out', str(cert)],
                           check=True, capture_output=True, timeout=30,
                           creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        finally:
            lock.unlink()
    fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest()
    return cert, key, fingerprint


class LocalDirectory:
    def __init__(self, root, public_address, port=30820):
        address = ipaddress.IPv4Address(public_address)
        if address.is_loopback or address.is_unspecified or address.is_multicast:
            raise ValueError('Enter the public IPv4 friends use to reach your router.')
        self.server = Server(('0.0.0.0', port), Directory(public_address=str(address)))
        self.thread = None
        try:
            cert, key, self.fingerprint = certificate(Path(root)/'private/online-directory')
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(cert, key)
            self.server.socket = context.wrap_socket(self.server.socket, server_side=True,
                                                     do_handshake_on_connect=False)
            self.address = f'https://127.0.0.1:{self.server.server_port}'
            self.public_url = f'https://{address}:{self.server.server_port}'
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


def start_match(root, public_address, compatibility, name, port, public, directory_port=30820):
    if public_address is None:
        public_address = fetch_public_ipv4()
    if not public_address:
        raise ValueError('Enter your public IPv4 or enable automatic IP detection.')
    try:
        directory = LocalDirectory(root, public_address, directory_port)
    except OSError as error:
        if getattr(error, 'winerror', None) == 10048 or getattr(error, 'errno', None) == 98:
            raise ValueError('Directory port is already in use. Stop your standalone directory, '
                             'or turn off automatic hosting to use the existing directory.') from error
        raise
    try:
        active = Session.create(Client(directory.address, directory.fingerprint), compatibility, name, port, public)
        active.local_directory = directory
        active.invitation = invitation(directory.public_url, directory.fingerprint, active.lobby['code'])
        return active
    except Exception:
        directory.close()
        raise


def invitation(address, fingerprint, code):
    value = json.dumps(dict(directory=address, fingerprint=fingerprint, code=code), separators=(',', ':'))
    return 'tsfp-online:' + base64.urlsafe_b64encode(value.encode()).decode().rstrip('=')


def parse_invitation(value):
    if not value.startswith('tsfp-online:') or len(value) > 2048:
        raise ValueError('Paste a TimeSplitters Online invitation.')
    try:
        encoded = value[len('tsfp-online:'):]
        data = json.loads(base64.b64decode(encoded + '='*(-len(encoded) % 4), altchars=b'-_', validate=True))
        directory = Client(data['directory'], data['fingerprint'])
        code = data['code']
        if (directory.url.scheme != 'https' or not directory.fingerprint or
            len(code) != 16 or any(c not in '0123456789abcdefABCDEF' for c in code)):
            raise ValueError('Invalid invitation')
        return data['directory'], directory.fingerprint, code.upper()
    except (ValueError, TypeError, KeyError, AttributeError) as error:
        raise ValueError('Invalid Online invitation. Ask the host to copy it again.') from error
