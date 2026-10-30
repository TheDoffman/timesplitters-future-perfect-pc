"""Shared game release identity for packaging and publishing (no network access)."""

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

import re

VERSION = re.compile(r'v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([a-z][a-z0-9-]{0,39})\.([1-9][0-9]*))?\Z')
MINIMUM_LAUNCHER = '1.3.0'


def identity(version, *, publishing=False):
    match = VERSION.fullmatch(version)
    if not match:
        raise ValueError('Use X.Y.Z for Stable or X.Y.Z-channel.N for a named preview (N starts at 1).')
    channel = match[4] or 'stable'
    if match[4] == 'stable':
        raise ValueError('Stable versions must not have a prerelease suffix.')
    # Legacy releases stay readable for exact-version history and repair, but
    # must not be offered as Beta updates or used for new publication.
    if channel in ('alpha', 'preview', 'rc', 'legacy'):
        if publishing:
            raise ValueError('Use beta.N or a named feature such as story-coop.N for new test releases.')
        channel = 'legacy'
    return dict(schema=1, channel=channel,
                name='Stable' if channel == 'stable' else 'Beta' if channel == 'beta' else 'Legacy Preview' if channel == 'legacy' else channel.replace('-', ' ').title()+' Preview',
                minimum_launcher=MINIMUM_LAUNCHER)
