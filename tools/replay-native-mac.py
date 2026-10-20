#!/usr/bin/env python3
"""Replay one captured Xbox frame on native Mac, with a 30-second limit."""
import argparse
import json
from pathlib import Path
import platform
import subprocess
import time
import vulkan_sdk

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--driver', choices=('kosmickrisp', 'moltenvk'), default=vulkan_sdk.DEFAULT_DRIVER)
    parser.add_argument('--draws', type=int, help='Replay only this many draws')
    parser.add_argument('--skip-draw', type=int, help='Skip this zero-based draw')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Requires Apple Silicon macOS')
    for value in (args.draws, args.skip_draw):
        if value is not None and not 0 <= value <= 100000:
            parser.error('Draw limits must be between 0 and 100000')
    capture = args.capture.resolve(strict=True)
    executable = ROOT/'build/macos/rhi-tests/replay/d3d8_replay'
    if not executable.is_file():
        parser.error('Build and validate replay first with tools/test-rhi-mac.py')
    env = vulkan_sdk.probe_environment(vulkan_sdk.verify_install(), args.driver)
    runtime = ROOT/'build/macos/rhi-tests/runtime'
    env.update(RECOMP_DISPLAY_CONFIG=str(runtime/'replay-runtime.ini'),
               TSFP_DISPLAY_CONFIG=str(runtime/'replay-display.ini'),
               RECOMP_INPUT_CONFIG=str(runtime/'replay-controls.json'),
               RECOMP_SAVE_DIR=str(runtime/'replay-saves'),
               RECOMP_FRAME_INTERP='1', RECOMP_FPS_CAP='0', RECOMP_VK_VALIDATION='1')
    output = ROOT/'private/captures/native-mac/replay'
    if output.is_symlink() or not output.resolve().is_relative_to(ROOT.resolve()):
        raise RuntimeError('Unexpected replay output path')
    output.mkdir(parents=True, exist_ok=True)
    image = output/'frame-000.bmp'
    image.unlink(missing_ok=True)
    command = [str(executable), str(capture), '--backend', 'vulkan', '--out', str(output/'frame-')]
    if args.draws is not None:
        command += ['--draws', str(args.draws)]
    if args.skip_draw is not None:
        command += ['--skip-draw', str(args.skip_draw)]
    started = time.monotonic()
    result = subprocess.run(command, env=env, cwd=ROOT, capture_output=True, text=True, timeout=30)
    logs = ROOT/'logs'
    logs.mkdir(exist_ok=True)
    text = result.stdout + result.stderr
    (logs/'native-frame-replay.log').write_text(text)
    summary = dict(source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                   working_tree_changes=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
                   executable_sha256=vulkan_sdk.sha256(executable), capture_sha256=vulkan_sdk.sha256(capture),
                   host=platform.platform(), driver=args.driver, sdk_version=vulkan_sdk.pin()['version'],
                   seconds=round(time.monotonic()-started, 3), returncode=result.returncode,
                   draws=args.draws, skip_draw=args.skip_draw, scope='One captured frame; not guest execution or gameplay')
    (logs/'native-frame-replay.json').write_text(json.dumps(summary, indent=2)+'\n')
    result.check_returncode()
    vulkan_sdk.require_clean_validation(text)
    if not image.is_file():
        raise RuntimeError('Replay did not produce a fresh frame; see logs/native-frame-replay.log')
    print(image)


if __name__ == '__main__':
    main()
