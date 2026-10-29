"""API-free scope and completion checks for Windows/Mac launcher CI."""

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
import os
from pathlib import Path
import re
import subprocess

POLICY_FILES = {
    'tools/project_paths.py', 'tests/conftest.py',
    '.github/workflows/launcher-compatibility.yml',
    '.github/workflows/launcher-build.yml', '.github/workflows/launcher-release.yml',
    'tools/ci/launcher-compatibility.py', 'tests/launcher/test_launcher_compatibility.py',
    'tools/release/launcher-release.py', 'tools/build/build-launcher.ps1',
    'tools/release/release_channels.py',
    'tools/build/build-mac-launcher.py', 'tools/build/fetch-launcher-art.ps1',
    'tools/play-native-mac.py', 'tools/testing/test-mac-launcher.py',
    'tools/playable-launch.ps1', 'config/build/mac-launcher-sdk.json',
    'config/build/launcher-artwork.json', 'config/pc-controls.default.json',
    'config/pc-discord.default.ini', 'src/interface/pc_display_modes.inc',
    'tests/launcher/test_native_mac_launcher.py', 'AGENTS.md',
    # Tests formerly owned by runtime-patches.yml now run with the launcher.
    'tools/build/build-game-icon.ps1', 'tools/build/build-online-icon.ps1', 'tests/repository/test_game_icon.py',
    'patches/platform/1150-game-icon.patch',
    'tests/support/native_compiler.py',
    'tests/launcher/package_launcher.py', 'tools/release/package-release.py',
    'tools/release/ImportGame.cs', 'requirements.txt',
    '.github/workflows/runtime-patches.yml', 'tools/ci/ci-runtime-scope.py',
    'tests/repository/test_ci_runtime_scope.py',
}


def relevant(path):
    return (path in POLICY_FILES or path.startswith('tools/launcher/') or
            path.startswith('tests/launcher/') or path.startswith('tests/launcher_lifecycle/'))


def exact_sha(value):
    if not isinstance(value, str) or not re.fullmatch('[0-9a-f]{40}', value):
        raise ValueError('Expected an exact commit SHA.')
    return value


def changed_paths(root, base, head):
    # Disabling rename detection retains both paths, including a launcher moved
    # out of the launcher directory. No network, secrets or shell expansion.
    raw = subprocess.check_output(['git', 'diff', '--no-ext-diff', '--no-textconv',
                                   '--no-renames', '--name-only', '-z',
                                   exact_sha(base), exact_sha(head)], cwd=root)
    return [p.decode('utf-8') for p in raw.split(b'\0') if p]


def gate(scope_result, should_build, build_result):
    if scope_result != 'success':
        raise ValueError('Launcher scope check did not complete successfully.')
    if should_build == 'true':
        if build_result != 'success':
            raise ValueError('Windows/Mac builds, tests and package verification must all pass.')
        return 'Windows and Mac launcher checks passed. OS behavior and visual parity still require review.'
    if should_build != 'false' or build_result != 'skipped':
        raise ValueError('Unexpected launcher validation result; refusing to pass.')
    return 'No launcher implementation changes; launcher builds were not needed.'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    scope = commands.add_parser('scope')
    scope.add_argument('--base')
    scope.add_argument('--head', required=True)
    scope.add_argument('--all', action='store_true')
    check = commands.add_parser('gate')
    check.add_argument('--scope', required=True)
    check.add_argument('--run', required=True)
    check.add_argument('--build', required=True)
    args = parser.parse_args()
    if args.command == 'gate':
        print(gate(args.scope, args.run, args.build))
        return
    head = exact_sha(args.head)
    if subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip() != head:
        raise ValueError('The validation checkout does not match the requested revision.')
    paths = [] if args.all else changed_paths('.', args.base, head)
    run = args.all or any(relevant(path) for path in paths)
    output = f'run={str(run).lower()}\nhead={head}\n'
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
            stream.write(output)
    print(output, end='')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as stream:
            stream.write('## Launcher compatibility\n\n' +
                         ('Windows and native Mac builds selected.' if run else 'No launcher implementation changes.') +
                         '\n\nUses standard hosted runners. No AI API, API key or automatic repair/merge is involved.\n')


if __name__ == '__main__':
    main()
