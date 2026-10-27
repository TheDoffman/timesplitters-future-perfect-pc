"""Keep heavyweight patch upgrades scoped without missing combined changes."""


import importlib.util
import json
import re
from pathlib import Path

import pytest

spec = importlib.util.spec_from_file_location('ci_runtime_scope', Path(__file__).resolve().parents[2] / 'tools/ci/ci-runtime-scope.py')
scope = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scope)


@pytest.mark.parametrize('path', ['vendor/ts3FPrecomp/snapshot.json',
                                 'tools/project_paths.py', 'tests/conftest.py',
                                 'vendor/ts3FPrecomp/source/src/kernel/kernel_bridge.c',
                                 'tools/build/prepare-runtime.py', 'tests/repository/test_runtime_snapshot.py'])
def test_bundled_runtime_changes_select_every_runtime_suite(path):
    assert all(scope.affected([path]).values())


def test_required_workflow_reports_on_every_pull_request():
    filename = 'runtime-patches.yml'
    workflow = (Path(__file__).resolve().parents[2] / '.github/workflows' / filename).read_text()
    trigger = re.search(r'^  pull_request:\n(.*?)(?=^  [a-z_]+:|^[^ \n]|\Z)', workflow, re.M | re.S)
    assert trigger, 'Required checks need a pull_request trigger.'
    assert not trigger[1].strip(), 'Required workflows must not skip PRs by path, branch or event type.'


@pytest.mark.parametrize('filename,inputs', [
    ('issue-forms.yml', {'tools/ci/issue-forms.py', 'tests/repository/test_issue_forms.py',
                         'tools/ci/known-issues.py', 'tests/repository/test_known_issues.py',
                         'VERSION', 'tools/launcher/LauncherVersion.cs'}),
    ('issue-resolution.yml', {'tools/ci/issue-resolution.py', 'tests/repository/test_issue_resolution.py'}),
])
def test_unrequired_issue_jobs_run_only_for_relevant_prs(filename, inputs):
    workflow = (Path(__file__).resolve().parents[2] / '.github/workflows' / filename).read_text()
    pr = workflow.split('  pull_request:\n', 1)[1].split('  push:\n', 1)[0]
    push = workflow.split('  push:\n', 1)[1].split('\npermissions:', 1)[0]
    assert '    paths:\n' in pr and '    paths:\n' in push
    for path in inputs:
        assert f'      - {path}\n' in pr
        assert f'      - {path}\n' in push


def test_version_refresh_cannot_push_directly_to_protected_main():
    workflow = (Path(__file__).resolve().parents[2] / '.github/workflows/issue-form-versions.yml').read_text()
    versions = workflow.split('\n  versions:\n', 1)[1]
    assert '--pull-request' in versions
    assert 'pull-requests: write' in versions
    assert 'git push' not in versions
    assert 'persist-credentials: false' in versions


@pytest.mark.parametrize('filename, job, check', [
    ('issue-forms.yml', 'validate', 'Issue form tests'),
    ('issue-resolution.yml', 'test', 'Issue resolution tests'),
])
def test_pr_issue_checks_contain_tests_only_and_keep_required_names(filename, job, check):
    workflow = (Path(__file__).resolve().parents[2] / '.github/workflows' / filename).read_text()
    triggers = workflow.split('\non:\n', 1)[1].split('\npermissions:\n', 1)[0]
    assert set(re.findall(r'^  ([a-z_]+):', triggers, re.M)) == {'pull_request', 'push'}
    jobs = workflow.split('\njobs:\n', 1)[1]
    assert re.findall(r'^  ([a-z_-]+):', jobs, re.M) == [job]
    assert f'    name: {check}\n' in jobs
    assert ': write' not in workflow
    assert 'GITHUB_TOKEN' not in workflow


@pytest.mark.parametrize('filename, events', [
    ('issue-labels.yml', {'issues'}),
    ('issue-form-versions.yml', {'push', 'release', 'workflow_dispatch'}),
    ('issue-resolution-comments.yml', {'pull_request_target'}),
])
def test_automation_is_not_scheduled_for_open_prs(filename, events):
    workflow = (Path(__file__).resolve().parents[2] / '.github/workflows' / filename).read_text()
    triggers = workflow.split('\non:\n', 1)[1].split('\npermissions:\n', 1)[0]
    assert set(re.findall(r'^  ([a-z_]+):', triggers, re.M)) == events
    if filename == 'issue-labels.yml':
        assert 'types: [opened, edited, reopened]' in triggers
        assert 'issues: write' in workflow and 'contents: write' not in workflow
        assert 'python tools/ci/issue-forms.py labels' in workflow
    elif filename == 'issue-form-versions.yml':
        assert 'types: [published, edited, deleted, unpublished]' in triggers
        assert 'branches: [main]' in triggers
        assert 'needs: validate' in workflow
        assert 'python -m unittest discover -s tests/repository -p test_issue_forms.py -v' in workflow
        # Release tags (including old/deleted ones) must not choose the code
        # used either to validate the references or to propose the update.
        assert workflow.count('ref: ${{ github.event.repository.default_branch }}') == 2
    else:
        assert 'types: [closed]' in triggers and 'branches: [main]' in triggers
        assert 'github.event.pull_request.merged == true' in workflow
        assert 'ref: ${{ github.event.repository.default_branch }}' in workflow
        assert 'persist-credentials: false' in workflow
        assert 'python tools/ci/issue-resolution.py' in workflow


@pytest.mark.parametrize('paths, expected', [
    (['tools/launcher/MainWindow.cs'], set()),
    (['tools/build/build-mac-launcher.py'], set()),
    (['tests/launcher/test_launcher_startup.py', 'tests/launcher/package_launcher.py', 'tests/repository/test_game_icon.py'], set()),
    (['tools/build/build-game-icon.ps1', 'config/build/mac-launcher-sdk.json'], set()),
    (['.github/workflows/launcher-build.yml', '.github/workflows/launcher-compatibility.yml'], set()),
    (['tools/play-native-mac.py', 'tests/launcher/test_native_mac_launcher.py'], {'native'}),
    (['tools/launcher/MainWindow.cs', 'src/input/pc_controls.c'], {'native', 'gpu'}),
    (['src/interface/pc_display.c'], {'native', 'gpu'}),
    (['src/interface/pc_display_modes.inc'], {'native', 'gpu'}),
    (['src/native/renderer_gl.c', 'src/graphics/menu_sampling_shader.h'], {'native', 'gpu'}),
    (['src/new_component.c', 'tests/new_test.py'], {'native', 'gpu'}),
    (['tools/new_runtime_tool.py'], {'native', 'gpu'}),
    (['tools/textures/texture-pack.py', 'tests/textures/texture_pack_tools.py'], {'native'}),
    (['tools/textures/texture-pack.py', 'src/textures/texture_pack.c'], {'native', 'gpu'}),
    (['tests/textures/texture_toggle.c'], {'native', 'gpu'}),
    (['tools/textures/texture-pack.py', 'patches/texture-mode.patch'], {'transitions', 'native', 'gpu'}),
    (['patches/legacy/old.patch'], {'transitions', 'native', 'gpu'}),
    (['tools/testing/test-rhi-mac.py', 'tests/rhi/CMakeLists.txt'], {'native', 'gpu'}),
    (['tools/build/vulkan_sdk.py', 'config/build/vulkan-sdk.json'], {'native', 'gpu'}),
    (['tools/build/build-native-mac.py', 'tools/build/native_mac_common.py'], {'native', 'gpu'}),
    (['tools/testing/testing_profile.py'], {'native'}),
    (['tests/support/native_compiler.py'], {'native'}),
    (['tools/network/online_theme.py', 'tools/network/bundle_online.py', 'tools/build/network-identity.py'], {'native'}),
    (['tools/build/crash_helper.c', 'src/diagnostics/pc_crash.c', 'tests/diagnostics/test_crash_capture.py'], {'native', 'gpu'}),
    (['tools/release/publish-release.py', 'tests/launcher/test_publish_release.py', 'docs/release/releasing.md'], {'release'}),
    (['tools/release/crash-symbols.py', 'tests/diagnostics/test_crash_symbols.py'], {'release'}),
    (['tools/release/package-release.py'], {'release'}),
    (['tools/textures/texture-release.py'], {'release'}),
    (['tests/textures/test_texture_release.py'], {'release'}),
    (['tools/build/apply-patches.py'], set(scope.SUITES)),
    (['tests/fixtures/launcher-patch-stacks.json'], set(scope.SUITES)),
    (['tsfp.ps1', 'requirements.txt'], set(scope.SUITES)),
    (['requirements-release.txt'], {'native'}),
    (['.github/workflows/runtime-patches.yml'], set(scope.SUITES)),
    (['tools/ci/issue-forms.py', 'tests/repository/test_issue_resolution.py'], set()),
    (['README.md', 'docs/textures/textures.md', 'docs/project/MAC_PORT_BACKLOG.md'], set()),
    ([], set()),
])
def test_scope(paths, expected):
    actual = scope.affected(paths)
    assert {name for name, enabled in actual.items() if enabled} == expected
    assert not actual['gpu'] or actual['native']


def test_manual_and_new_branch_runs_cover_everything():
    assert all(scope.affected([], full=True).values())


@pytest.mark.parametrize('transitions,native,release,patches,result', [
    ('false', 'false', 'false', 'skipped', 'skipped'),
    ('true', 'true', 'true', 'success', 'success'),
    ('true', 'false', 'false', 'success', 'skipped'),
    ('false', 'true', 'false', 'skipped', 'success'),
    ('false', 'false', 'true', 'skipped', 'success'),
])
def test_gate_accepts_only_selected_work(transitions, native, release, patches, result):
    assert 'passed' in scope.gate('success', transitions, native, release, patches, result)


@pytest.mark.parametrize('bad', ['failure', 'cancelled', 'skipped', '', 'in_progress'])
def test_required_gate_rejects_unsuccessful_selected_work(bad):
    with pytest.raises(ValueError):
        scope.gate(bad, 'true', 'true', 'false', 'success', 'success')
    with pytest.raises(ValueError):
        scope.gate('success', 'true', 'true', 'false', bad, 'success')
    with pytest.raises(ValueError):
        scope.gate('success', 'true', 'true', 'false', 'success', bad)


@pytest.mark.parametrize('bad', ['', 'yes', 'TRUE', 'null'])
def test_gate_rejects_missing_scope_outputs(bad):
    for index in range(3):
        flags = ['false', 'false', 'false']
        flags[index] = bad
        with pytest.raises(ValueError):
            scope.gate('success', *flags, 'skipped', 'skipped')


def test_unselected_work_cannot_hide_an_unexpected_failure():
    with pytest.raises(ValueError):
        scope.gate('success', 'false', 'false', 'false', 'failure', 'skipped')


def test_parallel_workers_preserve_required_check_contract():
    workflow = (Path(__file__).resolve().parents[2] /
                '.github/workflows/runtime-patches.yml').read_text()
    patches = workflow.split('\n  patches:\n')[1].split('\n  native:\n')[0]
    native = workflow.split('\n  native:\n')[1].split('\n  patch-transitions:\n')[0]
    gate = workflow.split('\n  patch-transitions:\n')[1]
    for worker in (patches, native):
        assert '    needs: plan\n' in worker
        assert 'os: [windows-latest, macos-latest]' in worker
    assert 'needs: patches' not in native
    assert 'python tools/build/apply-patches.py external/ts3FPrecomp patches' in native
    assert "needs.plan.outputs.native == 'true' && runner.os == 'Windows'" in native
    assert 'python -m pip install -r requirements-release.txt' in native
    assert "needs.plan.outputs.gpu == 'true' && runner.os == 'macOS'" in native
    assert 'name: Patch transitions (${{ matrix.os }})' in gate
    assert 'needs: [plan, patches, native]' in gate and 'if: always()' in gate
    assert 'python tools/ci/ci-runtime-scope.py --gate' in gate
    assert 'runs-on: ${{ matrix.os }}' in gate
    assert 'test_compatibility_worker.py' in gate
    for expression in ('needs.plan.result', 'needs.plan.outputs.transitions',
                       'needs.plan.outputs.native', 'needs.plan.outputs.release',
                       'needs.patches.result', 'needs.native.result'):
        assert '${{ ' + expression + ' }}' in gate


def test_single_gate_covers_runtime_and_launcher_without_changing_old_checks():
    workflow = (Path(__file__).resolve().parents[2] /
                '.github/workflows/runtime-patches.yml').read_text()
    plan = workflow.split('\n  plan:\n', 1)[1].split('\n  patches:\n', 1)[0]
    launcher = workflow.split('\n  launcher:\n', 1)[1].split('\n  pr-validation:\n', 1)[0]
    gate = workflow.split('\n  pr-validation:\n', 1)[1].split('\n  # Preserve', 1)[0]
    assert 'python tools/ci/launcher-compatibility.py scope' in plan
    assert 'launcher: ${{ steps.launcher.outputs.run }}' in plan
    assert 'uses: ./.github/workflows/launcher-build.yml' in launcher
    assert 'source_ref: ${{ needs.plan.outputs.head }}' in launcher
    assert 'name: PR validation' in gate
    assert 'needs: [plan, patches, native, launcher]' in gate
    assert 'if: always()' in gate
    assert 'python tools/ci/ci-pr-gate.py' in gate


def test_launcher_build_and_moved_tests_have_one_owner():
    root = Path(__file__).resolve().parents[2]
    runtime = (root / '.github/workflows/runtime-patches.yml').read_text()
    launcher = (root / '.github/workflows/launcher-build.yml').read_text()
    for command in ('./tools/build/build-launcher.ps1 -Test',
                    'python tools/build/build-mac-launcher.py --install-sdk --test'):
        assert command not in runtime and command in launcher
    for test in ('tests/launcher/test_launcher_startup.py', 'tests/launcher/package_launcher.py',
                 'tests/repository/test_game_icon.py'):
        assert test not in runtime and test in launcher
    for pin in ('pytest==', 'Pillow==', 'pefile==', 'ninja==', 'cmake=='):
        assert pin in launcher


@pytest.mark.parametrize('kind', ['pull_request', 'push', 'workflow_dispatch', 'new_branch'])
def test_event_routing_and_outputs(tmp_path, monkeypatch, kind):
    base = 'a' * 40
    event = {'pull_request': {'base': {'sha': base}}} if kind == 'pull_request' else {'before': base}
    if kind == 'new_branch':
        event['before'] = '0' * 40
    event_file, output = tmp_path / 'event.json', tmp_path / 'output'
    event_file.write_text(json.dumps(event))
    monkeypatch.setenv('GITHUB_EVENT_PATH', str(event_file))
    monkeypatch.setenv('GITHUB_OUTPUT', str(output))
    monkeypatch.setenv('GITHUB_EVENT_NAME', 'push' if kind == 'new_branch' else kind)
    calls = []
    monkeypatch.setattr(scope.subprocess, 'run', lambda args, **kwargs: calls.append(args))
    def diff(args):
        calls.append(args)
        return b'tools/release/publish-release.py\0docs/release/releasing.md\0'
    monkeypatch.setattr(scope.subprocess, 'check_output', diff)
    scope.main()
    result = dict(line.split('=') for line in output.read_text().splitlines())
    if kind in ('workflow_dispatch', 'new_branch'):
        assert all(value == 'true' for value in result.values())
        assert not calls
    else:
        assert result == dict(transitions='false', native='false', gpu='false', release='true')
        assert calls[0] == ['git', 'fetch', '--no-tags', '--depth=1', 'origin', base]
        assert calls[1] == ['git', 'diff', '--no-renames', '--name-only', '-z', base, 'HEAD']
