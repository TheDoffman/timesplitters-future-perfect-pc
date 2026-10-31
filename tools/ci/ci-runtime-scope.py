"""Select runtime work conservatively; launcher builds have one separate owner."""

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
    'tools/project_paths.py', 'tests/conftest.py',
    'tests/support/parallel_groups.py', 'tests/support/test_parallel_groups.py',
    'tools/testing/suite_timing.py', 'tests/support/test_suite_timing.py',
    '.gitattributes',
    '.github/workflows/runtime-patches.yml', 'requirements.txt', 'tsfp.ps1',
    'tools/ci/ci-runtime-scope.py', 'tests/repository/test_ci_runtime_scope.py',
    'tools/build/apply-patches.py', 'tests/repository/test_patch_tracking.py',
    'tools/build/prepare-runtime.py', 'tests/repository/test_runtime_snapshot.py',
    'tests/repository/test_patch_updates.py', 'tests/fixtures/launcher-patch-stacks.json',
    'tests/fixtures/renderer-patch-stacks.json',
}
RELEASE_INPUTS = {
    'tools/build/benchmark-patches.py', 'tools/release/benchmark-verification.py',
    'tools/release/publish-release.py', 'tests/launcher/test_publish_release.py',
    'tools/release/release_channels.py', 'tools/release/release-version.py',
    'tools/release/crash-symbols.py', 'tests/diagnostics/test_crash_symbols.py',
    'tools/release/recycle-release-work.ps1', 'tools/release/package-release.py',
    'tools/release/package-native-mac.py', 'tests/launcher/test_native_mac_package.py',
    'config/build/native-mac-runtime-licenses.json',
    'tools/textures/build_textures.py', 'tests/textures/test_build_textures.py',
    'tools/textures/texture-release.py', 'tests/textures/test_texture_release.py',
}
# Unknown runtime inputs still select GPU validation. New files must not
# silently lose coverage; only known offline scripts are exempted.
CPU_TOOLS = {
    'tests/compiler/test_guest_abi.py',
    'tests/compiler/test_guest_call_frames.py',
    'tests/compiler/test_guest_dispatch.py',
    'tests/compiler/test_translation_batch.py',
    'tests/compiler/test_translation_cli.py',
    'tests/compiler/test_translation_compare.py',
    'tests/compiler/test_translation_conditions.py',
    'tests/compiler/test_translation_context.py',
    'tests/compiler/test_translation_control.py',
    'tests/compiler/test_translation_discovery.py',
    'tests/compiler/test_translation_exchange.py',
    'tests/compiler/test_translation_function.py',
    'tests/compiler/test_translation_lifting.py',
    'tests/compiler/test_translation_mmx.py',
    'tests/compiler/test_translation_sse.py',
    'tests/compiler/test_translation_strings.py',
    'tests/compiler/test_translation_x87.py',
    'tools/compiler/generate_host_symbols.py',
    'tools/compiler/translation_batch.py',
    'tools/compiler/translation_cli.py',
    'tools/compiler/translation_compare.py',
    'tools/compiler/translation_conditions.py',
    'tools/compiler/translation_context.py',
    'tools/compiler/translation_control.py',
    'tools/compiler/translation_discovery.py',
    'tools/compiler/translation_exchange.py',
    'tools/compiler/translation_function.py',
    'tools/compiler/translation_helpers.py',
    'tools/compiler/translation_host_symbols.json',
    'tools/compiler/translation_host_symbols.txt',
    'tools/compiler/translation_lifting.py',
    'tools/compiler/translation_mmx.py',
    'tools/compiler/translation_names.py',
    'tools/compiler/translation_sse.py',
    'tools/compiler/translation_strings.py',
    'tools/compiler/translation_x87.py',

    'tools/compiler/translation_product.py', 'tests/compiler/test_translation_product.py',
    'tools/compiler/translation_bitops.py', 'tests/compiler/test_translation_bitops.py',
    'tools/compiler/translation_bits.py', 'tests/compiler/test_translation_bits.py',
    'tools/compiler/translation_integer.py', 'tests/compiler/test_translation_integer.py',
    'tools/compiler/translation_moves.py', 'tests/compiler/test_translation_moves.py',
    'tools/compiler/translation_operands.py', 'tests/compiler/test_translation_operands.py',
    'tools/compiler/branch_feedback.py', 'tests/compiler/test_branch_feedback.py',
    'tools/compiler/unresolved_report.py', 'tests/compiler/test_unresolved_report.py',
    'tools/compiler/xdk_catalog.py', 'tools/compiler/compile_pipeline.py',
    'tests/compiler/test_xdk_catalog.py', 'tests/compiler/test_compile_pipeline.py',
    'tools/compiler/translation_decode.py', 'tests/compiler/test_translation_decode.py',
    'tools/compiler/translation_layout.py', 'tests/compiler/test_translation_layout.py',
    'tools/compiler/translation_hooks.py', 'tests/compiler/test_translation_hooks.py',
    'tools/compiler/c_source_scan.py', 'tools/compiler/generation_output.py',
    'tests/compiler/test_c_source_scan.py', 'tests/compiler/test_generation_output.py',
    'tools/compiler/function_evidence.py', 'tools/compiler/function_categories.py', 'tools/compiler/function_catalog.py',
    'tests/compiler/test_function_evidence.py', 'tests/compiler/test_function_categories.py', 'tests/compiler/test_function_catalog.py',
    'tools/compiler/analysis_cache.py', 'tools/compiler/analysis_config.py', 'tools/compiler/analysis_output.py', 'tools/compiler/analysis_runner.py',
    'tests/compiler/test_analysis_cache.py', 'tests/compiler/test_analysis_runner.py',
    'tools/compiler/x86_flow.py', 'tests/compiler/test_x86_flow.py',
    'tools/compiler/x86_recovery.py', 'tests/compiler/test_x86_recovery.py',
    'tools/compiler/x86_functions.py', 'tools/compiler/recomp_symbols.py',
    'tests/compiler/test_x86_functions.py', 'tests/compiler/test_recomp_symbols.py',
    'tools/compiler/recomp_image.py', 'tools/compiler/x86_decode.py', 'tests/compiler/test_recomp_input.py',
    'tools/compiler/xbe_image.py', 'tools/compiler/xbe_kernel_exports.def', 'tests/compiler/test_xbe_image.py',
    'tools/compiler/disc_image.py', 'tools/build/import-game.py', 'tests/compiler/test_disc_image.py',
    'tools/build/recomp-generation.py', 'tools/network/online.py', 'tools/network/online_theme.py',
    'tools/network/bundle_online.py', 'tools/build/network-identity.py',
    'tools/play-native-mac.py', 'tools/testing/test-native-mac.py',
    'tools/testing/testing_profile.py', 'tests/launcher/test_native_mac_launcher.py',
    'tests/support/native_compiler.py',
}


def affected(paths, *, full=False):
    selected = set(SUITES) if full else set()
    for path in paths:
        if path in FULL_INPUTS or path.startswith('vendor/'):
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
                ('tools/textures/', 'tests/textures/'))
        ):
            selected.add('native')
        elif path.startswith('src/'):
            selected.update(('native', 'gpu'))
        elif launcher.relevant(path):
            continue  # launcher-build.yml owns these, including packaging/icons.
        elif path.startswith(('tools/ci/issue-', 'tools/ci/known-issues',
                              'tests/repository/test_issue_', 'tests/repository/test_known_issues')):
            continue  # Path-scoped issue workflows own these tests.
        elif path in ('tools/ci/compatibility-worker.py', 'tests/repository/test_compatibility_worker.py',
                      'tests/repository/test_mac_backlog.py', 'tools/repo-context.py',
                      'tests/repository/test_repo_context.py', 'tools/router/repository-context.mjs',
                      'tests/repository/repository-context.test.mjs', 'tests/repository/repository-evidence.test.mjs',
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
