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
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PORTABLE = TOOLS.name == 'Online' and TOOLS.parent.name == 'Support'
ROOT = TOOLS.parents[1] if PORTABLE else _layout_root
HANDSHAKES = Path(os.environ.get('TSFP_ONLINE_HANDSHAKES', ROOT / ('Logs/Online' if PORTABLE else 'private/online')))
SETTINGS = ROOT / ('Settings/pc-online.json' if PORTABLE else 'config/pc-online.json')
MANIFEST = Path(os.environ.get('TSFP_ONLINE_MANIFEST', TOOLS / 'network-identity.json' if PORTABLE else ROOT / ('build/macos/generated/network-identity.json' if sys.platform == 'darwin' else 'build/windows/generated/network-identity.json')))


def directory_folder(root):
    root = Path(root)
    return root / ('Settings/OnlineDirectory' if (root/'Support/Online/online.py').is_file()
                   else 'private/online-directory')
