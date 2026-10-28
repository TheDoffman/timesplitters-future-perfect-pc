#!/usr/bin/env python3
"""Bounded native boot probe, always using isolated saves and configuration."""

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

import argparse
from contextlib import contextmanager
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

ROOT = _layout_root
EXPECTED_XBE = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'


def isolated_environment(root, test):
    from testing_profile import feedback_dump_path
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    env.update(RECOMP_GAME_DIR=str(root / 'private/game'), RECOMP_SAVE_DIR=str(test), TSFP_SAVE_LOCATION_LOCK='1',
               RECOMP_ICALL_FEEDBACK_PATH=feedback_dump_path(root,test),
               RECOMP_INPUT_CONFIG=str(test / 'pc-controls.json'), RECOMP_DISPLAY_CONFIG=str(test / 'runtime.conf'),
               XDG_CONFIG_HOME=str(test / 'xdg-config'), XDG_DATA_HOME=str(test / 'xdg-data'),
               XDG_CACHE_HOME=str(test / 'xdg-cache'),
               TSFP_DISPLAY_CONFIG=str(test / 'pc-display.ini'),
               RECOMP_TRACE_BUDGET='500', RECOMP_LOG_FILE=str(root / 'logs/native-boot.err'))
    return env


def prepare_environment(root, test, unlocked=False):
    """Use the shared immutable fixture inside the existing native test root."""
    from testing_profile import contained, prepare_run
    test = contained(root, test, 'private/test-saves')
    if unlocked:
        run = test / 'unlocked'
        env = isolated_environment(root, run)
        prepared = prepare_run(root, run)
        env.update({k:v for k,v in prepared.items() if k.startswith(('RECOMP_', 'TSFP_'))})
        for key in ('RECOMP_SAVE_DIR', 'RECOMP_INPUT_CONFIG', 'RECOMP_DISPLAY_CONFIG',
                    'TSFP_DISPLAY_CONFIG', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME'):
            contained(root, Path(env[key]), 'private/test-saves')
        return env
    test.mkdir(parents=True, exist_ok=True)
    config = contained(root, test / 'pc-controls.json', 'private/test-saves')
    if not config.exists():
        shutil.copyfile(root / 'tests/fixtures/pc-controls.json', config)
    runtime = contained(root, test / 'runtime.conf', 'private/test-saves')
    if not runtime.exists():
        runtime.write_text('# Isolated native boot defaults\n')
    contained(root, test / 'pc-display.ini', 'private/test-saves')
    env = isolated_environment(root, test)
    for key in ('XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME'):
        contained(root, Path(env[key]), 'private/test-saves')
    return env


@contextmanager
def test_session(root, test, unlocked=False):
    """Hold native/profile ownership until the child and disk cleanup finish."""
    from testing_profile import contained
    test = contained(root, test, 'private/test-saves')
    if test != root.resolve() / 'private/test-saves/native-mac':
        raise ValueError('Unexpected native test root')
    test.mkdir(parents=True, exist_ok=True)
    locks = []
    prepared = False
    try:
        lock = test / 'run.lock'
        with lock.open('x') as stream:
            stream.write(str(os.getpid()))
        locks.append(lock)
        env = prepare_environment(root, test, unlocked)
        if unlocked:
            lock = test / 'unlocked/run.lock'
            with lock.open('x') as stream:
                stream.write(str(os.getpid()))
            locks.append(lock)
        prepared = True
        yield env
    finally:
        try:
            if prepared:
                recycle_test_disks(root, test)
        finally:
            for lock in reversed(locks):
                lock.unlink()


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


def present_counts(errors):
    """Last cumulative API-acceptance snapshot, never inferred from guest ticks."""
    samples=re.findall(r'\[PRESENT\] backend=\S+ total=(\d+) guest=(\d+) '
                       r'interpolated=(\d+) other=(\d+) skipped=(\d+) failed=(\d+)',errors)
    if not samples:return None
    return dict(zip(('total','guest','interpolated','other','skipped','failed'),map(int,samples[-1])))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=15,
                        help='Bounded run duration, 1–1800 seconds (default: 15)')
    parser.add_argument('--unlocked-profile', action='store_true',
                        help='Reset a disposable copy of the local Test Lab fixture and skip startup movies')
    parser.add_argument('--capture', action='store_true', help='Retain two rotating native frame captures')
    parser.add_argument('--test-keys', action='store_true', help='Read synthetic SDL key/mouse events from isolated test-keys.txt')
    parser.add_argument('--trace-render', action='store_true', help='Enable bounded frame tracing on request')
    parser.add_argument('--profile-frames', action='store_true',
                        help='Record up to 20,000 host present intervals, without GPU readback')
    parser.add_argument('--profile-without-validation', action='store_true',
                        help='With --profile-frames, measure Vulkan without validation-layer overhead; not a validation pass')
    parser.add_argument('--fps', type=int, choices=(60,), help='Override the native display target for this test only')
    parser.add_argument('--vulkan-driver',choices=('moltenvk','kosmickrisp'),
                        help='Temporary Vulkan driver override; default comes from BUILD.json')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('This probe requires an Apple Silicon Mac.')
    if not 1 <= args.seconds <= 1800:
        parser.error('--seconds must be between 1 and 1800')
    if args.profile_without_validation and not args.profile_frames:
        parser.error('--profile-without-validation requires --profile-frames')
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
    with test_session(ROOT, test, args.unlocked_profile) as env:
        return run_probe(args, test, env, exe, kind)


def run_probe(args, test, env, exe, kind):
    manifest_path=ROOT/'build/macos/BUILD.json'
    manifest=json.loads(manifest_path.read_text()) if manifest_path.is_file() else {}
    if manifest.get('renderer')=='vulkan':
        import vulkan_sdk
        env=vulkan_sdk.with_probe_gpu_environment(env,vulkan_sdk.verify_install(),
            vulkan_sdk.select_driver(manifest,getattr(args,'vulkan_driver',None)))
        env['RECOMP_VK_VALIDATION']='0' if getattr(args,'profile_without_validation',False) else '1'
    env.update(RECOMP_FRAME_INTERP='1', RECOMP_FRAME_TARGET='0',
               RECOMP_FPS_CAP='60', RECOMP_REFRESH_RATE='0')
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
    print(f'Native probe: {exe}\nIsolated saves: {env["RECOMP_SAVE_DIR"]}\nLimit: {args.seconds}s, 8 MiB logs', flush=True)
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
    counts = present_counts(errors)
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
                presented_frames_at_least=counts['total'] if counts else max((int(n) for n, _ in presented), default=0),
                host_present_counts_at_least=counts,
                sampled_gl_errors=[int(code, 16) for _, code in presented if int(code, 16)],
                native_fault='[NATIVE-FAULT]' in errors,
                rendering_connected=('host rendering connected' in errors and
                    ('[d3d8_gl] GL ' in errors or '[native-rhi] common Xbox renderer' in errors)),
                renderer=manifest.get('renderer','gl'),
                host_present_counter_available=counts is not None or bool(presented),
                shader_compile_failures=errors.count('compile failed'),
                vulkan_validation_errors=len(re.findall(r'\[VKVAL\]',errors)))
    cleanup = recycle_test_disks(ROOT, test)
    result = dict(boot=boot, disposable_test_disks=cleanup, branch=subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
                  source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  working_tree_changes=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
                  host=platform.platform(), executable=kind, xbe_sha256=EXPECTED_XBE,
                  executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                  seconds=round(time.monotonic()-started, 2), reason=reason, exit_code=proc.returncode,
                  test_directory=str(test.relative_to(ROOT)),
                  unlocked_profile=bool(args.unlocked_profile),
                  overrides={k:v for k,v in env.items() if k.startswith(('RECOMP_', 'TSFP_'))},
                  vulkan_driver=(vulkan_sdk.select_driver(manifest,getattr(args,'vulkan_driver',None)) if manifest.get('renderer')=='vulkan' else None),
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
    return 0 if (reason == 'time_limit' and frames and not boot['native_fault']
                 and not boot['shader_compile_failures'] and not boot['vulkan_validation_errors']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
