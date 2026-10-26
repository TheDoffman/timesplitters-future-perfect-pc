"""Exercise real runtime patches on small source-only checkout fixtures."""
from pathlib import Path
import importlib.util
import hashlib
import io
import json
import os
import re
from collections import Counter
import shutil
import subprocess
import sys
import tarfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / 'patches'
RUNTIME = ROOT / 'external/ts3FPrecomp'
spec = importlib.util.spec_from_file_location('apply_patches', ROOT / 'tools/apply-patches.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


@pytest.fixture(scope='session')
def upstream_files():
    # Read pristine bytes once, rather than starting git show for each file in
    # every test. Never reuse a mutable checkout between transition scenarios.
    names = {line[6:] for patch in PATCHES.rglob('*.patch')
             for line in patch.read_text(encoding='utf-8').splitlines()
             if line.startswith('+++ b/')}
    archive = subprocess.check_output(['git', '-C', str(RUNTIME), 'archive', 'HEAD', '--', *sorted(names)])
    with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
        files = {item.name: contents.extractfile(item).read() for item in contents if item.isfile()}
    assert files.keys() == names, 'Upstream archive must include every patched source file'
    return files


@pytest.fixture
def checkout(tmp_path, upstream_files):
    subprocess.run(['git', 'init', '-q', str(tmp_path)], check=True)
    for name, content in upstream_files.items():
        destination = tmp_path / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
    return tmp_path


def apply(tree, patch):
    result = installer.git(tree, 'apply', PATCHES / patch)
    assert result.returncode == 0, result.stderr


def snapshot(tree):
    return {p.relative_to(tree): p.read_bytes() for folder in ('src', 'titles', 'tools')
            for p in (tree / folder).rglob('*') if p.is_file()}


def test_fresh_checkout_and_repeated_build(checkout):
    assert installer.apply_patches(checkout, PATCHES)
    # Multiple features may wrap the same guest routine. They must compose
    # under one C definition; generation alone does not reject duplicates.
    manual = (checkout / 'titles/futureperfect/src/recomp_manual.c').read_text()
    wrappers = Counter(re.findall(r'\bvoid\s+(sub_[0-9A-Fa-f]+)\s*\([^)]*\)\s*\{', manual))
    assert all(count == 1 for count in wrappers.values()), wrappers
    # Both common RHI and legacy GL link the same live mode policy.
    assert (checkout / 'src/d3d/CMakeLists.txt').read_text().count(
        '"${TSFP_NATIVE_DIR}/../texture_mode.c"') == 2
    complete = snapshot(checkout)
    assert installer.apply_patches(checkout, PATCHES) == []
    assert snapshot(checkout) == complete


def test_upgrade_native_checkpoint_preserves_fixed_default(checkout, tmp_path):
    """The native checkpoint predates main's removal of interpolated replay."""
    # Later coefficient patches require the surface-constant baseline, so the
    # historical checkpoint must exclude both until the complete-stack upgrade.
    prior=tmp_path/'native-prior-patches'
    prior.mkdir()
    for patch in PATCHES.glob('*.patch'):
        if not patch.name.endswith(('-fixed-60-fps.patch','-native-interp-retirement.patch','-native-frame-rate-settings.patch','-native-live-frame-rate.patch', '-display-rounded-highlight.patch', '-native-surface-trace.patch', '-native-surface-fractional.patch', '-native-surface-precision.patch', '-native-surface-copy.patch', '-native-surface-constants.patch', '-native-surface-coefficients.patch', '-native-surface-preservation.patch', '-native-vehicle-physics.patch', '-native-surface-live.patch', '-native-surface-float32.patch', '-native-flip-handoff.patch', '-native-surface-capture.patch', '-native-surface-render.patch', '-native-demo-timing.patch', '-native-trace-quiet.patch')):
            shutil.copyfile(patch,prior/patch.name)
    assert installer.apply_patches(checkout,prior)
    assert 'pc_native_rate()' in (checkout/'src/hle/hle_d3d8_interp.c').read_text()
    assert installer.apply_patches(checkout,PATCHES)
    interp=(checkout/'src/hle/hle_d3d8_interp.c').read_text()
    assert 'gate_idle' not in interp and 'pc_native_rate' not in interp
    assert 'return 0;' in interp
    assert 'pc_native_timing_guest' in (checkout/'titles/futureperfect/src/recomp_manual.c').read_text()
    assert 'if (!pc_native_rate()) xbox_Nv2aFlipGateArm();' in (checkout/'src/hle/hle_d3d8.c').read_text()
    assert 'if (!strcmp(key, "frame_cap")) return "60";' in (checkout/'src/config/recomp_config.c').read_text()
    complete=snapshot(checkout)
    assert installer.apply_patches(checkout,PATCHES)==[]
    assert snapshot(checkout)==complete


@pytest.mark.parametrize('history', ['pc-presentation', 'native-renderer', 'native-launcher'])
@pytest.mark.parametrize('autocrlf', ['false', 'true'])
def test_integrate_independent_presentation_stacks(checkout, tmp_path, history,
                                                  autocrlf, monkeypatch):
    """Recorded PC, native-renderer and native-launcher stacks migrate safely."""
    # Exercise both hosts' Git conversion modes, including disposable staging
    # repositories created by the installer. Preserve any other test config.
    count = int(os.environ.get('GIT_CONFIG_COUNT', '0'))
    monkeypatch.setenv(f'GIT_CONFIG_KEY_{count}', 'core.autocrlf')
    monkeypatch.setenv(f'GIT_CONFIG_VALUE_{count}', autocrlf)
    monkeypatch.setenv('GIT_CONFIG_COUNT', str(count + 1))
    histories = json.loads((ROOT / 'tests/fixtures/renderer-patch-stacks.json').read_text())
    launcher_history = json.loads((ROOT / 'tests/fixtures/launcher-patch-stacks.json').read_text())
    histories['native-launcher'] = launcher_history['native-launcher']
    prior = tmp_path / 'prior-patches'
    prior.mkdir()
    for name in histories[history]['patches']:
        source = PATCHES / name
        if name.endswith(('-144-fps.patch', '-presentation-timing.patch',
                          '-interpolation-recovery.patch')):
            source = PATCHES / 'legacy' / name
        shutil.copyfile(source, prior / name)
    installer.apply_patches(checkout, prior)
    installer.apply_patches(checkout, PATCHES)
    integrated = snapshot(checkout)
    manual = checkout / 'titles/futureperfect/src/recomp_manual.c'
    newline = b'\r\n' if manual.read_bytes().endswith(b'\r\n') else b'\n'
    local_edit = newline + b'/* integration-local edit */' + newline
    manual.write_bytes(manual.read_bytes() + local_edit)
    installer.apply_patches(checkout, prior)
    installer.apply_patches(checkout, PATCHES)
    key = Path('titles/futureperfect/src/recomp_manual.c')
    integrated[key] += local_edit
    # Hash comparison is byte-exact without dumping megabytes of source into
    # CI logs if a transition changes one file unexpectedly.
    digest = lambda files: {name: hashlib.sha256(data).hexdigest()
                            for name, data in files.items()}
    assert digest(snapshot(checkout)) == digest(integrated)
    assert installer.apply_patches(checkout, PATCHES) == []


def test_upgrade_previous_controls_with_newer_input_features(checkout):
    # Historical state: controls/debug/prompts applied before display/textures
    # and the later Mac bring-up patch. Upgrade it with the complete stack.
    for patch in sorted(PATCHES.glob('*.patch')):
        if patch.name.endswith(('-texture-bind-scan.patch', '-model-transform-entry.patch', '-stable-gpr-refs.patch', '-zero-base-indices.patch', '-temporal-uplink-map.patch')):
            continue  # Requires the later cross-platform renderer context.
        if patch.name.endswith('-text-panel-scope.patch'):
            continue  # Depends on the later shared panel/display wrapper.
        if patch.name == 'texture-upload-streaming.patch':
            continue  # The historical state has neither texture-pack nor toggle.
        if patch.name.endswith(('-auto-display.patch', '-crane-vignette.patch')):
            continue  # Depends on display/viewport hooks installed by the complete migration.
        if patch.name.endswith(('-texture-mode-shared.patch', '-texture-mode-ui.patch')):
            continue  # Depends on the common native renderer added by migration.
        if patch.name.endswith(('-discord-presence.patch', '-shared-renderer-host.patch', '-shared-vulkan-mac.patch', '-shared-native-renderer.patch', '-shared-interpolation-depth.patch', '-shared-present-metrics.patch', '-native-launcher-services.patch', '-shared-screen-copy-clipping.patch', '-vulkan-portability-swizzle.patch', '-shared-interpolation-capture.patch')):
            continue  # These changes depend on later native/display or feedback blocks.
        if patch.name in ('zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-white-flash-overlay.patch',
                         'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-title-screen-matte.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-character-mouse.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-team-mouse.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-custom-choice-arrows.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-choice-arrows.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-setting-sliders.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-audio-menu-mouse.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-native-build-identity.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-native-menu-mode.patch',
                          'zzzzzzzzzz-decal-projection.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzz-arcade-rank-anchor.patch',
                          'zzzzzzzzzzz-model-projection.patch',
                          'zzzzzzzzzzzzzzzzz-channel-clears.patch',
                          'zzzzzzzzzzzzzzzzzz-ui-layout.patch',
                          'zzzzzzzzzzzzzzzzzz-ui-narrow.patch',
                          'zzzzzzzzzzzzzzzzzzz-tutorial-layout.patch',
                          'zzzzzzzzzzzzzzzzzzzz-fullscreen-effects.patch',
                          'zzzzzzzzzzzzzzzzzzzzz-story-caption.patch',
                          'zzzzzzzzzzzzzzzzzzzzzz-objectives-ui-integration.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzz-spirit-rendering.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-mouse.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-ghost-target-feedback.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-mouse-source.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-mouse-footer.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-mouse-themed-footer.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-quit-desktop.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-footer-background.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-menu-backdrop.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-main-menu-font.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-objectives-scope-coverage.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-sniper-scope-coverage.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-weapon-aim-overlays.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-special-weapon-overlays.patch',
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-transition-overlay.patch'):
            continue  # Requires the display wrapper, absent in this historical state.
        if patch.name.endswith(('-keyboard-settings-menu.patch', '-keyboard-weapon-slots.patch', '-controller-profile-routing.patch', '-keyboard-reload-buffer.patch', '-radial-stick-deadzone.patch')):
            continue  # Needs shared-input locks/display wrappers absent in the old snapshot.
        if patch.name.endswith(('-144-fps.patch', '-presentation-timing.patch', '-interpolation-recovery.patch', '-shared-presentation-integration.patch', '-fixed-60-fps.patch', '-late-frame-pacing.patch', '-native-frame-timing.patch', '-native-interp-retirement.patch', '-native-numeric-policy.patch', '-native-animation-scope.patch', '-native-input-ticks.patch', '-native-script-events.patch', '-native-frame-decay.patch', '-native-camera-timing.patch', '-native-weapon-event-scope.patch', '-native-npc-weapon-event-scope.patch', '-native-npc-animation-shots.patch', '-native-npc-aim-timing.patch', '-native-kind35-projectile-timing.patch', '-native-object-pause.patch', '-native-object-rocking.patch', '-native-light-preview.patch', '-native-visual-time.patch', '-native-movie-pacing.patch', '-native-frame-rate-settings.patch','-native-live-frame-rate.patch', '-display-rounded-highlight.patch', '-native-surface-trace.patch', '-native-surface-fractional.patch', '-native-surface-precision.patch', '-native-surface-copy.patch', '-native-surface-constants.patch', '-native-surface-coefficients.patch', '-native-surface-preservation.patch', '-native-vehicle-physics.patch', '-native-surface-live.patch', '-native-surface-float32.patch', '-native-flip-handoff.patch', '-native-surface-capture.patch', '-native-surface-render.patch', '-native-demo-timing.patch', '-native-trace-quiet.patch')):
            continue  # Depends on native/shared renderer context added by the full migration below.
        if patch.name not in ('pc-display.patch', 'texture-pack.patch', 'texture-toggle.patch', 'zz-mac-native-boot.patch', 'zzz-mac-renderer.patch', 'zzzz-mac-input.patch', 'zzzzz-mac-high-refresh.patch', 'zzzzzz-interpolation-layouts.patch', 'zzzzzzz-framebuffer-sampling-passes.patch', 'zzzzzzz-shared-input-model.patch', 'zzzzzzzz-mac-feature-adapters.patch', 'zzzzzzzzz-shared-screen-space.patch', 'zzzzzzzzzz-native-settings-init.patch', 'zzzzzzzzzzz-shared-linear-formats.patch', 'zzzzzzzzzzzz-native-prompts.patch', 'zzzzzzzzzzzzz-native-camera.patch', 'zzzzzzzzzzzzzz-native-debug.patch', 'zzzzzzzzzzzzzzz-cutscene-letterbox.patch', 'zzzzzzzzzzzzzzzzz-terminal-mouse-select.patch', 'zzzzzzzzzzzzzzzzz-story-objectives-health.patch'):
            apply(checkout, patch.name)
    manual = checkout / 'titles/futureperfect/src/recomp_manual.c'
    manual.write_bytes(manual.read_bytes() + b'\r\n/* unrelated local edit */\r\n')
    assert installer.apply_patches(checkout, PATCHES)
    assert b'pc_display_aim_panel(sub_00137010_gen,0x00137010u);' in manual.read_bytes()
    bindings=(checkout / 'src/input/input_bindings.c').read_text()
    assert 'pc_settings_capture_pump()' in bindings
    assert 'tmp.bind[i].count > 2' in bindings
    assert 'pc_weapon_keys_sample' in bindings
    assert 'pc_controls_reload_sample(reload_roles' in bindings
    assert 'pc_input_radial_axis_magnitude' in bindings
    assert 'use_pad_defaults = have_pad && (auto_pad >= 0 || c->pad_fallback)' in bindings
    assert b'pc_display_weapon_slots();' in manual.read_bytes()
    device = (checkout / 'src/d3d/d3d8_device.c').read_bytes()
    assert b'tsfp_texture_frame_complete();' in device
    assert b'tsfp_texture_binding_revision();' in device
    assert b'pc_ui_vignette_clip(g_2d_tag,g_2d_placement,clip);' in device
    assert b'pc_display_camera' in manual.read_bytes()
    assert b'pc_prompt_guest' in manual.read_bytes()
    assert b'PcDisplayMode mode = pc_display_effective();' in manual.read_bytes()
    for name in ('d3d8_device.c', 'd3d8_display.c'):
        renderer = (checkout / 'src/d3d' / name).read_bytes()
        assert b'pc_display_effective().width' in renderer
        assert b'pc_display_current().width' not in renderer
    assert manual.read_bytes().endswith(b'/* unrelated local edit */\r\n')
    assert installer.apply_patches(checkout, PATCHES) == []


def test_already_integrated_controls_and_display(checkout):
    # Same result as the previously combined pc-controls.patch from main.
    apply(checkout, 'pc-controls.patch')
    apply(checkout, 'pc-display.patch')
    assert installer.apply_patches(checkout, PATCHES)
    assert installer.apply_patches(checkout, PATCHES) == []


def test_upgrade_existing_mac_stack_with_networking(checkout, tmp_path):
    old_patches = tmp_path / 'mac-only-patches'
    old_patches.mkdir()
    for patch in PATCHES.glob('*.patch'):
        if patch.name.endswith(('-native-frame-timing.patch', '-native-interp-retirement.patch', '-native-numeric-policy.patch',
                                '-native-animation-scope.patch', '-native-input-ticks.patch', '-native-script-events.patch', '-native-frame-decay.patch', '-native-camera-timing.patch', '-native-weapon-event-scope.patch', '-native-npc-weapon-event-scope.patch', '-native-npc-animation-shots.patch', '-native-npc-aim-timing.patch', '-native-kind35-projectile-timing.patch', '-native-object-pause.patch', '-native-object-rocking.patch', '-native-light-preview.patch', '-native-visual-time.patch', '-native-movie-pacing.patch', '-native-frame-rate-settings.patch','-native-live-frame-rate.patch', '-display-rounded-highlight.patch', '-native-surface-trace.patch', '-native-surface-fractional.patch', '-native-surface-precision.patch', '-native-surface-copy.patch', '-native-surface-constants.patch', '-native-surface-coefficients.patch', '-native-surface-preservation.patch', '-native-vehicle-physics.patch', '-native-surface-live.patch', '-native-surface-float32.patch', '-native-flip-handoff.patch', '-native-surface-capture.patch', '-native-surface-render.patch', '-native-demo-timing.patch', '-native-trace-quiet.patch', '-model-transform-entry.patch', '-stable-gpr-refs.patch', '-zero-base-indices.patch')):
            continue  # This historical Mac stack predates native timing and network wrappers.
        if patch.name not in ('zz-pc-network.patch', 'zzz-pc-online.patch',
                              'zzzzzzzzzzzzzzz-pc-network-movement.patch',
                                  'zzzzzzzzzzzzzzzz-pc-network-state-refresh.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzz-pc-network-bots-test.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzzz-pc-network-bot-combat.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzzzz-pc-network-bot-scoreboard.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzzzzz-pc-network-bot-shots.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzzzzzz-pc-network-bot-bounds.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzzzzzzz-pc-network-bot-authority.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzzzzzzzz-pc-network-bot-presentation.patch',
                                  'zzzzzzzzzzzzzzzzzzzzzzzzzzzzz-pc-network-bot-capacity.patch'):
            shutil.copyfile(patch, old_patches / patch.name)
    assert installer.apply_patches(checkout, old_patches)
    assert installer.apply_patches(checkout, PATCHES)
    cmake = (checkout / 'titles/futureperfect/CMakeLists.txt').read_text()
    assert 'TSFP_NATIVE_DIR' in cmake and 'pc_network.c' in cmake
    assert installer.apply_patches(checkout, PATCHES) == []


def texture_branch_patches(tmp_path):
    patches = tmp_path / 'texture-patches'
    patches.mkdir()
    for patch in PATCHES.glob('*.patch'):
        shutil.copyfile(patch, patches / patch.name)
    shutil.copyfile(PATCHES / 'legacy/texture-toggle.patch', patches / 'texture-toggle.patch')
    return patches


def main_branch_patches(tmp_path):
    """Model historical main before the toggle and its streaming dependency."""
    patches = tmp_path / 'main-patches'
    patches.mkdir()
    for patch in PATCHES.glob('*.patch'):
        if patch.name not in ('texture-toggle.patch', 'texture-upload-streaming.patch'):
            shutil.copyfile(patch, patches / patch.name)
    shutil.copytree(PATCHES / 'legacy', patches / 'legacy')
    return patches


@pytest.mark.parametrize('crlf', [False, True])
def test_switch_from_texture_branch_to_main_and_back(checkout, tmp_path, crlf):
    main_patches = main_branch_patches(tmp_path)
    texture_patches = texture_branch_patches(tmp_path)
    installer.apply_patches(checkout, texture_patches)
    manual = checkout / 'titles/futureperfect/src/recomp_manual.c'
    manual.write_bytes(manual.read_bytes() + b'\n/* unrelated local edit */\n')
    if crlf:
        for name, content in snapshot(checkout).items():
            (checkout / name).write_bytes(content.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
    assert installer.apply_patches(checkout, main_patches)
    cmake = (checkout / 'src/input/CMakeLists.txt').read_text()
    assert 'texture_mode.c' not in cmake
    assert 'pc_controls.c' in cmake
    assert 'pc_network.c' in (checkout / 'titles/futureperfect/CMakeLists.txt').read_text()
    assert 'tsfp_texture_sync_bindings' not in (checkout / 'src/d3d/d3d8_device.c').read_text()
    assert 'unrelated local edit' in manual.read_text()
    complete = snapshot(checkout)
    assert installer.apply_patches(checkout, main_patches) == []
    assert snapshot(checkout) == complete
    # The same legacy patch can become active again on the texture branch.
    shutil.copytree(PATCHES / 'legacy', texture_patches / 'legacy')
    assert installer.apply_patches(checkout, texture_patches)
    assert 'texture_mode.c' in (checkout / 'src/input/CMakeLists.txt').read_text()
    assert installer.apply_patches(checkout, texture_patches) == []
    assert installer.apply_patches(checkout, main_patches)
    assert snapshot(checkout) == complete


def test_modified_legacy_patch_rejects_switch_without_writes(checkout, tmp_path):
    main_patches = main_branch_patches(tmp_path)
    installer.apply_patches(checkout, texture_branch_patches(tmp_path))
    source = checkout / 'src/d3d/d3d8_device.c'
    source.write_bytes(source.read_bytes().replace(b'tsfp_texture_is_4k()',
                                                 b'custom_texture_is_4k()'))
    before = snapshot(checkout)
    with pytest.raises(RuntimeError, match='conflicts with the runtime checkout'):
        installer.apply_patches(checkout, main_patches)
    assert snapshot(checkout) == before


def test_native_boot_source_conflict_preserves_all_files(checkout):
    installer.apply_patches(checkout, PATCHES)
    cmake = checkout / 'titles/futureperfect/CMakeLists.txt'
    content = cmake.read_bytes()
    assert b'list(APPEND GAME_SOURCES src/main.c)' in content
    cmake.write_bytes(content.replace(b'list(APPEND GAME_SOURCES src/main.c)',
                                     b'list(APPEND GAME_SOURCES custom_main.c)'))
    before = snapshot(checkout)
    with pytest.raises(RuntimeError, match='conflicts with the runtime checkout'):
        installer.apply_patches(checkout, PATCHES)
    assert snapshot(checkout) == before


def test_conflict_preserves_every_runtime_file(checkout):
    apply(checkout, 'pc-controls.patch')
    target = checkout / 'src/input/input_bindings.c'
    target.write_bytes(target.read_bytes().replace(
        b'pc_input_normalize_movement', b'custom_normalize_movement'))
    before = snapshot(checkout)
    with pytest.raises(RuntimeError, match='pc-controls.patch conflicts'):
        installer.apply_patches(checkout, PATCHES)
    assert snapshot(checkout) == before


def test_windows_line_endings(checkout):
    apply(checkout, 'pc-controls.patch')
    for name, content in snapshot(checkout).items():
        (checkout / name).write_bytes(content.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
    assert installer.apply_patches(checkout, PATCHES)
    assert installer.apply_patches(checkout, PATCHES) == []


@pytest.mark.parametrize('already_applied', [False, True])
def test_watermark_beside_native_overlay_guard(tmp_path, already_applied):
    tree, patches = tmp_path / 'runtime', tmp_path / 'patches'
    target = tree / 'src/hle/hle_d3d8.c'
    target.parent.mkdir(parents=True)
    patches.mkdir()
    installer.git(tree, 'init', '-q')
    original = '/* overlay */\nstatic int shown;\nstatic char line[96];\nvoid redraw(void);\n'
    target.write_text(original, encoding='utf-8')
    (patches / 'zz-mac-native-boot.patch').write_text(
        '--- a/src/hle/hle_d3d8.c\n+++ b/src/hle/hle_d3d8.c\n'
        '@@ -1,4 +1,5 @@\n /* overlay */\n+#ifdef _WIN32\n'
        ' static int shown;\n static char line[96];\n void redraw(void);\n', encoding='utf-8')
    watermark = patches / 'zzzz-pc-debug-latency-watermark.patch'
    watermark.write_text(
        '--- a/src/hle/hle_d3d8.c\n+++ b/src/hle/hle_d3d8.c\n'
        '@@ -1,4 +1,4 @@\n /* overlay */\n static int shown;\n'
        '-static char line[96];\n+static char line[192];\n void redraw(void);\n', encoding='utf-8')
    if already_applied:
        assert installer.git(tree, 'apply', watermark).returncode == 0
    assert installer.apply_patches(tree, patches)
    complete = target.read_bytes()
    assert b'#ifdef _WIN32' in complete and b'line[192]' in complete
    assert installer.apply_patches(tree, patches) == []
    # Reduced context must still reject changes to the actual patched code.
    target.write_bytes(complete.replace(b'line[192]', b'line[256]'))
    before = target.read_bytes()
    with pytest.raises(RuntimeError, match='conflicts with the runtime checkout'):
        installer.apply_patches(tree, patches)
    assert target.read_bytes() == before


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows PowerShell stderr behavior')
@pytest.mark.parametrize('exit_code', [0, 7])
def test_checked_command_uses_exit_status_with_redirected_stderr(exit_code, tmp_path):
    def quote(value):
        return "'" + str(value).replace("'", "''") + "'"

    native = tmp_path / 'native warning.py'
    native.write_text('import sys\nsys.stderr.write("warning\\n")\nsys.exit(int(sys.argv[1]))\n',
                      encoding='utf-8')
    script = f"""
$ErrorActionPreference = 'Stop'
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    {quote(ROOT / 'tsfp.ps1')}, [ref]$null, [ref]$null)
$function = $ast.Find({{ param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'Invoke-Checked'
}}, $true)
. ([scriptblock]::Create($function.Extent.Text))
try {{
    $output = Invoke-Checked {quote(sys.executable)} @({quote(native)}, '{exit_code}') *>&1
    if ({exit_code} -ne 0) {{ throw 'Nonzero exit was accepted' }}
    if ($ErrorActionPreference -ne 'Stop') {{ throw 'Preference was not restored' }}
    if (($output | Out-String) -notmatch 'warning') {{ throw 'Warning was lost' }}
}} catch {{
    if ({exit_code} -eq 0 -or $_.Exception.Message -notmatch 'failed \\(exit {exit_code}\\)') {{
        throw
    }}
    if ($ErrorActionPreference -ne 'Stop') {{ throw 'Preference was not restored' }}
}}
exit 0
"""
    result = subprocess.run(['powershell.exe', '-NoProfile', '-Command', script],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
