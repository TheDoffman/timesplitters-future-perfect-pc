#!/usr/bin/env python3
"""Bounded native boot probe, always using isolated saves and configuration."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
from native_mac_common import recycle_test_disks
import shutil
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_XBE = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'


def isolated_environment(root, test):
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    env.update(RECOMP_GAME_DIR=str(root / 'private/game'), RECOMP_SAVE_DIR=str(test),
               RECOMP_INPUT_CONFIG=str(test / 'pc-controls.json'), RECOMP_DISPLAY_CONFIG=str(test / 'runtime.conf'),
               XDG_CONFIG_HOME=str(test / 'xdg-config'), XDG_DATA_HOME=str(test / 'xdg-data'),
               TSFP_DISPLAY_CONFIG=str(test / 'pc-display.ini'),
               RECOMP_TRACE_BUDGET='500')
    return env

def capture_environment(root):
    captures = root / 'private/captures/native-mac'
    if captures.is_symlink() or not captures.resolve().is_relative_to(root.resolve() / 'private'):
        raise RuntimeError('Unexpected native capture directory')
    captures.mkdir(parents=True, exist_ok=True)
    for name in ('frame000.bmp', 'frame001.bmp'):
        if (captures / name).is_symlink():
            raise RuntimeError('Unexpected native capture file')
    return dict(RECOMP_HLE_D3D8_DUMP=str(captures / 'frame'),
                RECOMP_HLE_D3D8_DUMP_EVERY='60', RECOMP_HLE_D3D8_DUMP_KEEP_LAST='2')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=15,
                        help='Bounded run duration, 1–1800 seconds (default: 15)')
    parser.add_argument('--capture', action='store_true', help='Retain two rotating native frame captures')
    parser.add_argument('--test-keys', action='store_true', help='Read synthetic SDL key/mouse events from isolated test-keys.txt')
    parser.add_argument('--trace-render', action='store_true', help='Enable bounded frame tracing on request')
    parser.add_argument('--profile-frames', action='store_true',
                        help='Record up to 20,000 host present intervals, without GPU readback')
    parser.add_argument('--fps', type=int, choices=(60,120), help='Override the native display target for this test only')
    parser.add_argument('--capture-interpolation', action='store_true', help='Capture one guest/midpoint pair when capture-interpolation.txt requests it')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('This probe requires an Apple Silicon Mac.')
    if not 1 <= args.seconds <= 1800:
        parser.error('--seconds must be between 1 and 1800')
    xbe = ROOT / 'private/game/default.xbe'
    if hashlib.sha256(xbe.read_bytes()).hexdigest() != EXPECTED_XBE:
        raise SystemExit('Unsupported game executable')
    exe = ROOT / 'build/macos/futureperfect_recomp'
    kind = subprocess.check_output(['file', str(exe)], text=True).strip()
    if 'Mach-O 64-bit executable arm64' not in kind:
        raise SystemExit('Expected a native ARM64 Mach-O build')
    test = ROOT / 'private/test-saves/native-mac'
    if test.is_symlink() or not test.resolve().is_relative_to((ROOT / 'private/test-saves').resolve()):
        raise SystemExit('Unexpected isolated test path')
    test.mkdir(parents=True, exist_ok=True)
    config = test / 'pc-controls.json'
    if not config.exists():
        shutil.copyfile(ROOT / 'tests/fixtures/pc-controls.json', config)
    runtime_config = test / 'runtime.conf'
    if not runtime_config.exists():
        runtime_config.write_text('# Isolated native boot defaults\n')
    env = isolated_environment(ROOT, test)
    if args.fps:
        env.update(RECOMP_FRAME_INTERP='2' if args.fps==120 else '1',
                   RECOMP_FPS_CAP='60' if args.fps==120 else 'adaptive')
    if args.capture_interpolation:
        capture_environment(ROOT) # Same path and symlink checks as normal captures.
        captures=ROOT / 'private/captures/native-mac'
        request=test / 'capture-interpolation.txt'
        for path in (request,captures/'interpolation-guest.bmp',captures/'interpolation-midpoint.bmp'):
            if path.is_symlink():raise SystemExit('Unexpected interpolation capture path')
        request.write_text('')
        env.update(TSFP_NATIVE_INTERP_CAPTURE_REQUEST=str(request),
                   TSFP_NATIVE_INTERP_CAPTURE_PREFIX=str(captures/'interpolation'))
    if args.test_keys:
        keys = test / 'test-keys.txt'
        if keys.is_symlink():raise SystemExit('Unexpected test key file')
        keys.write_text('')
        env['TSFP_NATIVE_TEST_KEYS'] = str(keys)
    if args.trace_render:
        trace = test / 'trace-render.txt'
        if trace.is_symlink():raise SystemExit('Unexpected trace file')
        trace.write_text('')
        env['TSFP_NATIVE_TRACE_RENDER'] = str(trace)
    if args.capture:
        env.update(capture_environment(ROOT))
    logs = ROOT / 'logs'
    logs.mkdir(exist_ok=True)
    if args.profile_frames:
        profile = logs / 'native-frame-times.csv'
        if profile.is_symlink():raise SystemExit('Unexpected frame profile file')
        profile.write_text('')
        env['TSFP_NATIVE_FRAME_PROFILE'] = str(profile)
    out_path, err_path = logs / 'native-boot.out', logs / 'native-boot.err'
    print(f'Native probe: {exe}\nIsolated saves: {test}\nLimit: {args.seconds}s, 8 MiB logs', flush=True)
    started = time.monotonic()
    reason = 'process_exit'
    with out_path.open('wb') as out, err_path.open('wb') as err:
        proc = subprocess.Popen([str(exe)], cwd=ROOT, env=env, stdout=out, stderr=err, start_new_session=True)
        try:
            while proc.poll() is None:
                if time.monotonic() - started >= args.seconds:
                    reason = 'time_limit'; break
                if out_path.stat().st_size + err_path.stat().st_size > 8 * 1024 * 1024:
                    reason = 'log_limit'; break
                time.sleep(0.1)
        finally:
            if proc.poll() is None:
                os.killpg(proc.pid, signal.SIGTERM)
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(proc.pid, signal.SIGKILL); proc.wait()
    output = out_path.read_text(errors='replace')
    errors = err_path.read_text(errors='replace')
    frames = [int(n) for n in re.findall(r'\[NATIVE-GUEST\] completed frame (\d+)', errors)]
    presented = re.findall(r'\[native-gl\] presented frame (\d+), GL error=0x([0-9a-fA-F]+)', errors)
    captures = []
    if args.capture:
        for name in ('frame000.bmp', 'frame001.bmp'):
            path = ROOT / 'private/captures/native-mac' / name
            if path.exists() and path.stat().st_mtime >= time.time() - (time.monotonic() - started):
                captures.append(dict(path=str(path.relative_to(ROOT)), bytes=path.stat().st_size,
                                     sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    boot = dict(guest_entry_reached='Entering translated Xbox entry' in output,
                graphics_initialized='graphics initialization returned 0x00000000' in errors,
                completed_guest_frames_at_least=max(frames, default=0),
                presented_frames_at_least=max((int(n) for n, _ in presented), default=0),
                sampled_gl_errors=[int(code, 16) for _, code in presented if int(code, 16)],
                native_fault='[NATIVE-FAULT]' in errors,
                rendering_connected='[d3d8_gl] GL ' in errors and 'host rendering connected' in errors)
    cleanup = recycle_test_disks(ROOT, test)
    result = dict(boot=boot, disposable_test_disks=cleanup, branch=subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
                  source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  working_tree_changes=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
                  host=platform.platform(), executable=kind, xbe_sha256=EXPECTED_XBE,
                  executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                  seconds=round(time.monotonic()-started, 2), reason=reason, exit_code=proc.returncode,
                  test_directory=str(test.relative_to(ROOT)),
                  overrides={k:v for k,v in env.items() if k.startswith(('RECOMP_', 'TSFP_'))},
                  frame_captures=captures,
                  rendered_game_verified=False) # Visual recognition is a separate reviewed result.
    if args.profile_frames:
        from native_frame_stats import summarize
        try:
            timing = summarize(profile)
        except ValueError:
            timing = None
        result['frame_profile'] = dict(path=str(profile.relative_to(ROOT)),
                                       sha256=hashlib.sha256(profile.read_bytes()).hexdigest(),
                                       whole_run_including_loading=timing,
                                       row_limit=20000,
                                       metric='CPU wall-clock intervals between host presents, not display scanout')
    (logs / 'native-boot-summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k:result[k] for k in ('seconds', 'reason', 'exit_code')}, indent=2))
    # A process exit of zero can be the guest rebooting after a startup error.
    # Count this milestone only when real frame calls survive to our time limit.
    return 0 if reason == 'time_limit' and frames and not boot['native_fault'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
