"""Validate the complete patch stack before updating the runtime checkout.

Unwind recognized patches in reverse order in a small temporary tree, then
reapply the stack in order. Later patches may extend earlier patches' context;
checking each patch independently incorrectly reports those as conflicts.
Record the exact applied patch stack in runtime Git metadata so later builds
can migrate removed or edited patches after a branch switch. Legacy patches
are unwind-only migration inputs for runtimes built before tracking existed.
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

from pathlib import Path
import argparse
import difflib
import hashlib
import json
import os
import shutil
import subprocess
import tempfile


def git(tree, *args):
    return subprocess.run(
        # Upstream stores some sources as CRLF. Match context whitespace on
        # every host while retaining it in the output; non-whitespace edits
        # still conflict. Do not depend on a developer's global Git settings.
        ['git', '-C', str(tree), '-c', 'apply.ignoreWhitespace=change', *map(str, args)],
        capture_output=True, encoding='utf-8', errors='replace',
    )


def state_path(runtime):
    """Per-runtime/worktree metadata survives source branch switches."""
    result = git(runtime, 'rev-parse', '--absolute-git-dir')
    if result.returncode:
        raise RuntimeError(f'Cannot locate runtime Git metadata: {result.stderr}')
    return Path(result.stdout.strip()) / 'tsfp-patches.json'


def active_patch_paths(patch_dir):
    """Discover responsibility folders; legacy is exclusively an unwind input."""
    patch_dir = Path(patch_dir)
    return sorted((p for p in patch_dir.rglob('*.patch')
                   if 'legacy' not in p.relative_to(patch_dir).parts),
                  key=lambda p: p.name)


def patch_records(paths):
    paths = sorted(paths, key=lambda patch: patch.name)
    # Basenames are stable ledger identities and dependency keys. Folder
    # traversal order must never decide which of two same-named patches wins.
    if len({patch.name for patch in paths}) != len(paths):
        raise ValueError('Duplicate active patch basename; names must be globally unique')
    # These existing network patches target the original title CMake context.
    # Apply them before native boot inserts platform guards at that location;
    # retain the established order of all other patches and unwind in reverse.
    by_name = {patch.name: patch for patch in paths}
    native_boot = by_name.get('0400-mac-native-boot.patch')
    if native_boot:
        for name in ('0470-pc-network.patch', '0640-pc-online.patch'):
            if name in by_name:
                network = by_name[name]
                paths.remove(network)
                paths.insert(paths.index(native_boot), network)
    # Discord appends its Windows adapter beside the original native input
    # block. Apply it before build identity extends that block's final lines.
    # Both patches keep their original bytes so tracked older stacks unwind.
    identity = next((p for p in paths if p.name.endswith('-native-build-identity.patch')), None)
    discord = next((p for p in paths if p.name.endswith('-discord-presence.patch')), None)
    if identity and discord:
        paths.remove(identity)
        paths.insert(paths.index(discord) + 1, identity)
    # The independently developed PC presentation series extends replay and
    # present handling. Apply its integrated form after the shared renderer
    # stack; recorded prior bytes/order still unwind older Windows/Mac builds.
    for suffix in ('-144-fps.patch', '-presentation-timing.patch',
                   '-interpolation-recovery.patch', '-controls-accept-save.patch',
                   '-shared-presentation-integration.patch', '-native-frame-timing.patch',
                   '-native-interp-retirement.patch', '-fixed-60-fps.patch', '-late-frame-pacing.patch',
                   '-native-numeric-policy.patch', '-native-animation-scope.patch',
                   '-native-input-ticks.patch', '-native-script-events.patch', '-native-frame-decay.patch',
                   '-native-camera-timing.patch', '-native-weapon-event-scope.patch',
                   '-native-npc-weapon-event-scope.patch', '-native-npc-animation-shots.patch', '-native-npc-aim-timing.patch', '-native-kind35-projectile-timing.patch', '-native-object-pause.patch', '-native-object-rocking.patch', '-native-rocket-projectile-timing.patch', '-native-projectile-families.patch', '-native-light-preview.patch', '-native-visual-time.patch', '-native-movie-pacing.patch', '-native-frame-rate-settings.patch', '-native-live-frame-rate.patch', '-display-rounded-highlight.patch', '-native-surface-trace.patch', '-native-surface-fractional.patch', '-native-surface-precision.patch', '-native-surface-copy.patch', '-native-surface-constants.patch', '-native-surface-coefficients.patch', '-native-surface-preservation.patch', '-native-vehicle-physics.patch', '-native-surface-live.patch', '-native-surface-float32.patch', '-native-flip-handoff.patch', '-native-surface-capture.patch', '-native-surface-render.patch', '-native-trace-quiet.patch', '-native-demo-timing.patch', '-texture-bind-scan.patch', '-keyboard-reload-buffer.patch', '-model-transform-entry.patch', '-zero-base-indices.patch', '-stable-gpr-refs.patch', '-native-wind-history.patch', '-native-minigun-intent.patch', '-native-player-trigger-event.patch', '-kill-feed-firing-scope.patch'):
        patch = next((p for p in paths if p.name.endswith(suffix)), None)
        if patch:
            paths.remove(patch)
            paths.append(patch)
    # The Uplink copy builds on the final screen-copy and interpolation adapters.
    uplink = next((p for p in paths if p.name.endswith('-temporal-uplink-map.patch')), None)
    if uplink:
        paths.remove(uplink)
        paths.append(uplink)
    # Stick remapping extends the binding sampler after the late reload patch.
    radial = next((p for p in paths if p.name.endswith('-radial-stick-deadzone.patch')), None)
    if radial:
        paths.remove(radial)
        paths.append(radial)
    # Player device isolation extends both the late reload sampler and pad routing.
    players = by_name.get('2350-local-player-devices.patch')
    if players:
        paths.remove(players)
        paths.append(players)
    reconnect = by_name.get('2360-controller-reconnect-identity.patch')
    if reconnect:
        paths.remove(reconnect)
        paths.append(reconnect)

    # Own instruction bodies replace the final title-instrumented handlers.
    # Replay all historical instrumentation first so older stacks still unwind.
    for name in ('0180-independent-translation-moves.patch', '0170-independent-translation-integer.patch',
                 '0150-independent-translation-bits.patch', '0200-independent-translation-product.patch', '0140-independent-translation-bitops.patch', 'independent-translation-exchange.patch', 'independent-translation-compare.patch', 'independent-translation-strings.patch', 'independent-translation-conditions.patch', 'independent-translation-sse.patch', 'independent-translation-mmx.patch', 'independent-translation-x87.patch', 'independent-translation-control.patch', 'independent-translation-lifting.patch', 'independent-translation-context.patch', 'independent-translation-batch.patch', 'independent-translation-discovery.patch', 'independent-translation-function.patch', 'independent-translation-cli.patch', 'independent-guest-abi.patch', '0195-independent-guest-dispatch.patch', '0196-independent-call-frames.patch', '0197-independent-vector-support.patch', '0198-independent-scalar-support.patch', '0199-independent-runtime-header.patch', '0201-runtime-header-consumer-fixture.patch', '0202-independent-branch-feedback.patch', '0203-independent-nonlocal-jumps.patch', '0204-independent-guest-state.patch', '0205-independent-image-loader.patch', '0206-independent-guest-allocations.patch', '0207-independent-thread-memory.patch', '0208-independent-virtual-memory.patch', '0209-independent-native-memory.patch', '0211-independent-guest-memory-services.patch', '0212-independent-native-vm-host.patch', '0213-independent-guest-host-maps.patch', '0214-retire-compatibility-memory.patch', '0215-independent-kernel-objects.patch', '0216-independent-kernel-boot.patch', '0217-guest-next-packet-abi.patch', '0218-independent-kernel-requests.patch', '0219-independent-kernel-devices.patch', '0220-independent-kernel-sharing.patch', '0221-independent-request-lifetime.patch', '0222-original-driver-call-composer.patch', '0223-original-transfer-completion.patch', '0224-original-request-queue.patch', '0225-original-startio-callback.patch', '0226-original-device-queue-mirrors.patch', '0227-original-device-queue-lifetime.patch', '0229-original-guest-events.patch', '0231-original-completion-targets.patch', '0232-original-symbolic-links.patch', '0233-original-path-routing.patch', '0234-original-host-storage.patch', '0235-original-kernel-pool.patch', '0236-original-kernel-time.patch', '0237-original-counted-strings.patch', '0238-original-kernel-rtl.patch', '0239-original-guest-printf.patch', '0240-original-guest-numbers.patch', '0241-original-extended-product.patch'):
        replacement = by_name.get(name)
        if replacement:
            paths.remove(replacement)
            paths.append(replacement)
    # Host exit borders the pool wrapper replaced by the independent kernel.
    close = by_name.get('2303-audio-window-exit.patch')
    if close:
        paths.remove(close)
        paths.append(close)
    # Allocation evidence depends on the final independent allocator and boot.
    observer = by_name.get('2150-allocation-history.patch')
    if observer:
        paths.remove(observer)
        paths.append(observer)
    # Source-layout routing is applied after every historical source extension.
    layout = by_name.get('2090-repository-source-layout.patch')
    if layout:
        paths.remove(layout)
        paths.append(layout)
    # New adapters use the grouped paths and follow the historical layout migration.
    online = by_name.get('2140-native-online.patch')
    if online:
        paths.remove(online)
        paths.append(online)
    # AO capture version 10 extends the version-9 surface-constant chunks.
    ao_capture = by_name.get('2502-ambient-occlusion-capture.patch')
    if ao_capture:
        paths.remove(ao_capture)
        paths.append(ao_capture)
    # Scene ownership extends the final presentation/interpolation lifecycle.
    scene_targets = by_name.get('2504-scene-target-lifecycle.patch')
    if scene_targets:
        paths.remove(scene_targets)
        paths.append(scene_targets)
    # Offscreen binding extends the final scene/interpolation target lifecycle.
    msaa_bindings = by_name.get('2507-msaa-offscreen-bindings.patch')
    if msaa_bindings:
        paths.remove(msaa_bindings)
        paths.append(msaa_bindings)
    primary_msaa = by_name.get('2508-msaa-primary-scene.patch')
    if primary_msaa:
        paths.remove(primary_msaa)
        paths.append(primary_msaa)
    # Alias synchronization uses the final surface draw-view helper (2507).
    aliases = by_name.get('2510-msaa-surface-aliases.patch')
    if aliases:
        paths.remove(aliases)
        paths.append(aliases)
    copy_sync = by_name.get('2511-msaa-screen-copy-sync.patch')
    if copy_sync:
        paths.remove(copy_sync)
        paths.append(copy_sync)

    # Kernel arithmetic now shares the original fault boundary with utility
    # consumers. Move its source ownership after the historical path migration.
    division = by_name.get('0242-original-extended-division.patch')
    if division:
        paths.remove(division)
        paths.append(division)
    memory_ops = by_name.get('0243-original-memory-operations.patch')
    if memory_ops:
        paths.remove(memory_ops)
        paths.append(memory_ops)
    codec = by_name.get('0244-original-counted-codec.patch')
    if codec:
        paths.remove(codec)
        paths.append(codec)
    transform = by_name.get('0245-original-string-transform.patch')
    if transform:
        paths.remove(transform)
        paths.append(transform)
    raw_codec = by_name.get('0246-original-raw-codec.patch')
    if raw_codec:
        paths.remove(raw_codec)
        paths.append(raw_codec)
    stack_diagnostics = by_name.get('0247-original-stack-diagnostics.patch')
    if stack_diagnostics:
        paths.remove(stack_diagnostics)
        paths.append(stack_diagnostics)
    status_flags = by_name.get('0248-original-status-flags.patch')
    if status_flags:
        paths.remove(status_flags)
        paths.append(status_flags)
    status_header = by_name.get('0249-status-header-template.patch')
    if status_header:
        paths.remove(status_header)
        paths.append(status_header)
    context_records = by_name.get('0253-original-context-records.patch')
    if context_records:
        paths.remove(context_records)
        paths.append(context_records)
    exception_dispatch = by_name.get('0258-original-exception-dispatch.patch')
    if exception_dispatch:
        paths.remove(exception_dispatch)
        paths.append(exception_dispatch)
    execution_ownership = by_name.get('0259-original-execution-ownership.patch')
    if execution_ownership:
        paths.remove(execution_ownership)
        paths.append(execution_ownership)
    execution_host = by_name.get('0260-original-execution-host-policy.patch')
    if execution_host:
        paths.remove(execution_host)
        paths.append(execution_host)
    # SMAA extends the final config writer after native timing migrations.
    smaa = by_name.get('2470-smaa-antialiasing.patch')
    if smaa:
        paths.remove(smaa)
        paths.append(smaa)
    # Native HUD switches the final scene/depth ownership introduced by MSAA.
    native_hud = by_name.get('2516-native-resolution-hud.patch')
    if native_hud:
        paths.remove(native_hud)
        paths.append(native_hud)
    hud_stencil = by_name.get('2517-native-hud-stencil-plane.patch')
    if hud_stencil:
        paths.remove(hud_stencil)
        paths.append(hud_stencil)

    # 2401 already contains the exact portable declaration guard added by 2512.
    # Retain 2512 for stacks without Linux; applying that same hunk twice fails.
    if '2401-linux-vulkan-foundation.patch' in by_name:
        duplicate_guard = by_name.get('2512-portable-renderer-declarations.patch')
        if duplicate_guard:
            paths.remove(duplicate_guard)
    # Linux extends the final grouped SDL/RHI adapters. Keep every historical
    # Windows/Mac patch intact and replay this after their source-layout work.
    for name in ('2401-linux-vulkan-foundation.patch', '2402-linux-native-game.patch', '2403-linux-native-crypto.patch', '2404-linux-hle-renderer.patch', '2405-linux-runtime-link.patch', '2406-native-vblank-alias.patch', '2407-vulkan12-compatibility.patch', '2408-vulkan-descriptor-capacity.patch', '2409-linux-late-frame-pacing.patch', '2411-vulkan-upload-efficiency.patch', '2412-native-desktop-media.patch'):
        linux = by_name.get(name)
        if linux:
            paths.remove(linux)
            paths.append(linux)
    # Developer Tools consumes the final shared Windows/SDL input layout and
    # positioned RHI overlays, including Linux's last host-adapter extensions.
    developer_tools = by_name.get('2520-developer-tools-overlay.patch')
    if developer_tools:
        paths.remove(developer_tools)
        paths.append(developer_tools)
    developer_changes=by_name.get('2530-developer-tools-controls-and-cheats.patch')
    if developer_changes:
        paths.remove(developer_changes)
        paths.append(developer_changes)
    developer_settings=by_name.get('2540-developer-tools-settings-and-shortcut.patch')
    if developer_settings:
        paths.remove(developer_settings)
        paths.append(developer_settings)
    developer_movies=by_name.get('2550-developer-tools-cutscene-speed.patch')
    if developer_movies:
        paths.remove(developer_movies)
        paths.append(developer_movies)
    for name in ('2560-developer-tools-ui-audio-and-bindings.patch',
                 '2570-developer-tools-game-audio-clock.patch',
                 '2580-developer-tools-lobby-wrench.patch',
                 '2590-developer-tools-optional-audio-speed.patch',
                 '2600-developer-tools-sidebar-renderer.patch',
                 '2610-developer-tools-sidebar-refinement.patch',
                 '2620-developer-tools-shared-font.patch',
                 '2630-developer-tools-native-player-input.patch'):
        followup=by_name.get(name)
        if followup:
            paths.remove(followup)
            paths.append(followup)

    # Settings must use the active source after grouped/native target migrations.
    config_source = by_name.get('2595-config-source-worktree.patch')
    if config_source:
        paths.remove(config_source)
        paths.append(config_source)
    # Shared-depth resize extends the final header after temporal map and
    # Linux declaration migrations. Keep those historical hunks unchanged.
    shared_depth_resize = by_name.get('2600-msaa-scaled-shared-depth.patch')
    if shared_depth_resize:
        paths.remove(shared_depth_resize)
        paths.append(shared_depth_resize)
    # Inclusive portal clips extend the final shared renderer/scaled-target code.
    portal_scissors = by_name.get('2725-inclusive-portal-scissors.patch')
    if portal_scissors:
        paths.remove(portal_scissors)
        paths.append(portal_scissors)
    # Presets extend the final local-device/reload sampler without replacing it.
    presets = by_name.get('2940-modern-control-presets.patch')
    if presets:
        paths.remove(presets)
        paths.append(presets)

    # Deadzone controls extend the completed preset/local-player sampler.
    deadzones = by_name.get('2950-controller-deadzones.patch')
    if deadzones:
        paths.remove(deadzones)
        paths.append(deadzones)

    # Prompt activity includes explicit native P1 controller assignments.
    prompts = by_name.get('2955-native-explicit-controller-prompts.patch')
    if prompts:
        paths.remove(prompts)
        paths.append(prompts)

    # Keyboard actions stay semantic for legacy as well as newly seeded files.
    keyboard = by_name.get('2960-keyboard-gameplay-independence.patch')
    if keyboard:
        paths.remove(keyboard)
        paths.append(keyboard)
    keyboard_actions = by_name.get('2965-independent-keyboard-actions.patch')
    if keyboard_actions:
        paths.remove(keyboard_actions)
        paths.append(keyboard_actions)

    pad_sharing = by_name.get('2970-active-player-pad-sharing.patch')
    if pad_sharing:
        paths.remove(pad_sharing)
        paths.append(pad_sharing)

    records = []
    for patch in paths:
        # Native boot wraps the SDK library block; networking adds a separate
        # guarded block immediately before it. Require its unchanged adjacent
        # context line, rather than the older two lines across that insertion.
        # The Windows watermark also borders native boot's overlay guard.
        # Match its adjacent context without requiring the older unguarded
        # comment/function boundary. Removed/added code must still match.
        reduced_context = native_boot and (patch.name == '0700-pc-debug-latency-watermark.patch'
                                           or patch == native_boot and '0470-pc-network.patch' in by_name)
        # These additive wrappers ended the manual file when authored. A local
        # comment appended later must not make Git require an end-of-file match
        # during reverse application; retain adjacent context and exact code.
        reduced_context = reduced_context or patch.name in (
            '2520-developer-tools-overlay.patch',
            '2530-developer-tools-controls-and-cheats.patch',
            '2550-developer-tools-cutscene-speed.patch',
            '2580-developer-tools-lobby-wrench.patch')
        content = patch.read_bytes()
        records.append(dict(name=patch.name, content=content.decode('utf-8'),
                            sha256=hashlib.sha256(content).hexdigest(),
                            options=['-C1'] if reduced_context else []))
    return records


def read_state(path):
    if not path.exists():
        return None, None
    raw = path.read_bytes()
    try:
        state = json.loads(raw)
        if state['version'] != 1 or not isinstance(state['patches'], list):
            raise ValueError('unsupported schema')
        names = set()
        for patch in state['patches']:
            name = patch['name']
            if (not isinstance(name, str) or not name.endswith('.patch') or
                    '/' in name or '\\' in name or name in names):
                raise ValueError('invalid or duplicate patch name')
            names.add(name)
            if patch['options'] not in ([], ['-C1']):
                raise ValueError('unsupported patch options')
            if hashlib.sha256(patch['content'].encode('utf-8')).hexdigest() != patch['sha256']:
                raise ValueError(f'{name} content hash mismatch')
    except (ValueError, KeyError, TypeError, AttributeError) as error:
        raise RuntimeError(f'Invalid runtime patch record {path}: {error}. No runtime files were changed.') from error
    return raw, state['patches']


def preserve_line_endings(before, after):
    """Keep existing source formatting independently of Git's host EOL setting.

    Git can restore removed CRLF lines as LF when reversing a normalized patch.
    Keep exact endings on unchanged lines; new/replaced lines inherit the replaced
    region's style, or adjacent source for a pure insertion. Content and final-
    newline presence come from the validated patch result. Never alter binary data.
    """
    if before == after or b'\0' in before or b'\0' in after:
        return after

    def split(data):
        rows = data.split(b'\n')
        result = [(row[:-1], b'\r\n') if row.endswith(b'\r') else (row, b'\n')
                  for row in rows[:-1]]
        if rows[-1]:
            result.append((rows[-1], b''))
        return result

    old, new = split(before), split(after)
    old_text = [body for body, _ in old]
    new_text = [body for body, _ in new]
    if old_text == new_text:
        return b''.join(body + (old[i][1] if ending and old[i][1] else ending)
                        for i, (body, ending) in enumerate(new))

    def style(rows, fallback):
        crlf = sum(ending == b'\r\n' for _, ending in rows)
        lf = sum(ending == b'\n' for _, ending in rows)
        return (b'\r\n' if crlf >= lf else b'\n') if crlf + lf else fallback

    default = style(old, b'\n')
    output = []
    for action, a, b, c, d in difflib.SequenceMatcher(None, old_text, new_text).get_opcodes():
        if action == 'equal':
            output.extend(body + (old[a + i][1] if ending and old[a + i][1] else ending)
                          for i, (body, ending) in enumerate(new[c:d]))
        else:
            nearby = old[max(0, a - 1):min(len(old), b + 1)]
            ending = style(old[a:b], style(nearby, default))
            output.extend(body + (ending if present else b'') for body, present in new[c:d])
    return b''.join(output)


def validation_key(record, patches, originals, git_version):
    """Exact content receipt; never trust file size/mtime or only a Git revision."""
    value = dict(schema=1, validator=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                 git=git_version, record=hashlib.sha256(record or b'').hexdigest(),
                 patches=patches,
                 files={name: hashlib.sha256(data).hexdigest() for name, data in originals.items()})
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def save_validation(path, key):
    # Optional optimization metadata cannot turn a valid build into a failure.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='tsfp-validation-',
                                         suffix='.tmp', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(key.encode('ascii'))
        os.replace(temporary, path)
    except OSError:
        pass
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def apply_patches(runtime, patch_dir, *, force=False):
    runtime, patch_dir = Path(runtime).resolve(), Path(patch_dir).resolve()
    record_path = state_path(runtime)
    record_before, previous = read_state(record_path)
    active = active_patch_paths(patch_dir)
    patches = patch_records(active)
    if previous is None:
        # Bootstrap runtimes built before automatic tracking. Legacy patches
        # remain an unwind-only migration aid; an active copy takes precedence.
        known = {patch.name: patch for patch in sorted((patch_dir / 'legacy').glob('*.patch'))}
        known.update({patch.name: patch for patch in active})
        unwind_patches = patch_records(known.values())
    else:
        # Preserve the exact old bytes, order and context options, including
        # patches removed, renamed or edited on the newly checked-out branch.
        unwind_patches = previous
    names = set()
    for patch in [*unwind_patches, *patches]:
        for line in patch['content'].splitlines():
            if line.startswith('+++ b/'):
                name = line[6:]
                target = runtime / name
                if not target.resolve().is_relative_to(runtime) or not target.is_file():
                    raise RuntimeError(f'{patch["name"]}: invalid or missing target {name}')
                names.add(name)
    originals = {name: (runtime / name).read_bytes() for name in names}
    receipt = record_path.with_name('tsfp-patches-validated.sha256')
    version = git(runtime, '--version')
    if version.returncode:
        raise RuntimeError(f'Cannot identify Git for patch validation: {version.stderr}')
    key = validation_key(record_before, patches, originals, version.stdout.strip())
    try:
        reusable = not force and previous == patches and receipt.read_text(encoding='ascii') == key
    except (OSError, UnicodeError):
        reusable = False
    if reusable:
        if (record_path.read_bytes() != record_before or
                any((runtime/name).read_bytes() != data for name, data in originals.items())):
            raise RuntimeError('Runtime changed during cached patch validation; retry.')
        return []
    receipt.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='tsfp-patches-') as directory:
        stage = Path(directory) / 'runtime'
        stage.mkdir()
        initialized = git(stage, 'init', '-q')
        if initialized.returncode:
            raise RuntimeError(initialized.stderr)
        for name, content in originals.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        for phase, stack in [('unwind', reversed(unwind_patches)), ('apply', patches)]:
            for index, patch in enumerate(stack):
                payload = Path(directory) / f'{phase}-{index}.patch'
                payload.write_bytes(patch['content'].encode('utf-8'))
                options = patch['options'] + (['--reverse'] if phase == 'unwind' else [])
                if phase == 'unwind' and previous is None:
                    # Bootstrap probes skip non-matching legacy patches. Keep
                    # their dry run so an actual apply/write failure is fatal.
                    result = git(stage, 'apply', *options, '--check', payload)
                    if result.returncode:
                        continue
                # Default git apply rejects the whole patch on a bad hunk.
                # This disposable tree is itself the validation pass; avoid a
                # duplicate --check process for tracked unwind/active patches.
                result = git(stage, 'apply', *options, payload)
                if result.returncode:
                    raise RuntimeError(
                        f'{patch["name"]} conflicts with the runtime checkout during {phase}. '
                        f'No runtime files were changed.\n{result.stderr}'
                    )
        # Preserve author-owned source formatting after the whole semantic
        # migration, before its transactional validation/publication boundary.
        for name in names:
            target = stage / name
            patched = target.read_bytes()
            preserved = preserve_line_endings(originals[name], patched)
            if preserved != patched:
                target.write_bytes(preserved)
        changed = [name for name in sorted(names)
                   if (stage / name).read_bytes() != originals[name]]
        # Do not overwrite an edit made while the stack was being validated.
        for name in names:
            if (runtime / name).read_bytes() != originals[name]:
                raise RuntimeError(f'{name} changed during patch validation; retry.')
        if (record_path.read_bytes() if record_path.exists() else None) != record_before:
            raise RuntimeError('Runtime patch record changed during validation; retry.')
        record_after = (json.dumps(dict(version=1, patches=patches), indent=2) + '\n').encode('utf-8')
        if not changed and record_after == record_before:
            # Only a completed full no-op validation earns this receipt. A
            # failed migration cannot bless a partially written runtime tree.
            save_validation(receipt, key)
            return []
        # Prepare metadata before source writes and publish it last. On a write
        # failure restore sources while keeping the previous record intact.
        temporary = None
        written = []
        try:
            with tempfile.NamedTemporaryFile(dir=record_path.parent, prefix='tsfp-patches-',
                                             suffix='.tmp', delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(record_after)
                stream.flush()
                os.fsync(stream.fileno())
            for name in changed:
                written.append(name)
                shutil.copyfile(stage / name, runtime / name)
            os.replace(temporary, record_path)
        except OSError as error:
            for name in reversed(written):
                (runtime / name).write_bytes(originals[name])
            raise RuntimeError(f'Patch update failed; restored runtime sources and kept the previous record: {error}') from error
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
        # Seal only the bytes validated in the temporary tree, never a fresh
        # read that could bless an unrelated edit made after validation.
        active_names = {line[6:] for patch in patches for line in patch['content'].splitlines()
                        if line.startswith('+++ b/')}
        validated = {name: (stage/name).read_bytes() for name in active_names}
        save_validation(receipt, validation_key(record_after, patches, validated, version.stdout.strip()))
    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('patches', type=Path)
    parser.add_argument('--revalidate', action='store_true', help='Force full unwind/apply even when validated content matches')
    args = parser.parse_args()
    try:
        changed = apply_patches(args.runtime, args.patches, force=args.revalidate)
    except RuntimeError as error:
        parser.exit(1, f'{error}\n')
    print(f'Local patches verified; {len(changed)} runtime file(s) updated.')
