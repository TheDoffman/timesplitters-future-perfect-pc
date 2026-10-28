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
                   '-native-npc-weapon-event-scope.patch', '-native-npc-animation-shots.patch', '-native-npc-aim-timing.patch', '-native-kind35-projectile-timing.patch', '-native-object-pause.patch', '-native-object-rocking.patch', '-native-light-preview.patch', '-native-visual-time.patch', '-native-movie-pacing.patch', '-native-frame-rate-settings.patch', '-native-live-frame-rate.patch', '-display-rounded-highlight.patch', '-native-surface-trace.patch', '-native-surface-fractional.patch', '-native-surface-precision.patch', '-native-surface-copy.patch', '-native-surface-constants.patch', '-native-surface-coefficients.patch', '-native-surface-preservation.patch', '-native-vehicle-physics.patch', '-native-surface-live.patch', '-native-surface-float32.patch', '-native-flip-handoff.patch', '-native-surface-capture.patch', '-native-surface-render.patch', '-native-trace-quiet.patch', '-native-demo-timing.patch', '-texture-bind-scan.patch', '-keyboard-reload-buffer.patch', '-model-transform-entry.patch', '-zero-base-indices.patch', '-stable-gpr-refs.patch', '-native-wind-history.patch'):
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
    # Own instruction bodies replace the final title-instrumented handlers.
    # Replay all historical instrumentation first so older stacks still unwind.
    for name in ('0180-independent-translation-moves.patch', '0170-independent-translation-integer.patch',
                 '0150-independent-translation-bits.patch', '0200-independent-translation-product.patch', '0140-independent-translation-bitops.patch', 'independent-translation-exchange.patch', 'independent-translation-compare.patch', 'independent-translation-strings.patch', 'independent-translation-conditions.patch', 'independent-translation-sse.patch', 'independent-translation-mmx.patch', 'independent-translation-x87.patch', 'independent-translation-control.patch', 'independent-translation-lifting.patch', 'independent-translation-context.patch', 'independent-translation-batch.patch', 'independent-translation-discovery.patch', 'independent-translation-function.patch', 'independent-translation-cli.patch', 'independent-guest-abi.patch', '0195-independent-guest-dispatch.patch', '0196-independent-call-frames.patch', '0197-independent-vector-support.patch', '0198-independent-scalar-support.patch', '0199-independent-runtime-header.patch', '0201-runtime-header-consumer-fixture.patch', '0202-independent-branch-feedback.patch', '0203-independent-nonlocal-jumps.patch', '0204-independent-guest-state.patch', '0205-independent-image-loader.patch', '0206-independent-guest-allocations.patch', '0207-independent-thread-memory.patch', '0208-independent-virtual-memory.patch', '0209-independent-native-memory.patch', '0211-independent-guest-memory-services.patch', '0212-independent-native-vm-host.patch'):
        replacement = by_name.get(name)
        if replacement:
            paths.remove(replacement)
            paths.append(replacement)
    # Source-layout routing is applied after every historical source extension.
    layout = by_name.get('2090-repository-source-layout.patch')
    if layout:
        paths.remove(layout)
        paths.append(layout)
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


def apply_patches(runtime, patch_dir):
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
    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('patches', type=Path)
    args = parser.parse_args()
    try:
        changed = apply_patches(args.runtime, args.patches)
    except RuntimeError as error:
        parser.exit(1, f'{error}\n')
    print(f'Local patches verified; {len(changed)} runtime file(s) updated.')
