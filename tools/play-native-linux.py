#!/usr/bin/env python3
"""Open the Linux development game in its own SDL window with isolated play saves."""
import argparse
import fcntl
import importlib.util
import json
import platform
from pathlib import Path
import subprocess
import sys
import threading

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
import linux_vulkan_sdk
from vulkan_sdk import sha256
from testing_profile import prepare_run
from native_mac_common import recycle_test_disks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    drivers = parser.add_mutually_exclusive_group()
    drivers.add_argument('--driver-manifest', type=Path)
    drivers.add_argument('--wsl-gpu', action='store_true', help='Use the verified private experimental Dozen driver')
    parser.add_argument('--validation', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Linux':
        parser.error('Run with the Linux Python environment')
    game = args.game_dir.resolve()
    if sha256(game / 'default.xbe') != '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc':
        parser.error('Unsupported US Xbox executable')
    exe = ROOT / 'build/linux/game/futureperfect_recomp'
    receipt = json.loads((exe.parent / 'BUILD.json').read_text())
    if sha256(exe) != receipt['executable_sha256']:
        parser.error('Executable differs from its build receipt; rebuild first')
    driver = args.driver_manifest
    if args.wsl_gpu:
        base = ROOT / 'external/mesa-dzn'
        source = json.loads((base / 'SOURCE.json').read_text())
        driver = base / 'install/share/vulkan/icd.d/dzn_icd.x86_64.json'
        files = {'patch_sha256': ROOT / 'tools/build/mesa-dzn-granularity.diff',
                 'library_sha256': base / 'install/lib/x86_64-linux-gnu/libvulkan_dzn.so',
                 'manifest_sha256': driver}
        if source.get('mesa_version') != '26.2.5' or any(source.get(k) != sha256(p) for k, p in files.items()):
            parser.error('Private driver differs from its receipt; run build-wsl-vulkan.py')
    test = ROOT / 'private/test-saves/native-linux'
    if test.is_symlink() or not test.resolve().is_relative_to((ROOT / 'private').resolve()):
        parser.error('Refusing redirected play profile')
    test.mkdir(parents=True, exist_ok=True)
    with (test / 'run.lock').open('a+') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error('Another Linux play/test session is running')
        profile = test / 'manual'
        if not (profile / 'test-run.json').exists():
            prepare_run(ROOT, profile)
        display = profile / 'pc-display.ini'
        if not display.exists():
            display.write_text('TSFP_DISPLAY_V2 640 480 55\n')
        env = linux_vulkan_sdk.probe_environment(linux_vulkan_sdk.verify_install())
        if driver:
            linux_vulkan_sdk.select_driver(env, driver)
        # The actual WSLg Wayland socket may be outside root's XDG directory.
        wsl_runtime = Path('/mnt/wslg/runtime-dir')
        if (wsl_runtime / 'wayland-0').exists():
            env.update(SDL_VIDEODRIVER='wayland', XDG_RUNTIME_DIR=str(wsl_runtime), WAYLAND_DISPLAY='wayland-0')
        env.update(RECOMP_GAME_DIR=str(game), RECOMP_SAVE_DIR=str(profile / 'saves'),
                   RECOMP_INPUT_CONFIG=str(profile / 'controls.json'), RECOMP_DISPLAY_CONFIG=str(profile / 'runtime.ini'),
                   TSFP_DISPLAY_CONFIG=str(display), TSFP_SAVE_LOCATION_LOCK='1',
                   XDG_CONFIG_HOME=str(profile / 'xdg-config'), XDG_DATA_HOME=str(profile / 'xdg-data'),
                   XDG_CACHE_HOME=str(profile / 'xdg-cache'), TSFP_CRASH_DIR=str(profile / 'crashes'),
                   RECOMP_LOG_FILE=str(ROOT / 'logs/linux-manual.err'), RECOMP_TRACE_BUDGET='0',
                   RECOMP_XMV_PLAY='0', RECOMP_FPS_CAP='60', TSFP_NATIVE_FPS='0', TSFP_TEXTURE_MODE='classic',
                   RECOMP_SHADER_CACHE='1', RECOMP_VK_VALIDATION='1' if args.validation else '0')
        # Use the same bounded log drain as the existing native interactive host.
        spec = importlib.util.spec_from_file_location('native_play', ROOT / 'tools/play-native-mac.py')
        play = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(play)
        (ROOT / 'logs').mkdir(exist_ok=True)
        child = subprocess.Popen([str(exe)], cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, start_new_session=True)
        (ROOT / 'logs/linux-manual.pid').write_text(str(child.pid))
        readers = [threading.Thread(target=play.drain_log, args=(stream, ROOT / 'logs' / name))
                   for stream, name in ((child.stdout, 'linux-manual.out'), (child.stderr, 'linux-manual.err'))]
        for reader in readers:
            reader.start()
        try:
            code = child.wait()
        finally:
            for reader in readers:
                reader.join(timeout=5)
            recycle_test_disks(ROOT, test)
        raise SystemExit(code)


if __name__ == '__main__':
    main()
