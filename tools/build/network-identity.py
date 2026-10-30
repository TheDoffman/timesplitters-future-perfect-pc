"""Generate a strict, path-independent PC multiplayer compatibility identity.

Hash game data and native source/patches, never user settings or saves. A rebuild
after gameplay/content edits produces a new identity. Cosmetic-only differences
are conservatively incompatible until explicitly classified by the protocol.
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
import json
from pathlib import Path

PROTOCOL = 'tsfp-pc-system-link-v1'


def game_entries(game):
    entries = []
    # Match both import methods: title executable and asset trees. Xbox dash
    # updaters and runtime CACHE files are outside the playable content set.
    # In particular, launching the game must not change its own identity.
    for child in game.iterdir():
        if child.is_file() and child.name.lower() == 'default.xbe':
            paths = [child]
        elif child.is_dir() and child.name.lower() in ('pak', 'xmv'):
            paths = (path for path in child.rglob('*') if path.is_file())
        else:
            continue
        for path in paths:
            with path.open('rb') as stream:
                digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            entries.append(('game/' + path.relative_to(game).as_posix().lower(), digest))
    if not any(name == 'game/default.xbe' for name, _ in entries):
        raise ValueError('Missing game/default.xbe')
    if len({name for name, _ in entries}) != len(entries):
        raise ValueError('Duplicate case-insensitive game content paths')
    return sorted(entries)


def verify_content(game, manifest):
    expected = {name: digest for name, digest in manifest['files'] if name.startswith('game/')}
    actual = dict(game_entries(game))
    if manifest.get('protocol') == PROTOCOL and actual == expected:
        return
    missing = sorted(expected.keys() - actual.keys())
    extra = sorted(actual.keys() - expected.keys())
    changed = sorted(name for name in expected.keys() & actual.keys() if expected[name] != actual[name])
    details = []
    for label, names in [('Missing', missing), ('Changed', changed), ('Unexpected', extra)]:
        if names:
            details.append(f'{label}: '+', '.join(name.removeprefix('game/') for name in names[:3]))
    if manifest.get('protocol') != PROTOCOL:
        details.append('Unsupported compatibility manifest')
    raise ValueError('Game content differs from the multiplayer build. '
                     'Use matching game files and the same package version on every PC. '
                     'Source builds: run tsfp.ps1 Build again.\n'+'\n'.join(details))


def executable_hash(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_build(executable, manifest, stamp):
    if (stamp.get('compatibility') != manifest['compatibility'] or
        stamp.get('executable_sha256') != executable_hash(executable)):
        raise ValueError('Executable does not match the completed multiplayer build. Run tsfp.ps1 Build again.')


def identity(root, game, runtime=None):
    entries = []
    if (root/'VERSION').is_file():
        entries.append(('VERSION', hashlib.sha256((root/'VERSION').read_bytes().replace(b'\r\n', b'\n')).hexdigest()))
    for folder in ('src', 'patches'):
        for path in sorted((root / folder).rglob('*')):
            if path.is_file():
                # Normalize text line endings so checkout configuration does not
                # separate otherwise identical Windows/Linux source checkouts.
                data = path.read_bytes().replace(b'\r\n', b'\n')
                entries.append((path.relative_to(root).as_posix(), hashlib.sha256(data).hexdigest()))
    for name in ('online_transport.py', 'online_client.py', 'online.py', 'online_host.py', 'online_paths.py', 'online_theme.py', 'online_platform.py', 'lobby_server.py'):
        path = root / 'tools/network' / name
        if path.is_file():
            entries.append(('tools/' + name, hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()))
    # Include the actual runtime and lifted code, not just patch intentions.
    # Local edits to upstream code must also produce an incompatible build.
    runtime = runtime or root / 'external/ts3FPrecomp'
    for folder in ('src', 'include', 'titles/futureperfect'):
        for path in sorted((runtime / folder).rglob('*')):
            if path.is_file() and (path.suffix.lower() in ('.c', '.h', '.inc') or path.name == 'CMakeLists.txt'):
                data = path.read_bytes().replace(b'\r\n', b'\n')
                entries.append(('runtime/' + path.relative_to(runtime).as_posix(), hashlib.sha256(data).hexdigest()))
    entries.extend(game_entries(game))
    # pathlib sorts Windows paths case-insensitively and POSIX paths by case.
    # Hash one canonical sequence of portable strings on every host.
    entries.sort(key=lambda entry: entry[0])
    payload = json.dumps({'protocol': PROTOCOL, 'files': entries}, separators=(',', ':'), sort_keys=True)
    return hashlib.sha256(payload.encode()).digest(), entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=_layout_root)
    parser.add_argument('--game', type=Path)
    parser.add_argument('--runtime', type=Path, help='Same-host patched runtime used by this build')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--verify', action='store_true', help='Reject game-data changes since the last build')
    parser.add_argument('--executable', type=Path, help='Also check the completed build executable during verification')
    parser.add_argument('--seal-executable', type=Path, help='Record the executable after a successful build')
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output or root / 'build/windows/generated'
    if args.seal_executable:
        manifest = json.loads((output / 'network-identity.json').read_text())
        stamp = dict(compatibility=manifest['compatibility'], executable_sha256=executable_hash(args.seal_executable))
        temporary = output / 'network-build.json.tmp'
        temporary.write_text(json.dumps(stamp, indent=2) + '\n')
        temporary.replace(output / 'network-build.json')
        return
    if args.verify:
        manifest = json.loads((output / 'network-identity.json').read_text())
        if args.executable:
            verify_build(args.executable, manifest, json.loads((output / 'network-build.json').read_text()))
        verify_content(args.game or root / 'private/game', manifest)
        print('Multiplayer game-content verification passed')
        return
    digest, entries = identity(root, args.game or root / 'private/game', args.runtime)
    output.mkdir(parents=True, exist_ok=True)
    header = ('/* Generated by tools/build/network-identity.py; do not edit. */\n'
              '#define TSFP_NETWORK_COMPATIBILITY {' + ','.join(f'0x{b:02x}' for b in digest) + '}\n'
              f'#define TSFP_NETWORK_COMPATIBILITY_HEX "{digest.hex()}"\n')
    target = output / 'pc_network_build.h'
    if not target.exists() or target.read_text() != header:
        target.write_text(header)
    (output / 'network-identity.json').write_text(json.dumps(
        {'protocol': PROTOCOL, 'compatibility': digest.hex(), 'files': entries}, indent=2) + '\n')
    print(f'Multiplayer compatibility: {digest.hex()} ({len(entries)} files)')


if __name__ == '__main__':
    main()
