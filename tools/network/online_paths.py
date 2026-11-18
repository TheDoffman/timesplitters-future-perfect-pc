"""Paths shared by source builds and the self-contained portable companion."""

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

import os
import json
import ipaddress
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PORTABLE = TOOLS.name == 'Online' and TOOLS.parent.name == 'Support'
ROOT = TOOLS.parents[1] if PORTABLE else _layout_root
HANDSHAKES = Path(os.environ.get('TSFP_ONLINE_HANDSHAKES', ROOT / ('Logs/Online' if PORTABLE else 'private/online')))
SETTINGS = ROOT / ('Settings/pc-online.json' if PORTABLE else 'config/pc-online.json')


def directory_route(address, fingerprint):
    """Optional device-local LAN route, scoped to an exact public URL and pin."""
    if not fingerprint or not address.startswith('https://'):
        return None
    path = Path(os.environ.get('TSFP_ONLINE_SETTINGS', SETTINGS)).with_name('pc-online-routes.json')
    try:
        with path.open(encoding='utf-8') as stream:
            raw = stream.read(16385)
        if len(raw) > 16384:
            return None
        routes = json.loads(raw).get('routes', [])
        if not isinstance(routes, list) or len(routes) > 4:
            return None
        for route in routes:
            if (not isinstance(route, dict) or route.get('directory', '').rstrip('/') != address.rstrip('/')
                    or route.get('fingerprint') != fingerprint):
                continue
            if not isinstance(route.get('local_address'), str):
                continue
            local = ipaddress.IPv4Address(route['local_address'])
            if any(local in ipaddress.IPv4Network(net) for net in
                   ('10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16')):
                return str(local)
    except (OSError, ValueError, TypeError, KeyError, AttributeError):
        pass
    return None


def service_defaults():
    """Public service address shipped with releases; never includes credentials."""
    path = TOOLS / 'online-service.json'
    defaults = json.loads(path.read_text(encoding='utf-8')) if path.is_file() else {}
    return dict(directory=os.environ.get('TSFP_ONLINE_DIRECTORY', defaults.get('directory', '')),
                fingerprint=os.environ.get('TSFP_ONLINE_FINGERPRINT', defaults.get('fingerprint', '')))


MANIFEST = Path(os.environ.get('TSFP_ONLINE_MANIFEST', TOOLS / 'network-identity.json' if PORTABLE else ROOT / ('build/macos/generated/network-identity.json' if sys.platform == 'darwin' else 'build/windows/generated/network-identity.json')))


def directory_folder(root):
    root = Path(root)
    return root / ('Settings/OnlineDirectory' if (root/'Support/Online/online.py').is_file()
                   else 'private/online-directory')
