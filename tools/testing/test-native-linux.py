#!/usr/bin/env python3
"""Bounded Linux game run with isolated saves/settings and two rotating captures."""
import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
import linux_vulkan_sdk
from vulkan_sdk import sha256
from native_mac_common import recycle_test_disks
EXPECTED='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir',type=Path,required=True)
    p.add_argument('--seconds',type=int,default=120)
    p.add_argument('--driver-manifest',type=Path)
    p.add_argument('--capture',action='store_true')
    p.add_argument('--test-keys',action='store_true',help='Read synthetic SDL events from the isolated test-keys.txt')
    args=p.parse_args()
    if platform.system()!='Linux' or platform.machine().lower() not in ('x86_64','amd64'):p.error('Requires Linux x64')
    if not 1<=args.seconds<=600:p.error('Duration must be 1–600 seconds')
    game=args.game_dir.resolve()
    if sha256(game/'default.xbe')!=EXPECTED:p.error('Unsupported XBE')
    build=ROOT/'build/linux/game';exe=build/'futureperfect_recomp'
    manifest=json.loads((build/'BUILD.json').read_text())
    if sha256(exe)!=manifest['executable_sha256']:p.error('Game executable differs from build receipt')
    test=ROOT/'private/test-saves/native-linux';capture=ROOT/'private/captures/native-linux'
    for folder in (test,capture):
        if folder.is_symlink() or not folder.resolve().is_relative_to((ROOT/'private').resolve()):p.error('Redirected test directory')
        folder.mkdir(parents=True,exist_ok=True)
    lock=test/'run.lock'
    # flock remains held until the whole child process group has stopped.
    import fcntl
    with lock.open('a+') as owner:
        try:fcntl.flock(owner,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:p.error('Another Linux test owns this profile')
        for name,source in (('pc-controls.json',ROOT/'tests/fixtures/pc-controls.json'),):
            if not (test/name).exists():shutil.copyfile(source,test/name)
        if not (test/'runtime.conf').exists():(test/'runtime.conf').write_text('# Isolated Linux defaults\n')
        if not (test/'pc-display.ini').exists():(test/'pc-display.ini').write_text('TSFP_DISPLAY_V2 640 480 55\n')
        env=linux_vulkan_sdk.probe_environment(linux_vulkan_sdk.verify_install())
        env.update(RECOMP_GAME_DIR=str(game),RECOMP_SAVE_DIR=str(test),
            RECOMP_INPUT_CONFIG=str(test/'pc-controls.json'),RECOMP_DISPLAY_CONFIG=str(test/'runtime.conf'),
            TSFP_DISPLAY_CONFIG=str(test/'pc-display.ini'),TSFP_SAVE_LOCATION_LOCK='1',
            XDG_CONFIG_HOME=str(test/'xdg-config'),XDG_DATA_HOME=str(test/'xdg-data'),XDG_CACHE_HOME=str(test/'xdg-cache'),
            RECOMP_LOG_FILE=str(ROOT/'logs/linux-game.err'),TSFP_CRASH_DIR=str(test/'crashes'),
            RECOMP_XMV_PLAY='0',RECOMP_FPS_CAP='60',TSFP_NATIVE_FPS='0')
        if args.driver_manifest:env['VK_DRIVER_FILES']=str(args.driver_manifest.resolve(strict=True))
        if args.capture:env.update(RECOMP_HLE_D3D8_DUMP=str(capture/'frame'),RECOMP_HLE_D3D8_DUMP_EVERY='60',RECOMP_HLE_D3D8_DUMP_KEEP_LAST='2')
        if args.test_keys:
            keys=test/'test-keys.txt';keys.touch(exist_ok=True);env['TSFP_NATIVE_TEST_KEYS']=str(keys)
        logs=ROOT/'logs';logs.mkdir(exist_ok=True);started=time.monotonic();timed_out=False;reason='process_exit'
        with (logs/'linux-game.out').open('w') as out,(logs/'linux-game.err').open('w') as err:
            child=subprocess.Popen([str(exe)],cwd=ROOT,env=env,stdout=out,stderr=err,start_new_session=True)
            try:
                while child.poll() is None:
                    if time.monotonic()-started>=args.seconds:timed_out=True;reason='time_limit';break
                    if (logs/'linux-game.out').stat().st_size+(logs/'linux-game.err').stat().st_size>8*1024**2:
                        reason='log_limit';break
                    time.sleep(.1)
            finally:
                if child.poll() is None:
                    os.killpg(child.pid,signal.SIGTERM)
                    try:child.wait(timeout=5)
                    except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait()
                # A terminated game may leave its crash helper waiting on shared state.
                try:os.killpg(child.pid,signal.SIGTERM)
                except ProcessLookupError:pass
        text=(logs/'linux-game.out').read_text(errors='replace')+(logs/'linux-game.err').read_text(errors='replace')
        presents=re.findall(r'\[PRESENT\] backend=\S+ total=(\d+) guest=(\d+) interpolated=(\d+) other=(\d+) skipped=(\d+) failed=(\d+)',text)
        summary=dict(source_revision=manifest['source_revision'],executable_sha256=manifest['executable_sha256'],
            host=platform.platform(),driver_manifest=env.get('VK_DRIVER_FILES','system discovery'),
            returncode=child.returncode,bounded_stop=timed_out,stop_reason=reason,seconds=round(time.monotonic()-started,2),
            entered_guest='Entering translated Xbox entry' in text,
            presents=dict(zip(('total','guest','interpolated','other','skipped','failed'),map(int,presents[-1]))) if presents else None,
            captures=[str(f.relative_to(ROOT)) for f in sorted(capture.glob('*.bmp'))] if args.capture else [],
            scope='Bounded isolated actual game; captures require visual review; not a full playthrough')
        summary['completed_guest_frames_at_least']=max([int(n) for n in re.findall(r'\[NATIVE-GUEST\] completed frame (\d+)',text)],default=0)
        summary['vulkan_validation_errors']=text.count('[VKVAL]')+text.count('[VKVAL error]')
        summary['disposable_test_disks']=recycle_test_disks(ROOT,test)
        (logs/'linux-game.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
        passed=timed_out and summary['presents'] and summary['presents']['guest']>0 and not summary['vulkan_validation_errors']
        if not passed:raise SystemExit(1)

if __name__=='__main__':main()
