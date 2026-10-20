"""Select affected source-only CI suites; manual runs always exercise everything."""
import json
import os
from pathlib import Path
import subprocess


SUITES = ('transitions', 'native', 'release', 'launcher')


def affected(paths, *, full=False):
    selected = set(SUITES) if full else set()
    for path in paths:
        if (path in ('.github/workflows/runtime-patches.yml', 'requirements.txt', 'tsfp.ps1',
                     'tools/ci-runtime-scope.py', 'tests/test_ci_runtime_scope.py',
                     'tools/apply-patches.py', 'tests/test_patch_tracking.py',
                     'tests/test_patch_updates.py', 'tests/fixtures/launcher-patch-stacks.json', 'tests/fixtures/renderer-patch-stacks.json') or path.startswith('patches/')):
            selected.update(SUITES)
        elif (path.startswith('tools/launcher/') or path in (
                '.github/workflows/launcher-release.yml', '.github/workflows/launcher-build.yml',
                '.github/workflows/launcher-compatibility.yml', 'tools/launcher-compatibility.py',
                'tests/test_launcher_compatibility.py', 'tools/launcher-release.py',
                'tools/fetch-launcher-art.ps1', 'config/launcher-artwork.json', 'tests/test_launcher_release.py',
                'tools/build-launcher.ps1', 'tools/build-mac-launcher.py', 'tools/test-mac-launcher.py', 'config/mac-launcher-sdk.json', 'tools/playable-launch.ps1',
                'tools/build-game-icon.ps1', 'tests/test_game_icon.py',
                'tests/launcher_updates.cs', 'tests/launcher_settings.cs', 'tests/launcher_maintenance.cs', 'tests/launcher_downloads.cs', 'tests/launcher_news.cs', 'tests/launcher_startup.cs',
                'tests/launcher_crashes.cs', 'tests/launcher_sessions.cs', 'tests/launcher_shared_ui.cs', 'tests/test_launcher_shared_ui.py',
                'tests/test_launcher_startup.py', 'tests/package_launcher.py',
                'tools/release/ImportGame.cs', 'config/pc-controls.default.json',
                'config/pc-discord.default.ini')):
            selected.add('launcher')
        elif path in ('tools/publish-release.py', 'tests/test_publish_release.py',
                      'tools/crash-symbols.py', 'tests/test_crash_symbols.py',
                      'tools/recycle-release-work.ps1', 'tools/package-release.py'):
            selected.add('release')
        elif path.startswith(('src/', 'tests/')) or path in (
                'tools/recomp-generation.py', 'tools/online.py', 'tools/online_theme.py',
                'tools/crash_helper.c', 'tools/bundle_online.py', 'tools/network-identity.py',
                'tools/build-native-mac.py', 'tools/play-native-mac.py',
                'tools/test-native-mac.py', 'tools/native_mac_common.py',
                'tools/testing_profile.py', 'tools/vulkan_sdk.py', 'tools/test-rhi-mac.py',
                'tools/frame_replay_contract.py', 'tools/replay-native-mac.py',
                'config/vulkan-sdk.json'):
            # Other paths admitted by the workflow are native/runtime inputs.
            selected.add('native')
        if path == 'src/pc_display_modes.inc':
            selected.add('launcher')  # Also embedded into the launcher EXE.
    return {suite: suite in selected for suite in SUITES}


def main():
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
    kind = os.environ['GITHUB_EVENT_NAME']
    base = (event['pull_request']['base']['sha'] if kind == 'pull_request'
            else event.get('before'))
    full = kind == 'workflow_dispatch' or not base or set(base) == {'0'}
    paths = []
    if not full:
        subprocess.run(['git', 'fetch', '--no-tags', '--depth=1', 'origin', base], check=True)
        changes = subprocess.check_output(['git', 'diff', '--no-renames', '--name-only', '-z', base, 'HEAD'])
        paths = changes.decode('utf-8').rstrip('\0').split('\0') if changes else []
    suites = affected(paths, full=full)
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
        for suite, enabled in suites.items():
            output.write(f'{suite}={str(enabled).lower()}\n')
    print('Selected suites: ' + ', '.join(suite for suite, enabled in suites.items() if enabled))


if __name__ == '__main__':
    main()
