#!/usr/bin/env python3
"""Launch the experimental native Mac build for interactive play."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import threading

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_XBE = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'


def check_game(root):
    game = root / 'private/game'
    if not (game / 'default.xbe').is_file():
        raise ValueError(f'Missing game data. Copy the other computer\'s private/game folder to {game}.')
    if hashlib.sha256((game / 'default.xbe').read_bytes()).hexdigest() != EXPECTED_XBE:
        raise ValueError('default.xbe does not match the supported US Xbox release.')
    for name in ('pak', 'xmv'):
        if not (game / name).is_dir() or not any((game / name).iterdir()):
            raise ValueError(f'Missing or empty game asset directory: {game / name}')


def read_controls(path):
    controls = json.loads(path.read_text(encoding='utf-8-sig'))
    mouse = controls.get('mouse', {})
    sensitivity = mouse.get('sensitivity', 0.10)
    if isinstance(sensitivity, bool) or not isinstance(sensitivity, (int, float)) or not math.isfinite(sensitivity) or not 0.001 <= sensitivity <= 10:
        raise ValueError('Mouse sensitivity must be a finite number from 0.001 to 10.')
    return sensitivity, bool(mouse.get('enabled', True)), bool(mouse.get('invert_y', False))


def game_environment(base, root, fps=None):
    env = {k:v for k,v in base.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    settings = root / 'private/native-settings'
    env.update(RECOMP_GAME_DIR=str(root / 'private/game'),
               RECOMP_SAVE_DIR=str(root / 'private/saves'),
               RECOMP_INPUT_CONFIG=str(root / 'config/pc-controls.json'),
               RECOMP_DISPLAY_CONFIG=str(settings / 'runtime.conf'),
               TSFP_DISPLAY_CONFIG=str(root / 'config/pc-display.ini'),
               XDG_CONFIG_HOME=str(settings / 'xdg-config'),
               XDG_DATA_HOME=str(settings / 'xdg-data'), RECOMP_TRACE_BUDGET='0',
               RECOMP_LOG_FILE=str(root / 'logs/native-play.err'))
    if fps not in (None,60):raise ValueError('Only 60 FPS is supported')
    env.update(RECOMP_FRAME_INTERP='1', RECOMP_FRAME_TARGET='0',
               RECOMP_FPS_CAP='60', RECOMP_REFRESH_RATE='0')
    if (root/'private/texture-remaster/pack').is_dir():
        env['TSFP_TEXTURE_PACK_DIR'] = str(root/'private/texture-remaster/pack')
    return env


def prepare_settings(root):
    config = root / 'config/pc-controls.json'
    config.parent.mkdir(parents=True, exist_ok=True)
    if not config.exists():
        with config.open('xb') as output:
            output.write((root / 'config/pc-controls.default.json').read_bytes())
    (root / 'private/saves').mkdir(parents=True, exist_ok=True)
    settings = root / 'private/native-settings'
    settings.mkdir(parents=True, exist_ok=True)
    runtime = settings / 'runtime.conf'
    if not runtime.exists():
        with runtime.open('x') as output:
            output.write('# Native runtime settings; PC settings remain in config/pc-display.ini\n')


def renderer_environment(existing, root, manifest, driver=None):
    """Honor the executable's renderer without changing saved preferences."""
    env = {k:v for k,v in existing.items() if not k.startswith(('VK_', 'SDL_VULKAN', 'DYLD_'))}
    renderer = manifest.get('renderer', 'gl')
    if renderer == 'vulkan':
        import vulkan_sdk
        try:
            gpu = vulkan_sdk.launch_environment(root, manifest, driver)
        except RuntimeError as error:
            raise ValueError(f'Native Vulkan prerequisites: {error}') from error
        env.update(gpu)
    elif renderer != 'gl':
        raise ValueError(f'Unsupported native renderer in BUILD.json: {renderer}')
    return env


def drain_log(stream, path, budget=4*1024*1024):
    # Drain after the cap too, so verbose guest logging cannot stall gameplay.
    with path.open('wb') as output:
        while True:
            chunk = stream.read(65536)
            if not chunk:
                break
            if budget:
                kept = chunk[:budget]
                output.write(kept)
                budget -= len(kept)
    stream.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Read-only prerequisite check')
    parser.add_argument('--fps', type=int, choices=(60,), help='Display target (60 FPS)')
    parser.add_argument('--vulkan-driver', choices=('moltenvk','kosmickrisp'), help='Temporary driver override; default comes from BUILD.json')
    parser.add_argument('--texture-pack', type=Path, help='Use an existing compiled DDS texture pack for this launch')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        raise ValueError('This native build requires an Apple Silicon Mac.')
    check_game(ROOT)
    executable = ROOT / 'build/macos/futureperfect_recomp'
    kind = subprocess.check_output(['file', str(executable)], text=True)
    if 'Mach-O 64-bit executable arm64' not in kind:
        raise ValueError('Build the native runtime with tools/build-native-mac.py first.')
    manifest = json.loads((executable.parent / 'BUILD.json').read_text())
    if hashlib.sha256(executable.read_bytes()).hexdigest() != manifest.get('executable_sha256'):
        raise ValueError('Native executable differs from BUILD.json; rebuild before launching.')
    controls = ROOT / 'config/pc-controls.json'
    read_controls(controls if controls.exists() else ROOT / 'config/pc-controls.default.json')
    if args.check:
        sys.dont_write_bytecode = True
    env = renderer_environment(game_environment(os.environ, ROOT, args.fps), ROOT, manifest, args.vulkan_driver)
    print(f'Native ARM64 build ready: {executable}')
    if args.texture_pack and not args.texture_pack.is_dir():
        raise ValueError('Texture pack directory does not exist.')
    if args.check:
        return 0
    prepare_settings(ROOT)
    logs = ROOT / 'logs'
    logs.mkdir(exist_ok=True)
    print('Arrows/Enter navigate. Click inside the game for mouse-look; click again to fire. Escape pauses/releases the pointer. F1 also toggles capture. F2 opens Settings; F9 toggles FPS; F12 opens Debug; F11 toggles fullscreen. Close the game window to quit.', flush=True)
    print('Experimental: first-level rendering and basic input tested; campaign completion is unverified.', flush=True)
    if args.texture_pack:
        env['TSFP_TEXTURE_PACK_DIR'] = str(args.texture_pack.resolve())
    proc = subprocess.Popen([str(executable)], cwd=ROOT, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    readers = [threading.Thread(target=drain_log, args=(stream, logs / name))
               for stream, name in ((proc.stdout, 'native-play.out'), (proc.stderr, 'native-play.err'))]
    for reader in readers:
        reader.start()
    try:
        result = proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        try:
            result = proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            result = proc.wait()
    finally:
        for reader in readers:
            reader.join()
    return result


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f'Native Mac launch failed: {exc}', file=sys.stderr)
        sys.exit(1)
