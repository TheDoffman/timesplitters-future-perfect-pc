"""Player-side invitation encoding and connection validation."""

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

import base64
import http.client
import ipaddress
import json
from online_client import Client



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
        if (directory.url.scheme != 'https' or
            len(code) != 16 or any(c not in '0123456789abcdefABCDEF' for c in code)):
            raise ValueError('Invalid invitation')
        return data['directory'], directory.fingerprint, code.upper()
    except (ValueError, TypeError, KeyError, AttributeError) as error:
        raise ValueError('Invalid Online invitation. Ask the host to copy it again.') from error
