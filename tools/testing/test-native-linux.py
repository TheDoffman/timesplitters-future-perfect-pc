#!/usr/bin/env python3
"""Bounded Linux game run with isolated saves/settings and two rotating captures."""
import argparse
import json
import hashlib
import math
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import struct
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

def test_frame_rate(test,fps):
    # Saved preferences override TSFP_NATIVE_FPS. Reset only this disposable
    # profile to the requested preset, refusing redirected timing files.
    if fps not in (60,120,180,240):raise ValueError('Unsupported frame-rate preset')
    fd=os.open(test/'pc-frame-rate.ini',os.O_WRONLY|os.O_CREAT|os.O_TRUNC|os.O_NOFOLLOW,0o600)
    with os.fdopen(fd,'w') as output:output.write(f'TSFP_FRAME_RATE_V1 {0 if fps==60 else fps}\n')


def input_script(path,seconds):
    if path.stat().st_size>65536:raise ValueError('Input script is too large')
    data=json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(data,dict) or data.get('schema')!=1 or not isinstance(data.get('events'),list) or len(data['events'])>256:
        raise ValueError('Invalid input script')
    events=[];previous=-1
    for event in data['events']:
        if not isinstance(event,dict):raise ValueError('Invalid input event')
        at=event.get('at');commands=event.get('commands')
        if type(at) not in (int,float) or not math.isfinite(at) or not previous<=at<seconds or at<0:
            raise ValueError('Input events must be ordered inside the bounded run')
        if not isinstance(commands,list) or not 1<=len(commands)<=16 or any(
            not isinstance(line,str) or not re.fullmatch(r'(?:focus [01]|[+-][A-Za-z0-9 ]{1,32})',line) for line in commands):
            raise ValueError('Only bounded synthetic keyboard/focus commands are allowed')
        events.append((at,'\n'.join(commands)+'\n'));previous=at
    return events


def story_snapshot(word):
    """Same supported-US scene/flags observed by native_refresh_game.py."""
    scene,flags,pointer=word(0x79094C),word(0x7DE458),word(0x761C48)
    if pointer and not 0x10000<=pointer<0x84000000:raise ValueError('Invalid Story pause pointer')
    paused=word(pointer)&1 if pointer else 0
    return dict(scene=scene,flags=flags,paused=paused,active=scene==2 and not flags&0x20000000 and not paused)


def story_window(samples,frames):
    import csv
    import native_frame_stats
    windows=[];start=None
    for sample in samples:
        if sample['active']:
            if start is None:start=sample['clock_ns']
            end=sample['clock_ns']
        elif start is not None:windows.append((start,end));start=None
    if start is not None:windows.append((start,end))
    if not windows:raise ValueError('No active Story gameplay observed')
    start,end=max(windows,key=lambda span:span[1]-span[0])
    with frames.open() as source:rows=list(csv.DictReader(source))
    if len(rows)<2:raise ValueError('No complete frame timing evidence')
    start=max(start+5_000_000_000,int(rows[0]['clock_ns'])+1)
    end=min(end,int(rows[-1]['clock_ns'])-1)
    if end-start<15_000_000_000:raise ValueError('Need at least 15 measured seconds of active, unpaused Story gameplay')
    result=native_frame_stats.summarize_clock_window(frames,start,end)
    result.update(start_ns=start,end_ns=end,scope='State-verified active Story, excluding five-second warmup; host presents, not scanout')
    return result


def validation_active(text,requested):
    return bool(requested and re.search(r'\[RHI\] vulkan: [^\n]*validation on',text))

def runtime_target(package_root=None):
    build=Path(package_root).resolve() if package_root else ROOT/'build/linux/game'
    exe=build/('Bin/futureperfect_recomp' if package_root else 'futureperfect_recomp')
    manifest=json.loads((build/'BUILD.json').read_text())
    expected=manifest.get('files',{}).get('Bin/futureperfect_recomp') if package_root else manifest.get('executable_sha256')
    if not expected or sha256(exe)!=expected:raise ValueError('Game executable differs from build receipt')
    if package_root:
        native=manifest.get('native_build',{})
        manifest=dict(manifest,source_revision=manifest.get('source_commit',native.get('source_revision','unknown')),
                      executable_sha256=expected,host_optimization=native.get('host_optimization','unknown packaged host'))
    return build,exe,manifest

def runtime_environment(package_root=None):
    if not package_root:return linux_vulkan_sdk.probe_environment(linux_vulkan_sdk.verify_install())
    # Relocation acceptance must not find the checkout SDK through LD_LIBRARY_PATH.
    env={k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_','TSFP_','VK_','SDL_VULKAN','DYLD_'))
         and k not in ('LD_LIBRARY_PATH','LD_PRELOAD','PYTHONPATH','PYTHONHOME')}
    root=Path(package_root).resolve()
    env.update(RECOMP_FFMPEG_DIR=str(root/'Bin'),RECOMP_VK_VALIDATION='1',RECOMP_SHADER_CACHE='0',
        TSFP_ONLINE_PYTHON=str(root/'Support/Python/bin/python3'),TSFP_ONLINE_SCRIPT=str(root/'Support/Online/online.py'),
        TSFP_ONLINE_MANIFEST=str(root/'Support/Online/network-identity.json'))
    return env

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir',type=Path,required=True)
    p.add_argument('--package-root',type=Path,help='Verify and run a relocated package without the checkout SDK')
    p.add_argument('--seconds',type=int,default=120)
    p.add_argument('--driver-manifest',type=Path)
    p.add_argument('--capture',action='store_true')
    p.add_argument('--movies',action='store_true',help='Play startup XMV movies instead of skipping them')
    p.add_argument('--performance',action='store_true',help='Measure interactive settings: shader cache on, validation off (not validation acceptance)')
    p.add_argument('--fps',type=int,choices=(60,120,180,240),default=60,
                   help='Explicit isolated timing preset; high refresh retains the existing beta policy')
    p.add_argument('--unlocked-profile',action='store_true',help='Use the shared local Test Lab fixture in a disposable nested profile')
    p.add_argument('--profile-name',default='unlocked',help='Separate disposable fixture copy; preserves other runs')
    p.add_argument('--frame-profile',action='store_true',help='Record bounded actual present intervals and timing percentiles')
    p.add_argument('--profile-start',type=int,default=0,help='Seconds after first present before collecting frame timings')
    p.add_argument('--input-script',type=Path,help='Bounded synthetic keyboard schedule, run only in the isolated profile')
    p.add_argument('--upload-profile',action='store_true',help='Record opt-in Vulkan upload and wait counters')
    p.add_argument('--upload-baseline',action='store_true',help='Use the legacy upload copies for a same-binary A/B comparison')
    p.add_argument('--story-benchmark',action='store_true',help='Observe only this child scene/flags, skip its intro, and require a gameplay timing window')
    p.add_argument('--test-keys',action='store_true',help='Read synthetic SDL events from the isolated test-keys.txt')
    args=p.parse_args()
    if platform.system()!='Linux' or platform.machine().lower() not in ('x86_64','amd64'):p.error('Requires Linux x64')
    if not 1<=args.seconds<=600:p.error('Duration must be 1–600 seconds')
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,47}',args.profile_name) or args.profile_name in ('manual','crashes','xdg-cache','xdg-config','xdg-data'):
        p.error('Use a simple disposable profile name, excluding reserved folders')
    if not 0<=args.profile_start<args.seconds:p.error('Profile start must be inside the bounded run')
    if args.story_benchmark and not (args.input_script and args.unlocked_profile and args.frame_profile):
        p.error('Story benchmarks require an isolated fixture, input script and frame profiling')
    try:events=input_script(args.input_script,args.seconds) if args.input_script else []
    except (ValueError,OSError) as error:p.error(str(error))
    game=args.game_dir.resolve()
    if sha256(game/'default.xbe')!=EXPECTED:p.error('Unsupported XBE')
    try:build,exe,manifest=runtime_target(args.package_root)
    except (ValueError,OSError) as error:p.error(str(error))
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
        test_frame_rate(test,args.fps)
        env=runtime_environment(args.package_root)
        env.update(RECOMP_GAME_DIR=str(game),RECOMP_SAVE_DIR=str(test),
            RECOMP_INPUT_CONFIG=str(test/'pc-controls.json'),RECOMP_DISPLAY_CONFIG=str(test/'runtime.conf'),
            TSFP_DISPLAY_CONFIG=str(test/'pc-display.ini'),TSFP_SAVE_LOCATION_LOCK='1',
            XDG_CONFIG_HOME=str(test/'xdg-config'),XDG_DATA_HOME=str(test/'xdg-data'),XDG_CACHE_HOME=str(test/'xdg-cache'),
            RECOMP_LOG_FILE=str(ROOT/'logs/linux-game.err'),TSFP_CRASH_DIR=str(test/'crashes'),
            RECOMP_XMV_PLAY='1' if args.movies else '0',RECOMP_FPS_CAP='60',TSFP_NATIVE_FPS=str(0 if args.fps==60 else args.fps),RECOMP_TRACE_BUDGET='500')
        if args.unlocked_profile:
            from testing_profile import prepare_run
            prepared=prepare_run(ROOT,test/args.profile_name)
            for key in ('RECOMP_SAVE_DIR','RECOMP_INPUT_CONFIG','RECOMP_ICALL_FEEDBACK_PATH',
                        'TSFP_TEXTURE_PACK_DIR','TSFP_TEXTURE_MODE'):
                env[key]=prepared[key]
        if args.driver_manifest:linux_vulkan_sdk.select_driver(env,args.driver_manifest)
        if args.performance:env.update(RECOMP_VK_VALIDATION='0',RECOMP_SHADER_CACHE='1')
        if args.upload_profile:env['TSFP_RHI_UPLOAD_PROFILE']='1'
        if args.upload_baseline:env['TSFP_RHI_UPLOAD_BASELINE']='1'
        if args.frame_profile:
            env.update(TSFP_FRAME_PROFILE=str(ROOT/'logs/linux-game-frames.csv'),
                       TSFP_FRAME_PROFILE_DELAY_MS=str(args.profile_start*1000))
        if args.capture:env.update(RECOMP_HLE_D3D8_DUMP=str(capture/'frame'),RECOMP_HLE_D3D8_DUMP_EVERY='60',RECOMP_HLE_D3D8_DUMP_KEEP_LAST='2')
        if args.test_keys or args.input_script:
            keys=test/'test-keys.txt'
            flags=os.O_WRONLY|os.O_CREAT|os.O_NOFOLLOW|(os.O_TRUNC if args.input_script else os.O_APPEND)
            os.close(os.open(keys,flags,0o600));env['TSFP_NATIVE_TEST_KEYS']=str(keys)
        logs=ROOT/'logs';logs.mkdir(exist_ok=True);started=time.monotonic();timed_out=False;reason='process_exit'
        (logs/'linux-game.json').write_text(json.dumps(dict(completed=False,source_revision=manifest['source_revision'])))
        states=[];memory_fd=None;offset=None;next_state=0;skip_at=None;resumed=False;cutscene_since=None
        import resource
        cpu_before=resource.getrusage(resource.RUSAGE_CHILDREN)
        with (logs/'linux-game.out').open('w') as out,(logs/'linux-game.err').open('w') as err:
            child=subprocess.Popen([str(exe)],cwd=ROOT,env=env,stdout=out,stderr=err,start_new_session=True)
            try:
                while child.poll() is None:
                    elapsed=time.monotonic()-started
                    if args.story_benchmark and elapsed>=next_state:
                        next_state=elapsed+.5
                        if offset is None:
                            boot=(logs/'linux-game.out').read_text(errors='replace')
                            match=re.search(r'Xbox memory offset: (0x[0-9a-fA-F]+)',boot)
                            if match:
                                offset=int(match[1],16);memory_fd=os.open(f'/proc/{child.pid}/mem',os.O_RDONLY|os.O_CLOEXEC)
                            elif elapsed>15:raise RuntimeError('Owned child did not report its guest mapping')
                        if offset is not None:
                            def word(address):return struct.unpack('<I',os.pread(memory_fd,4,offset+address))[0]
                            state=story_snapshot(word);state.update(seconds=round(elapsed,3),clock_ns=time.monotonic_ns())
                            if not states or (state['scene'],state['flags']&0x20000000,state['paused'])!=(states[-1]['scene'],states[-1]['flags']&0x20000000,states[-1]['paused']):
                                print(f"[STORY-STATE] pid={child.pid} seconds={elapsed:.3f} scene={state['scene']} cinematic={int(bool(state['flags']&0x20000000))} paused={state['paused']}",flush=True)
                            states.append(state)
                            # The US opening movie uses scene24, before Story
                            # scene2. Match the existing Windows observer: the
                            # cinematic flag identifies the skippable intro.
                            cinematic=bool(state['flags']&0x20000000)
                            if cinematic and cutscene_since is None:cutscene_since=elapsed
                            if cinematic and elapsed>=50 and cutscene_since is not None and elapsed-cutscene_since>=2 and skip_at is None:
                                skip_at=elapsed;events.extend([(elapsed,'+Escape\n'),(elapsed+.2,'-Escape\n')]);events.sort()
                            if skip_at is not None and .5<elapsed-skip_at<5 and state['paused'] and not resumed:
                                resumed=True;events.extend([(elapsed,'+E\n'),(elapsed+.2,'-E\n')]);events.sort()
                    while events and time.monotonic()-started>=events[0][0]:
                        _,commands=events.pop(0)
                        fd=os.open(keys,os.O_WRONLY|os.O_APPEND|os.O_NOFOLLOW)
                        with os.fdopen(fd,'w') as stream:stream.write(commands)
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
                if memory_fd is not None:os.close(memory_fd)
        cpu_after=resource.getrusage(resource.RUSAGE_CHILDREN)
        text=(logs/'linux-game.out').read_text(errors='replace')+(logs/'linux-game.err').read_text(errors='replace')
        presents=re.findall(r'\[PRESENT\] backend=\S+ total=(\d+) guest=(\d+) interpolated=(\d+) other=(\d+) skipped=(\d+) failed=(\d+)',text)
        summary=dict(source_revision=manifest['source_revision'],executable_sha256=manifest['executable_sha256'],
            host=platform.platform(),driver_manifest=env.get('VK_DRIVER_FILES','system discovery'),
            validation_requested=not args.performance,validation_enabled=validation_active(text,not args.performance),shader_cache_enabled=args.performance,
            host_optimization=manifest.get('host_optimization','legacy unoptimized host'),
            cpu_user_seconds=round(cpu_after.ru_utime-cpu_before.ru_utime,3),
            cpu_system_seconds=round(cpu_after.ru_stime-cpu_before.ru_stime,3),
            requested_fps=args.fps,
            upload_optimization=not args.upload_baseline,
            input_script_sha256=hashlib.sha256(args.input_script.read_bytes()).hexdigest() if args.input_script else None,
            requested_timing_observed=('[native] fixed 60 FPS' if args.fps==60 else f'[native] target {args.fps} fresh game FPS') in text,
            profile='Test Lab disposable copy' if args.unlocked_profile else 'isolated fresh/local test profile',
            returncode=child.returncode,bounded_stop=timed_out,stop_reason=reason,seconds=round(time.monotonic()-started,2),
            entered_guest='Entering translated Xbox entry' in text,
            presents=dict(zip(('total','guest','interpolated','other','skipped','failed'),map(int,presents[-1]))) if presents else None,
            captures=[str(f.relative_to(ROOT)) for f in sorted(capture.glob('*.bmp'))] if args.capture else [],
            scope='Bounded isolated actual game; captures require visual review; not a full playthrough')
        summary['completed_guest_frames_at_least']=max([int(n) for n in re.findall(r'\[NATIVE-GUEST\] completed frame (\d+)',text)],default=0)
        summary['vulkan_validation_errors']=text.count('[VKVAL]')+text.count('[VKVAL error]')
        if args.upload_profile:
            summary['upload_metrics']=[{key:int(value) for key,value in re.findall(r'(\w+)=(\d+)',line)}
                                       for line in text.splitlines() if line.startswith('[VK-UPLOAD]')]
            if not summary['upload_metrics']:raise RuntimeError('No upload metrics: rebuild the Vulkan runtime with profiling support')
        analysis_ok=True
        if args.frame_profile:
            import native_frame_stats
            summary['frame_timings']=native_frame_stats.summarize(ROOT/'logs/linux-game-frames.csv',args.profile_start)
            summary['frame_timings']['scope']='Successful host-present intervals; not GPU completion or scanout'
        if args.story_benchmark:
            summary['story_samples']=states
            try:summary['gameplay_timings']=story_window(states,ROOT/'logs/linux-game-frames.csv')
            except ValueError as error:summary['gameplay_error']=str(error);analysis_ok=False
        summary['disposable_test_disks']=recycle_test_disks(ROOT,test,save_directory=Path(env['RECOMP_SAVE_DIR']))
        (logs/'linux-game.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
        passed=analysis_ok and timed_out and summary['requested_timing_observed'] and summary['presents'] and summary['presents']['guest']>0 and not summary['vulkan_validation_errors']
        if not passed:raise SystemExit(1)

if __name__=='__main__':main()
