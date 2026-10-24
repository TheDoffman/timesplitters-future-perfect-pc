"""Bounded Windows native-update test; uses only disposable Test Lab data.

CPU submission times do not prove monitor scanout or perceptual smoothness.
--capture retains bounded guest-frame BMPs for motion/fidelity review.
"""
import argparse
import ctypes
import hashlib
import os
import re
from pathlib import Path
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from testing_profile import XBE_SHA256, prepare_run, recycle_disks, check_fixture
from native_entity_trace import snapshot_entities
from native_effect_trace import snapshot_effects


def guest_memory_ready(read_bytes, elapsed):
    """Allow guest mapping to finish before any observer reads its globals.

    Keep the existing five-second native-observation deadline: failure to map
    is a failed test, rather than an indefinitely empty effects trace.
    """
    scene = read_bytes(0x79094c, 4)
    if scene is not None and len(scene) == 4:
        return True
    if elapsed >= 5:
        raise RuntimeError('Guest memory is unreadable after the five-second startup deadline')
    return False


def recycle_captures(run, tag):
    """Keep three frames from this run, never adjacent evidence or linked files.

    The runtime KEEP_LAST option rotates only its final slots; it also retains
    its early frames. Enforce this runner's smaller budget during capture.
    """
    if not re.fullmatch(r'[a-zA-Z0-9-]+',tag):
        raise ValueError('Invalid capture tag')
    root=run.resolve(strict=True)
    pattern=re.compile(re.escape(tag)+r'-frame(?:[0-9]{3}|_s[0-9]{8,})\.bmp')
    files=[path for path in run.glob(tag+'-frame*.bmp') if pattern.fullmatch(path.name)]
    for path in files:
        if path.is_symlink() or path.resolve().parent!=root:
            raise ValueError('Capture path escaped test root or is a link')
    files.sort(key=lambda path:(path.stat().st_mtime_ns,path.name))
    for path in files[:-3]:
        path.unlink()


def native_present_checks(text):
    """Check presentation kinds/errors, not freshness of submitted content.

    Guest-tagged presents can repeat movie pictures without a level update.
    Freshness, update cadence and game speed require separate observations.
    """
    reports=[tuple(map(int,m)) for m in re.findall(
        r'\[PRESENT\] backend=\S+ total=(\d+) guest=(\d+) interpolated=(\d+) '
        r'other=(\d+) skipped=(\d+) failed=(\d+)',text)]
    return dict(guest_presents_observed=any(r[1]>0 for r in reports),
                interpolation_disabled=bool(reports) and
                    all(r[0]==r[1] and r[2]==r[3]==0 for r in reports),
                present_failures_absent=bool(reports) and all(r[5]==0 for r in reports))


def gameplay_checks(samples):
    """Check the replay's observable actions, independently of scene flags.

    These are bounded smoke checks, not proof of correct speed or full-game
    playability. A frozen opening sequence must not pass merely as Story.
    """
    active=[sample for sample in samples if 85 <= sample['seconds'] <= 115]
    positions=[sample['position'] for sample in active]
    moved=bool(positions) and any(
        sum((a-b)**2 for a,b in zip(position,positions[0])) > 1
        for position in positions[1:])
    clips={sample['hand']['clip'] for sample in active if sample.get('hand')}
    advanced=any(sample.get('hand') and
                 sample['hand']['time'][0] > sample['hand']['time'][1]
                 for sample in active)
    # This runner starts the first Story mission from the Test Lab fixture.
    # The 60-Hz reference receives weapon 14 during the opening handoff.
    # Punching in an unarmed state must not count as completing that handoff.
    equipped=bool(active) and all(sample.get('equipped',[None,None])[1]==14
                                  for sample in active)
    # Count both magazines plus shared reserve once per ammo type. A reload
    # moves ammunition between them and must not count as a fired shot.
    totals=[]
    for sample in active:
        by_type={}
        for slot in sample.get('ammo',[]):
            current=by_type.setdefault(slot['type'],dict(reserve=slot['reserve'],loaded=0))
            current['loaded']+=slot['loaded']
        if by_type: totals.append(sum(v['reserve']+v['loaded'] for v in by_type.values()))
    fired=bool(totals) and any(total<totals[0] for total in totals[1:])
    return dict(replayed_movement=moved,hand_animation_advanced=advanced,
                hand_animation_changed=len(clips)>1,opening_firearm_equipped=equipped,
                ammunition_consumed=fired)


def opening_firearm_check(samples):
    # Give the opening handoff a full minute after the first observed Story
    # player. Short menu/intro smoke tests cannot establish its completion.
    if not samples or samples[-1]['seconds']-samples[0]['seconds'] < 60:
        return {}
    # Keep this distinct from the stress-window requirement. A late handoff
    # must not overwrite a failed requirement to be equipped before firing.
    return dict(opening_firearm_equipped_at_end=samples[-1].get('equipped',[None,None])[1]==14)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fps', type=int, choices=(60, 120, 144, 180, 240), required=True)
    parser.add_argument('--native', action='store_true',
                        help='test native updates; omit for the original 60 FPS reference')
    parser.add_argument('--label', default='', help='optional evidence label (letters, digits and hyphens)')
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--stress', action='store_true',
                        help='replay walking, firing and camera turns after the intro')
    parser.add_argument('--pause-check', action='store_true')
    parser.add_argument('--no-intro-skip', action='store_true',
                        help='let the opening cinematic finish naturally for timing diagnosis')
    parser.add_argument('--trace-handoff', action='store_true',
                        help='bounded function and equipped-weapon tracing for the opening regression')
    parser.add_argument('--trace-camera', action='store_true',
                        help='sample live camera position/direction every half second, including the intro')
    parser.add_argument('--trace-entities', action='store_true',
                        help='sample the owned child entity list once a second (16 MiB limit)')
    parser.add_argument('--trace-effects', action='store_true',
                        help='sample owned child effect lists each observation (~0.5 seconds, 16 MiB limit)')
    parser.add_argument('--handoff-watch', choices=('none', 'equipped', 'loadout', 'npc-position'), default='equipped',
                        help='fixed diagnostic address to watch; none samples actors by live lookup only')
    parser.add_argument('--profile', action='store_true')
    parser.add_argument('--presentmon', type=Path,
                        help='optional PresentMon console EXE; captures only this test PID')
    parser.add_argument('--resources', action='store_true', help='sample child private memory and handle count every five seconds')
    parser.add_argument('--seconds', type=int, choices=range(60,601), default=90)
    parser.add_argument('--windowed', action='store_true')
    args = parser.parse_args()
    if not args.native and args.fps!=60:
        parser.error('The original timing reference is fixed at 60 FPS; use --native for higher rates.')
    if args.label and not re.fullmatch(r'[a-zA-Z0-9-]{1,32}',args.label):
        parser.error('--label must contain 1-32 letters, digits or hyphens')
    runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    entity_tracer_sha256=hashlib.sha256(
        (ROOT/'tools/native_entity_trace.py').read_bytes()).hexdigest()
    effect_tracer_sha256=hashlib.sha256(
        (ROOT/'tools/native_effect_trace.py').read_bytes()).hexdigest()
    if os.name != 'nt': parser.error('Windows only')
    assert hashlib.sha256((ROOT / 'private/game/default.xbe').read_bytes()).hexdigest() == XBE_SHA256
    run = ROOT / 'private/test-saves/high-refresh'
    env = prepare_run(ROOT, run)
    tag = ('native-' if args.native else '') + str(args.fps)
    if args.no_intro_skip: tag += '-natural-intro'
    if args.trace_handoff: tag += '-handoff'
    if args.trace_handoff and args.handoff_watch!='equipped': tag += '-'+args.handoff_watch
    if args.resources: tag += '-stability'
    if args.label: tag += '-'+args.label
    (run / 'runtime.ini').write_text(f'frame_target = 0\nframe_interp = 1\nframe_cap = 60\nfps_overlay = 1\nfullscreen = {int(not args.windowed)}\nvrr = 1\n')
    (run / 'display.ini').write_text('TSFP_DISPLAY_V1 1280 720\n')
    sequence = [(5000, 69), (10000, 69), (14000, 40), (18000, 69),
                (22000, 69), (26000, 69), (30000, 69), (34000, 69)]
    if not args.no_intro_skip: sequence.append((45000, 27))
    if args.pause_check: sequence += [(80000, 27), (83000, 27)]
    events = [(at, f'key {key} 1') for at, key in sequence]
    events += [(at+180, f'key {key} 0') for at, key in sequence]
    # Slow camera pan in active gameplay; no desktop input is synthesized.
    events += [(at, 'mouse 2 0') for at in range(65000, 76000, 100)]
    if args.stress:
        events += [(at, 'mouse 4 0') for at in range(85000, 115000, 750)]
        for start, stop, key in [(85000, 90000, 87), (92000, 97000, 68),
                                  (99000, 104000, 83), (106000, 111000, 65)]:
            events += [(start, f'key {key} 1'), (stop, f'key {key} 0')]
        for at in range(87000, 112000, 700):
            events += [(at, 'key 1 1'), (at+180, 'key 1 0')]
    assert len(events)<=256, 'PC replay hook accepts at most 256 events'
    (run / 'input.txt').write_text('\n'.join(f'{at} {event}' for at,event in sorted(events))+'\n')
    env.update(TSFP_PC_INPUT_REPLAY=str(run / 'input.txt'), RECOMP_WINDOW_BACKGROUND='1' if args.windowed else '0',
               RECOMP_PC_MOUSE='1', RECOMP_FPS='5', TSFP_PC_INPUT_STATE=str(run / 'state.txt'))
    if args.native:
        env.update(TSFP_NATIVE_FPS=str(args.fps), RECOMP_FRAME_TARGET='0', RECOMP_FRAME_INTERP='1',
                   TSFP_NATIVE_TRACE=str(run / (tag+'-frames.csv')))
    if args.trace_handoff and args.handoff_watch!='none':
        # This address belongs to the reproducible Test Lab first-mission
        # allocation. Record the actual player below so a shifted allocation
        # cannot silently be mistaken for an equipped-weapon trace.
        watch={'equipped':'0x8239bc3c','loadout':'0x82305114','npc-position':'0x8204d03c'}
        env.update(RECOMP_WATCH_VA=watch[args.handoff_watch],
                   RECOMP_TRACE_ONLY='sub_001BDAF0,sub_001B9DC0,sub_001B7D70,sub_001BA220,sub_001BA130,sub_001BFC00',
                   RECOMP_TRACE_ONLY_MAX='1200')
    (run / 'state.txt').write_text('0 0\n')
    (run / 'discord.ini').write_text('TSFP_DISCORD_V1 0 0\n')
    env['TSFP_DISCORD_CONFIG'] = str(run / 'discord.ini')
    if args.profile: env.update(RECOMP_WAIT_LOG='1',RECOMP_WAIT_PROFILE='1',
        RECOMP_SAMPLE='200',RECOMP_SAMPLE_REPORT='10',RECOMP_SAMPLE_DEPTH='8',
        RECOMP_SAMPLE_DUMP=str(run/(tag+'-sample.txt')))
    if args.capture:
        env.update(RECOMP_HLE_D3D8_DUMP=str(run / (tag+'-frame')),
                   RECOMP_HLE_D3D8_DUMP_EVERY=str(args.fps*5), RECOMP_HLE_D3D8_DUMP_KEEP_LAST='3')
    lock = run / 'run.lock'
    with lock.open('x') as marker: marker.write(str(os.getpid()))
    process = None
    profiler = None
    threads = []
    states=[];resources=[];native_samples=[];game_samples=[];camera_samples=[];completed=False
    entity_samples=[];entity_bytes=0;entity_trace_limited=False
    effect_samples=[];effect_bytes=0;effect_trace_limited=False
    terminated_by_runner=False
    def drain(pipe, path):
        with path.open('wb') as output:
            remaining = 4 * 1024 * 1024
            while chunk := pipe.read(4096):
                output.write(chunk[:remaining]); remaining = max(0, remaining-len(chunk))
    try:
        process = subprocess.Popen([str(ROOT / 'build/windows/Release/futureperfect_recomp.exe')],
            cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            creationflags=subprocess.CREATE_NO_WINDOW)
        print(f'Isolated {args.fps} FPS test PID {process.pid}, limit {args.seconds} seconds', flush=True)
        if args.presentmon:
            profiler=subprocess.Popen([str(args.presentmon.resolve()), '--process_id', str(process.pid),
                '--session_name', 'TSFP-'+str(process.pid), '--output_file', str(run/(tag+'-presentmon.csv')),
                '--no_console_stats', '--no_track_input', '--qpc_time_ms', '--timed', str(args.seconds),
                '--terminate_after_timed', '--terminate_on_proc_exit'],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            thread=threading.Thread(target=drain,args=(profiler.stdout,run/(tag+'-presentmon.log')),daemon=True)
            thread.start();threads.append(thread)
        for pipe, suffix in [(process.stdout, '.out'), (process.stderr, '.err')]:
            thread = threading.Thread(target=drain, args=(pipe, run / (tag+suffix)), daemon=True)
            thread.start(); threads.append(thread)
        # Observe only the documented scene/flags in our own disposable child.
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        class MemoryCounters(ctypes.Structure):
            _fields_=[('cb',ctypes.c_ulong),('PageFaultCount',ctypes.c_ulong)]+[
                (name,ctypes.c_size_t) for name in ('PeakWorkingSetSize','WorkingSetSize',
                'QuotaPeakPagedPoolUsage','QuotaPagedPoolUsage','QuotaPeakNonPagedPoolUsage',
                'QuotaNonPagedPoolUsage','PagefileUsage','PeakPagefileUsage','PrivateUsage')]
        kernel.K32GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.POINTER(MemoryCounters),ctypes.c_ulong]
        kernel.GetProcessHandleCount.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
        handle = kernel.OpenProcess(0x410, False, process.pid)
        assert handle, 'Cannot inspect owned test child'
        def word(address):
            value = ctypes.c_uint32()
            assert kernel.ReadProcessMemory(handle, address+0x10000, ctypes.byref(value), 4, None)
            return value.value
        def floats(address, count):
            values = (ctypes.c_float * count)()
            assert kernel.ReadProcessMemory(handle, address+0x10000, values, count*4, None)
            return list(values)
        def short(address):
            value=ctypes.c_int16()
            assert kernel.ReadProcessMemory(handle,address+0x10000,ctypes.byref(value),2,None)
            return value.value
        def read_bytes(address, size):
            values=(ctypes.c_ubyte*size)()
            if not kernel.ReadProcessMemory(handle,address+0x10000,values,size,None):
                return None
            return bytes(values)
        def ammunition(actor):
            # US sub_001B67E0 / sub_001BA220: equipped loadout+0x98,
            # weapon records at actor+0xA4 and +0x1D4, reserve at +0x61E.
            loadout=word(actor+0x98)
            if loadout>=256: return []
            table=word(0x4f9bac); slots=[]
            for offset,record in [(0x4fa134,0xa4),(0x4fa138,0x1d4)]:
                weapon=word(offset+loadout*0x3c)
                if weapon>=256: continue
                definition=table+weapon*0x29c
                ammo=word(definition+0x14)
                if not 0<=ammo<45: continue
                slots.append(dict(weapon=weapon,type=ammo,
                    capacity=word(definition+0x18),
                    loaded=short(actor+record+0x38+ammo*2),
                    reserve=short(actor+0x61e+ammo*2)))
            return slots
        def animation(entity):
            if not 0x80000000 <= entity < 0x84000000: return None
            model=word(entity+4)
            if not 0x80000000 <= model < 0x84000000: return None
            return dict(entity=entity,model=model,clip=word(model+0xd4),
                        time=floats(model+0xdc,2),speed=floats(model+0x108,1)[0],
                        cycle=word(model+0x124),flags=word(model+0x128),
                        next_clip=word(model+0x12c),next_time=floats(model+0x134,2),
                        blend=floats(model+0x198,1)[0],
                        position=floats(entity+0x64,3),
                        previous_position=floats(entity+0xec,3),
                        velocity=floats(entity+0xf8,3))
        def handoff_actor():
            # Read-only equivalent of US sub_00163730's RBS1 actor lookup.
            first,last=word(0x790950),word(0x74c37c)
            if not 0 <= first <= last <= 256: return None
            for index in range(first,last):
                actor=word(0x7a3580+index*4)
                if not 0x80000000 <= actor < 0x84000000: continue
                if word(actor+0x24)&8: continue
                definition=word(actor+0xbf0)
                if definition and word(definition+8)==0x52425331:
                    return dict(actor=actor,loadout=word(actor+0x94),
                                animation=animation(word(actor)))
            return None
        started=time.monotonic(); skipped=False; next_resource=0;next_entities=30
        try:
            while time.monotonic()-started < args.seconds:
                assert process.poll() is None, f'Game exited early: {process.returncode}'
                if profiler and profiler.poll() is not None:
                    assert profiler.returncode==0, 'PresentMon failed; see its bounded log'
                trace=run/(tag+'-presentmon.csv')
                assert not args.presentmon or not trace.exists() or trace.stat().st_size<16*1024*1024, 'PresentMon CSV exceeded 16 MiB'
                elapsed=time.monotonic()-started
                if args.capture:
                    recycle_captures(run,tag)
                if not guest_memory_ready(read_bytes, elapsed):
                    time.sleep(.05)
                    continue
                if args.trace_effects and not effect_trace_limited and word(0x79094c)==2:
                    import json
                    snapshot=snapshot_effects(read_bytes)
                    snapshot.update(seconds=round(elapsed,4),ticks=word(0x7de318),
                                    flags=word(0x7de458))
                    encoded=json.dumps(snapshot)
                    effect_bytes+=len(encoded)+2
                    if effect_bytes<=16*1024*1024:
                        effect_samples.append(snapshot)
                    else:
                        effect_trace_limited=True
                if args.trace_entities and elapsed>=next_entities and not entity_trace_limited:
                    import json
                    if word(0x79094c)==2:
                        snapshot=snapshot_entities(word,read_bytes)
                        snapshot.update(seconds=round(elapsed,4),ticks=word(0x7de318),
                                        flags=word(0x7de458))
                        encoded=json.dumps(snapshot)
                        entity_bytes+=len(encoded)+2
                        if entity_bytes<=16*1024*1024:
                            entity_samples.append(snapshot)
                        else:
                            entity_trace_limited=True
                    next_entities=elapsed+1
                if args.trace_camera and elapsed > 30:
                    camera=word(0x7b0cc0)
                    if 0x80000000 <= camera < 0x84000000:
                        camera_samples.append(dict(seconds=round(elapsed,4),camera=camera,
                            mode=word(camera+0x218),camera_flags=word(camera+0x21c),
                            scene=word(0x79094c),flags=word(0x7de458),
                            position=floats(camera+0x27c,3),
                            direction=floats(camera+0x334,3),
                            track=word(camera+0xfd0),
                            track_seconds=floats(camera+0xfd4,1)[0],
                            effect_seconds=floats(camera+0x398,1)[0],
                            kick=[floats(camera+0x3a4,1)[0],floats(camera+0x3ac,1)[0]],
                            kick_decay=[floats(camera+0x3a8,1)[0],floats(camera+0x3b0,1)[0]]))
                if elapsed > 50 and word(0x79094C)==2:
                    player=word(0x7b0c7c)
                    if 0x80000000 <= player < 0x84000000:
                        actor=word(word(player+0x14)+0x7c)
                        game_samples.append(dict(seconds=round(elapsed,4),player=player,
                            actor=actor,loadout=word(actor+0x94),
                            ammo=ammunition(actor),
                            position=floats(player+0xb4,3),look=floats(player+0x100,2),
                            equipped=[word(player+0x1b4),word(player+0x58c)],
                            weapon_action=[word(player+0x1c8),word(player+0x5a0)],
                            body=animation(word(player+0x14)),
                            hand=animation(word(player+0x5b0)),
                            weapon=animation(word(player+0xe84)),
                            handoff_actor=handoff_actor() if args.trace_handoff else None))
                        if args.trace_handoff:
                            if args.handoff_watch=='equipped':
                                assert player+0x58c==0x8239bc3c, 'Handoff watch address no longer matches player allocation'
                            if args.handoff_watch=='loadout':
                                assert actor+0x94==0x82305114, 'Loadout watch address no longer matches actor allocation'
                            if args.handoff_watch=='npc-position':
                                observed=game_samples[-1]['handoff_actor']
                                assert observed and observed['animation']['entity']+0x64==0x8204d03c, 'NPC watch address no longer matches actor allocation'
                if args.native and elapsed > 5:
                    native_samples.append(dict(seconds=round(elapsed,4),
                        frames=word(0x7497B8),ticks=word(0x7497B0),
                        raw_delta_bits=word(0x4E7948),game_delta_bits=word(0x4E794C),
                        scene=word(0x79094C),flags=word(0x7DE458)))
                if args.resources and elapsed>=next_resource:
                    memory=MemoryCounters();memory.cb=ctypes.sizeof(memory);count=ctypes.c_ulong()
                    assert kernel.K32GetProcessMemoryInfo(handle,ctypes.byref(memory),memory.cb)
                    assert kernel.GetProcessHandleCount(handle,ctypes.byref(count))
                    resources.append(dict(seconds=round(elapsed,2),private_bytes=memory.PrivateUsage,
                        working_set_bytes=memory.WorkingSetSize,handles=count.value))
                    next_resource=elapsed+5
                if elapsed > 50:
                    scene, flags=word(0x79094C),word(0x7DE458)
                    if flags & 0x20000000 and not skipped and not args.no_intro_skip:
                        (run / 'state.txt').write_text('27 1\n');time.sleep(.2)
                        (run / 'state.txt').write_text('0 0\n');skipped=True
                    pause_pointer=word(0x761C48)
                    paused=word(pause_pointer)&1 if pause_pointer else 0
                    states.append((round(elapsed,2),scene,flags,paused))
                time.sleep(.5)
            completed=True
        finally:
            kernel.CloseHandle(handle)
        import json
        (run / (tag+'-states.json')).write_text(json.dumps(states))
        if args.native:
            (run / (tag+'-timing.json')).write_text(json.dumps(native_samples,indent=2))
        if args.resources:(run/(tag+'-resources.json')).write_text(json.dumps(resources,indent=2))
    finally:
        if process is not None and process.poll() is None:
            terminated_by_runner=True
            process.terminate(); process.wait(timeout=10)
        if profiler is not None and profiler.poll() is None:
            try: profiler.wait(timeout=10)
            except subprocess.TimeoutExpired: profiler.terminate();profiler.wait(timeout=10)
        for thread in threads: thread.join(10)
        # Persist partial evidence too. An interrupted run must not leave an
        # older successful run's timing file looking like current evidence.
        import json
        text=(run/(tag+'.err')).read_text(errors='replace')
        checks=dict(no_crash='[CRASH]' not in text,
                    story=any(scene==2 and not flags&0x20000000 for _,scene,flags,_ in states))
        if args.native:
            checks.update(native_hook='[NATIVE-TIMING]' in text,**native_present_checks(text))
            (run/(tag+'-timing.json')).write_text(json.dumps(native_samples,indent=2))
        if args.pause_check:
            checks.update(paused=any(80.5<t<82.5 and pause for t,_,_,pause in states),
                          resumed=any(t>84 and not pause for t,_,_,pause in states))
        if args.stress and args.seconds >= 116:
            checks.update(gameplay_checks(game_samples))
        checks.update(opening_firearm_check(game_samples))
        (run/(tag+'-validation.json')).write_text(json.dumps(dict(
            completed=completed,exit_code=process.returncode if process else None,
            terminated_by_runner=terminated_by_runner,
            passed=completed and all(checks.values()),checks=checks,
            executable_sha256=hashlib.sha256((ROOT/'build/windows/Release/futureperfect_recomp.exe').read_bytes()).hexdigest(),
            runner_sha256=runner_sha256,native_fps=args.fps if args.native else 0,
            presentation_fps=args.fps,seconds=args.seconds,
            capture_enabled=args.capture,profiling_enabled=args.profile,
            resource_sampling=args.resources,presentmon_enabled=bool(args.presentmon),
            intro_skip_requested=not args.no_intro_skip,
            camera_trace=args.trace_camera,camera_samples=len(camera_samples),
            entity_trace=args.trace_entities,entity_samples=len(entity_samples),
            entity_tracer_sha256=entity_tracer_sha256,
            entity_trace_limited=entity_trace_limited,
            effect_trace=args.trace_effects,effect_samples=len(effect_samples),
            effect_tracer_sha256=effect_tracer_sha256,
            effect_trace_limited=effect_trace_limited,
            handoff_trace=args.trace_handoff,
            handoff_watch=args.handoff_watch if args.trace_handoff else None,
            samples=len(native_samples),gameplay_samples=len(game_samples)),indent=2))
        (run/(tag+'-states.json')).write_text(json.dumps(states))
        (run/(tag+'-gameplay.json')).write_text(json.dumps(game_samples,indent=2))
        if args.trace_camera:
            (run/(tag+'-camera.json')).write_text(json.dumps(camera_samples,indent=2))
        if args.trace_entities:
            (run/(tag+'-entities.json')).write_text(json.dumps(entity_samples))
        if args.trace_effects:
            (run/(tag+'-effects.json')).write_text(json.dumps(effect_samples))
        if args.capture:
            recycle_captures(run,tag)
        lock.unlink(); recycle_disks(ROOT, run)
    check_fixture(ROOT)
    if args.presentmon:
        trace=run/(tag+'-presentmon.csv')
        print('PresentMon:', str(trace) if trace.exists() and trace.stat().st_size else 'No frame events captured; GPU/display timing unavailable from ETW.',flush=True)
    log = (run / (tag+'.err')).read_text(errors='replace')
    assert '[CRASH]' not in log
    if args.native:
        assert '[NATIVE-TIMING]' in log, 'Native timing hook was not executed'
        assert all(native_present_checks(log).values()), 'Native presentation kind/error checks failed'
        print('Native engine timing:', run / (tag+'-timing.json'),flush=True)
    if args.stress and args.seconds >= 116:
        observed=gameplay_checks(game_samples)
        assert all(observed.values()), f'Replayed gameplay did not progress: {observed}'
    handoff=opening_firearm_check(game_samples)
    assert all(handoff.values()), f'Opening firearm handoff did not complete: {handoff}'
    assert any(scene==2 and not flags & 0x20000000 for _,scene,flags,_ in states), 'Did not reach Story gameplay'
    if args.pause_check:
        assert any(80.5<t<82.5 and pause for t,_,_,pause in states), 'Pause not observed'
        assert any(t>84 and not pause for t,_,_,pause in states), 'Resume not observed'
    print('\n'.join(line for line in log.splitlines() if '[INTERP]' in line or '[FPS]' in line or '[PC-DISPLAY] Camera' in line))
    print('Evidence:', run / (tag+'-validation.json'))


if __name__ == '__main__': main()
