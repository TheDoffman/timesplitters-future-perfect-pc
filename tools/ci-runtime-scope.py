"""Select runtime work conservatively; launcher builds have one separate owner."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

spec = importlib.util.spec_from_file_location(
    'launcher_scope', Path(__file__).with_name('launcher-compatibility.py'))
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)

SUITES = ('transitions', 'native', 'gpu', 'release')
FULL_INPUTS = {
    '.github/workflows/runtime-patches.yml', 'requirements.txt', 'tsfp.ps1',
    'tools/ci-runtime-scope.py', 'tests/test_ci_runtime_scope.py',
    'tools/apply-patches.py', 'tests/test_patch_tracking.py',
    'tests/test_patch_updates.py', 'tests/fixtures/launcher-patch-stacks.json',
    'tests/fixtures/renderer-patch-stacks.json',
}
RELEASE_INPUTS = {
    'tools/publish-release.py', 'tests/test_publish_release.py',
    'tools/release_channels.py', 'tools/release-version.py',
    'tools/crash-symbols.py', 'tests/test_crash_symbols.py',
    'tools/recycle-release-work.ps1', 'tools/package-release.py',
    'tools/package-native-mac.py', 'tests/test_native_mac_package.py',
    'config/native-mac-runtime-licenses.json',
    'tools/build_textures.py', 'tests/test_build_textures.py',
    'tools/texture-release.py', 'tests/test_texture_release.py',
}
# Unknown runtime inputs still select GPU validation. New files must not
# silently lose coverage; only known offline scripts are exempted.
CPU_TOOLS = {
    'tools/x86_flow.py', 'tests/test_x86_flow.py',
    'tools/recomp_image.py', 'tools/x86_decode.py', 'tests/test_recomp_input.py',
    'tools/xbe_image.py', 'tools/xbe_kernel_exports.def', 'tests/test_xbe_image.py',
    'tools/disc_image.py', 'tools/import-game.py', 'tests/test_disc_image.py',
    'tools/recomp-generation.py', 'tools/online.py', 'tools/online_theme.py',
    'tools/bundle_online.py', 'tools/network-identity.py',
    'tools/play-native-mac.py', 'tools/test-native-mac.py',
    'tools/testing_profile.py', 'tests/test_native_mac_launcher.py',
    'tests/native_compiler.py',
}


def affected(paths, *, full=False):
    selected = set(SUITES) if full else set()
    for path in paths:
        if path in FULL_INPUTS:
            selected.update(SUITES)
        elif path.startswith('patches/'):
            # Patches can change arbitrary upstream source/CMake. Preserve
            # migration and rendering coverage on both hosts, not launcher builds.
            selected.update(('transitions', 'native', 'gpu'))
        elif path == 'requirements-release.txt':
            selected.add('native')  # Online HTTPS tests use the packaged certificate provider.
        elif path in RELEASE_INPUTS:
            selected.add('release')
        elif path in CPU_TOOLS or (
            path.endswith('.py') and path.startswith(
                ('tools/texture-', 'tests/texture_', 'tests/test_texture_'))
        ):
            selected.add('native')
        elif path.startswith('src/'):
            selected.update(('native', 'gpu'))
        elif launcher.relevant(path):
            continue  # launcher-build.yml owns these, including packaging/icons.
        elif path.startswith(('tools/issue-', 'tools/known-issues',
                              'tests/test_issue_', 'tests/test_known_issues')):
            continue  # Path-scoped issue workflows own these tests.
        elif path in ('tools/compatibility-worker.py', 'tests/test_compatibility_worker.py',
                      'tests/test_mac_backlog.py', 'tools/repo-context.py',
                      'tests/test_repo_context.py', 'tools/router/repository-context.mjs',
                      'tests/repository-context.test.mjs', 'tests/repository-evidence.test.mjs',
                      'tools/router/repository-evidence.mjs', 'tools/router/advisory-evidence.mjs'):
            continue  # The lightweight plan/gate jobs always run these contracts.
        elif path.startswith(('tools/', 'tests/', 'config/')):
            selected.update(('native', 'gpu'))
    return {suite: suite in selected for suite in SUITES}


def gate(plan, transitions, native, release, patches_result, native_result):
    """Preserve required names without passing failed or unexpectedly skipped work."""
    if plan != 'success' or any(v not in ('true', 'false')
                                for v in (transitions, native, release)):
        raise ValueError('Runtime scope failed or produced missing/invalid outputs.')
    for name, selected, result in (
        ('patch upgrades', transitions == 'true', patches_result),
        ('native/release', native == 'true' or release == 'true', native_result),
    ):
        expected = 'success' if selected else 'skipped'
        if result != expected:
            raise ValueError(f'{name}: expected {expected}, got {result!r}')
    return 'All selected Windows/Mac runtime checks passed.'


def main():
    if '--gate' in sys.argv:
        print(gate(*(os.environ.get(name, '') for name in (
            'PLAN_RESULT', 'RUN_TRANSITIONS', 'RUN_NATIVE', 'RUN_RELEASE',
            'PATCHES_RESULT', 'NATIVE_RESULT'))))
        return
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
