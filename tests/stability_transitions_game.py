"""Bounded, operator-steered integration session on an isolated Test Lab copy.

Append JSON lines to commands.jsonl: keys (virtual-key numbers), label, gap, hold,
window (toggle/focus), restart, or stop. Inputs go to the game's replay hook;
window messages target only the child. Captures are renderer output, not scanout.
The entire session is limited to ten minutes, including process restarts.
"""
import argparse
import ctypes as c
import csv
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import threading
import time
from PIL import Image
import pc_settings_game as ui
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from testing_profile import prepare_run,recycle_disks,check_fixture,XBE_SHA256,signature,BODY_SIZE

def weapon_set_metrics(run):
    """Check settled five-second guest-swap windows, excluding transitions.

    Captures still need review: counters alone cannot identify a menu screen.
    """
    report=json.loads((run/'report.json').read_text())
    samples=[(float(t),float(fps)) for t,fps in re.findall(
        r'\[FPS\] t=\s*([\d.]+)s\s+([\d.]+) fps',(run/'run0.err').read_text(errors='replace'))]
    result={}
    for item in report:
        label=item.get('command',{}).get('label')
        if label not in ('before-weapons','weapons','weapons-selection','after-weapons'):continue
        end=item['seconds']; wait=item['command'].get('wait',2.5)
        # A window ends at t and spans the previous five seconds. Leave two
        # seconds after input for assets/transitions before starting a window.
        values=[fps for t,fps in samples if end-wait+7<=t<=end]
        result[label]=values
    (run/'weapon-set-metrics.json').write_text(json.dumps(result,indent=2)+'\n')
    assert len(result)==4 and all(len(v)>=1 and min(v)>=50 for v in result.values()),result
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-name',default='stability-transitions')
    parser.add_argument('--native-fps',type=int,choices=(0,60,120,144),
        help='0 selects Original60; 60, 120 and 144 select the native fractional clock')
    parser.add_argument('--manual-start',action='store_true')
    parser.add_argument('--lan-role',choices=('host','client'),help='Use isolated loopback peers 127.0.0.2/3 for a bounded LAN session')
    parser.add_argument('--sample-network-ms',type=int,choices=range(2,21),help='Read both actor positions in a background sampler; records observation timing, not rendered frames')
    parser.add_argument('--trace-network-poses',action='store_true',help='Record completed gameplay-update positions at the shared clock boundary')
    parser.add_argument('--no-captures',action='store_true',help='Disable renderer dumps and label screenshots for timing measurements')
    parser.add_argument('--profile-presents',action='store_true',help='Record bounded host Present intervals for original or native timing')
    parser.add_argument('--watch-hud',action='store_true',help='Observe bounded HUD message slots and their elapsed time')
    parser.add_argument('--trace-surface-switches',action='store_true',help='Record 90 positive-time images per FPS segment, at most 360 total')
    parser.add_argument('--live-surfaces',action='store_true',help='Switch experimental water precision with the FPS preset; requires all three surface experiments')
    parser.add_argument('--bias-surfaces',action='store_true',help='Correct experimental water centering; requires float and fractional surfaces')
    parser.add_argument('--float-surfaces',action='store_true',help='Use experimental half-float feedback intermediates independently of timestep conversion')
    parser.add_argument('--fractional-surfaces',action='store_true',help='Enable the experimental procedural feedback conversion for comparison')
    parser.add_argument('--trace-surface-images',action='store_true',help='Read back up to 360 Scotland procedural images; synchronous GPU diagnostics, not a pacing test')
    parser.add_argument('--watch-surfaces',action='store_true',help='Observe the five procedural texture resources; asynchronous metadata, not image or update-cadence proof')
    parser.add_argument('--watch-input',action='store_true',help='Trace changes to player-one input suppression in the bounded log')
    parser.add_argument('--trace-vehicle-contact',action='store_true',help='Trace up to 64 entries each for actor/vehicle contact, impulse selection and suspension response')
    parser.add_argument('--minimized',action='store_true',help='Keep the isolated test window minimized')
    parser.add_argument('--seconds',type=int,choices=range(1,601),default=600)
    parser.add_argument('--scenario',choices=['weapon-set','surface-feedback','surface-feedback-gpu'])
    args=parser.parse_args()
    if args.scenario in ('surface-feedback','surface-feedback-gpu') and (args.live_surfaces or args.bias_surfaces or args.float_surfaces or args.fractional_surfaces):
        parser.error('surface-feedback captures the original feedback; disable the older surface experiments')
    if args.watch_input and args.trace_vehicle_contact:
        parser.error('--watch-input and --trace-vehicle-contact select different bounded function traces; run them separately')
    if args.trace_surface_switches and not args.trace_surface_images:
        parser.error("--trace-surface-switches requires --trace-surface-images")
    if args.live_surfaces and not (args.bias_surfaces and args.float_surfaces and args.fractional_surfaces):
        parser.error("--live-surfaces requires --bias-surfaces, --float-surfaces and --fractional-surfaces")
    if args.bias_surfaces and not (args.float_surfaces and args.fractional_surfaces):
        parser.error("--bias-surfaces requires --float-surfaces and --fractional-surfaces")
    assert not args.sample_network_ms or args.lan_role
    assert not args.trace_network_poses or args.lan_role
    assert args.run_name.replace('-','').isalnum() and len(args.run_name)<48
    assert os.name=='nt'
    assert hashlib.sha256((ROOT/'private/game/default.xbe').read_bytes()).hexdigest()==XBE_SHA256
    if args.lan_role:
        subprocess.run([sys.executable,str(ROOT/'tools/network-identity.py'),'--verify',
                        '--executable',str(ROOT/'build/windows/Release/futureperfect_recomp.exe')],check=True)
    run=ROOT/'private/test-saves'/args.run_name;env=prepare_run(ROOT,run)
    if args.lan_role:
        env.update(TSFP_NETWORK='1',
                   TSFP_NET_BIND='127.0.0.2' if args.lan_role=='host' else '127.0.0.3',
                   TSFP_NET_PEERS='127.0.0.3' if args.lan_role=='host' else '127.0.0.2')
    # This owned, unlocked test directory starts from the requested CLI rate.
    # In-session restarts retain any preference saved by the menu under test.
    (run/'pc-frame-rate.ini').unlink(missing_ok=True)
    (run/'runtime.ini').write_text('frame_target = 144\nframe_cap = 60\nframe_interp = 1\nfullscreen = 0\nvrr = 1\n')
    (run/'display.ini').write_text('TSFP_DISPLAY_V1 1280 720\n')
    (run/'discord.ini').write_text('TSFP_DISCORD_V1 0 0\n')
    commands=run/'commands.jsonl';commands.write_text('')
    if args.scenario=='weapon-set':
        route=[dict(label='main-menu',wait=24),
            dict(keys=[40,69,69,69,69,38,38],gap=1.3,label='before-weapons',wait=12),
            dict(keys=[69],label='weapons',wait=20),
            dict(keys=[40,40,40],label='weapons-selection',wait=10),
            dict(keys=[70],label='after-weapons',wait=15),dict(stop=True)]
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
    if args.scenario in ('surface-feedback','surface-feedback-gpu'):
        route=json.loads((ROOT/'tests/fixtures/procedural-surface-commands.json').read_text())['scotland']
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
        env['TSFP_SURFACE_CAPTURE']=str(run/'feedback.bin')
        (run/'feedback.bin').unlink(missing_ok=True)
        (run/'feedback.bin.gpu').unlink(missing_ok=True)
        if args.scenario=='surface-feedback-gpu':env['TSFP_SURFACE_GPU_CAPTURE']='1'
        (run/'capture-provenance.json').write_text(json.dumps(dict(
            executable_sha256=hashlib.sha256((ROOT/'build/windows/Release/futureperfect_recomp.exe').read_bytes()).hexdigest(),
            xbe_sha256=XBE_SHA256,native_fps=args.native_fps,
            scope='Synchronous completed GPU feedback capture, not a performance measurement'),indent=2))
    state=run/'input-state.txt';state.write_text('0 0\n')
    initial=[(5000,69),(10000,69),(14000,40),(18000,69)]
    replay=run/'input.txt';replay.write_text('\n'.join(f'{at+d} key {k} {v}' for at,k in initial for d,v in [(0,1),(180,0)])+'\n')
    env.update(TSFP_PC_INPUT_REPLAY=str(replay),TSFP_PC_INPUT_STATE=str(state),
        RECOMP_WINDOW_BACKGROUND='1',RECOMP_PC_MOUSE='1',TSFP_DISCORD_CONFIG=str(run/'discord.ini'),
        RECOMP_HLE_D3D8_DUMP=str(run/'frame'),RECOMP_HLE_D3D8_DUMP_EVERY='30',
        RECOMP_HLE_D3D8_DUMP_KEEP_LAST='3',RECOMP_FPS='5',RECOMP_INTERP_DEBUG='1')
    if args.no_captures:
        for key in ('RECOMP_HLE_D3D8_DUMP','RECOMP_HLE_D3D8_DUMP_EVERY','RECOMP_HLE_D3D8_DUMP_KEEP_LAST'):
            env.pop(key,None)
    if args.native_fps is not None:
        env.update(TSFP_NATIVE_FPS=str(args.native_fps),RECOMP_FRAME_TARGET='0',RECOMP_FRAME_INTERP='1',
                   TSFP_NATIVE_TRACE=str(run/'native-frames.csv'))
    env['TSFP_SURFACE_TRACE_SWITCHES']='1' if args.trace_surface_switches else '0'
    env['TSFP_SURFACE_LIVE_TEST']='1' if args.live_surfaces else '0'
    env['TSFP_SURFACE_BIAS_TEST']='1' if args.bias_surfaces else '0'
    env['TSFP_SURFACE_FLOAT_TEST']='1' if args.float_surfaces else '0'
    env['TSFP_SURFACE_FRACTIONAL_TEST']='1' if args.fractional_surfaces else '0'
    if args.watch_input:
        env.update(RECOMP_WATCH_VA='0x6B9408', RECOMP_TRACE_ONLY='sub_00042830,sub_00042870,sub_000428D0', RECOMP_TRACE_ONLY_MAX='32')
    if args.trace_vehicle_contact:
        env.update(RECOMP_TRACE_ONLY='sub_0017CAE0,sub_0017BC90,sub_00174EC0',RECOMP_TRACE_ONLY_MAX='64')
    if args.manual_start: replay.write_text('')
    def profiles():
        result={}
        for profile in (run/'saves/UserData/45410066').glob('*/tsftrpft'):
            raw=profile.read_bytes()
            assert len(raw)==BODY_SIZE+20 and signature(raw[:BODY_SIZE])==raw[BODY_SIZE:]
            result[profile.parent.name]=hashlib.sha256(raw).hexdigest()
        assert result
        return result
    original=profiles();report=[];samples=[];threads=[];process=None;handle=None
    motion_stop=threading.Event();motion_thread=None;motion_errors=[]
    kernel=c.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.restype=c.c_void_p
    kernel.ReadProcessMemory.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.c_void_p]
    kernel.CloseHandle.argtypes=[c.c_void_p]
    user=ui.user;user.GetForegroundWindow.restype=c.c_void_p
    user.SetForegroundWindow.argtypes=[c.c_void_p]
    def word(address):
        value=c.c_uint32()
        if not kernel.ReadProcessMemory(handle,address+0x10000,c.byref(value),4,None):return None
        return value.value
    def floats(address,count):
        values=(c.c_float*count)()
        if not kernel.ReadProcessMemory(handle,address+0x10000,values,count*4,None):return None
        return list(values)
    def short(address):
        value=c.c_int16()
        if not kernel.ReadProcessMemory(handle,address+0x10000,c.byref(value),2,None):return None
        return value.value
    def player_snapshot():
        player=word(0x7b0c7c)
        if player is None or not 0x80000000<=player<0x84000000:return None
        entity=word(player+0x14)
        actor=word(entity+0x7c) if entity else None
        result=dict(player=player,position=floats(player+0xb4,3),look=floats(player+0x100,2),
                    hit_response=floats(player+0xf4,3),
                    equipped=[word(player+0x1b4),word(player+0x58c)],ammo=[])
        # Original actor damage updates entity+0x1cc (native_sniper_damage.c).
        # Read-only observation helps distinguish route/combat deaths from stalls.
        if entity is not None and 0x80000000<=entity<0x84000000:
            health=floats(entity+0x1cc,1)
            if health is not None:result['health']=health[0]
        if actor is None or not 0x80000000<=actor<0x84000000:return result
        loadout=word(actor+0x98);table=word(0x4f9bac)
        if loadout is None or not 0<=loadout<256 or not table:return result
        for base,record in [(0x4fa134,0xa4),(0x4fa138,0x1d4)]:
            weapon=word(base+loadout*0x3c)
            if weapon is None or not 0<=weapon<256:continue
            definition=table+weapon*0x29c;ammo=word(definition+0x14)
            if ammo is None or not 0<=ammo<45:continue
            result['ammo'].append(dict(weapon=weapon,type=ammo,capacity=word(definition+0x18),
                loaded=short(actor+record+0x38+ammo*2),reserve=short(actor+0x61e+ammo*2)))
        return result
    def hud_snapshot():
        messages=[]
        for slot in range(16):
            address=0x7a6160+0x38+slot*0x16c
            kind=word(address)
            if kind is None or not 1<=kind<=13:continue
            text=c.create_string_buffer(320)
            if not kernel.ReadProcessMemory(handle,address+4+0x10000,text,320,None):continue
            messages.append(dict(slot=slot,kind=kind,text=text.value.decode('latin-1'),
                timing=floats(address+0x144,5)))
        return messages
    def surface_snapshot():
        # Original allocator 0x127400 has exactly five 0xfc-byte slots. Do not
        # trust live pointers/counts during loading, or infer GPU execution from
        # CPU cache stamps. These asynchronous reads can cross an update.
        pool=word(0x742350);count=word(0x7421a0)
        result=dict(pool=pool,allocated=count,frame=word(0x7497b8),
                    float_delta=floats(0x4e794c,1),resources=[])
        if pool is None or not 0x80000000<=pool<=0x84000000-5*0xfc:
            return result
        for slot in range(5):
            address=pool+slot*0xfc;kind=word(address)
            if kind is None or kind==0xffffffff:continue
            table=word(address+0x20)
            result['resources'].append(dict(slot=slot,kind=kind,
                stamps=[word(address+4+i*4) for i in range(6)],
                parameters=word(address+0x1c),image_table=table,
                images=[word(table+i*4) for i in range(4)]
                    if table is not None and 0x12000<=table<=0x84000000-16 else None,
                indices=[word(address+0x24+i*4) for i in range(4)],
                disturbances=word(address+0xf4)))
        result['pool_after']=word(0x742350)
        return result
    def network_snapshot():
        actors=[];pool=word(0x7356d8);roster=word(0x7de400) or 0
        humans,bots=roster&255,(roster>>16)&255
        if pool is not None and 0x80000000<=pool<0x84000000:
            for slot in range(min(16,humans+bots)):
                entity=word(pool+slot*0xea0)
                if entity is not None and 0x80000000<=entity<0x84000000:
                    actors.append(dict(slot=slot,entity=entity,position=floats(entity+0x64,3),
                                       health=floats(entity+0x1cc,1)))
        return dict(state=word(0x76b1a4),role=word(0x76b140),human_slots=humans,bot_slots=bots,
                    peers=[i for i in range(16) if word(0x666040+i*4)],actors=actors)
    def snapshot():
        pointer=word(0x761C48)
        return dict(pid=process.pid,scene=word(0x79094C),flags=word(0x7DE458),
            paused=word(pointer)&1 if pointer else 0,mission=word(0x78AD48),
            held=word(0x6B93DC),pressed=word(0x6B93E0),suppression=word(0x6B9408),
            raw_ticks=word(0x4E7940),game_ticks=word(0x4E7944),level_ticks=word(0x7DE318),
            match_state=word(0x7DE30C),countdown=word(0x4D2878),countdown_ticks=word(0x4D287C),
            player=player_snapshot(),**(dict(hud=hud_snapshot()) if args.watch_hud else {}),
            **(dict(network=network_snapshot()) if args.lan_role else {}),
            **(dict(surfaces=surface_snapshot()) if args.watch_surfaces else {}))
    def drain(pipe,path):
        with path.open('wb') as out:
            left=4*1024*1024
            while chunk:=pipe.read(4096):out.write(chunk[:left]);left=max(0,left-len(chunk))
    def sample_motion(path):
        # Independent of command waits. Timestamp the whole observation because
        # RPM reads are not an atomic game-frame snapshot. Each XYZ is one read.
        try:
            with path.open('w',newline='') as out:
                writer=csv.writer(out)
                writer.writerow(['begin','end','scene','network','match','level_ticks',
                                 'x0','y0','z0','x1','y1','z1'])
                while not motion_stop.is_set():
                    began=time.perf_counter()
                    scene,network,match=word(0x79094c),word(0x76b1a4),word(0x7de30c)
                    if network==15 and match==7:
                        pool=word(0x7356d8);positions=[]
                        for slot in range(2):
                            entity=word(pool+slot*0xea0) if pool and 0x80000000<=pool<0x84000000 else None
                            xyz=floats(entity+0x64,3) if entity and 0x80000000<=entity<0x84000000 else None
                            positions.extend(xyz or [None]*3)
                        tick=word(0x7de318)
                        writer.writerow([began,time.perf_counter(),scene,network,match,tick,*positions])
                        if out.tell()>32*1024*1024:raise RuntimeError('Network observation exceeded 32 MiB bound')
                    # CPython uses a high-resolution waitable timer for sleep
                    # on Windows; Event.wait can round a 2 ms wait to ~16 ms.
                    time.sleep(max(0,args.sample_network_ms/1000-(time.perf_counter()-began)))
        except Exception as exc:motion_errors.append(repr(exc))
    def launch(index):
        nonlocal process,handle,window,motion_thread
        state.write_text('0 0\n')
        if args.trace_surface_images:
            env['TSFP_SURFACE_TRACE']=str(run/f'run{index}-surface.csv')
        if args.profile_presents:
            env['TSFP_FRAME_PROFILE']=str(run/f'run{index}-presents.csv')
        if args.trace_network_poses:
            env['TSFP_NATIVE_POSE_TRACE']=str(run/f'run{index}-network-poses.csv')
        process=subprocess.Popen([str(ROOT/'build/windows/Release/futureperfect_recomp.exe')],cwd=ROOT,env=env,
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
        handle=kernel.OpenProcess(0x410,False,process.pid);assert handle
        for pipe,ext in [(process.stdout,'out'),(process.stderr,'err')]:
            t=threading.Thread(target=drain,args=(pipe,run/f'run{index}.{ext}'),daemon=True);t.start();threads.append(t)
        window=ui.wait_window(process.pid,'TimeSplitters FP')
        if args.sample_network_ms:
            motion_stop.clear()
            motion_thread=threading.Thread(target=sample_motion,args=(run/f'run{index}-network-motion.csv',),daemon=True)
            motion_thread.start()
        if args.minimized:
            user.ShowWindow.argtypes=[c.c_void_p,c.c_int]
            user.ShowWindow(window,6)
        print('Child',index,process.pid,flush=True)
    def close():
        nonlocal handle,motion_thread
        motion_stop.set()
        if motion_thread:
            motion_thread.join(5)
            assert not motion_thread.is_alive(),'Motion sampler did not stop before handle close'
            motion_thread=None
        if process and process.poll() is None:
            user.PostMessageW(window,0x10,0,0)
            try:process.wait(10)
            except subprocess.TimeoutExpired:process.terminate();process.wait(10)
        if handle:kernel.CloseHandle(handle);handle=None
        for t in threads:t.join(5)
        threads.clear()
    def capture(label):
        if args.no_captures:return
        assert label.replace('-','').isalnum() and len(label)<48
        assert (run/(label+'.png')).exists() or len(list(run.glob('*.png')))<48
        for _ in range(20):
            # The renderer rotates dumps while this process enumerates them.
            frames=[]
            for path in run.glob('frame*.bmp'):
                try: frames.append((path.stat().st_mtime_ns,path))
                except FileNotFoundError: continue
            frames.sort()
            if frames:
                try:
                    with Image.open(frames[-1][1]) as im:im.save(run/(label+'.png'))
                    return
                except OSError:pass
            time.sleep(.1)
        raise AssertionError('No renderer capture')
    window=None;index=0;seen=0;start=time.perf_counter();next_sample=0
    lock=run/'run.lock'
    (run/'provenance.json').write_text(json.dumps(dict(
        executable_sha256=hashlib.sha256((ROOT/'build/windows/Release/futureperfect_recomp.exe').read_bytes()).hexdigest(),
        runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        native_fps=args.native_fps or 0,seconds=args.seconds,captures=not args.no_captures,watch_hud=args.watch_hud,watch_surfaces=args.watch_surfaces,trace_surface_images=args.trace_surface_images,fractional_surfaces=args.fractional_surfaces,float_surfaces=args.float_surfaces,bias_surfaces=args.bias_surfaces,live_surfaces=args.live_surfaces,trace_surface_switches=args.trace_surface_switches,minimized=args.minimized,profile_presents=args.profile_presents,lan_role=args.lan_role,sample_network_ms=args.sample_network_ms,trace_network_poses=args.trace_network_poses,observation_clock=time.get_clock_info('perf_counter').implementation,clock_resolution=time.get_clock_info('perf_counter').resolution,started_monotonic=start),indent=2))
    with lock.open('x') as f:f.write(str(os.getpid()))
    try:
        launch(index)
        while time.perf_counter()-start<args.seconds:
            assert process.poll() is None,'Unexpected game exit'
            elapsed=time.perf_counter()-start
            if elapsed>=next_sample:
                item=dict(seconds=round(elapsed,2),**snapshot());samples.append(item)
                (run/'current.json').write_text(json.dumps(item));next_sample=elapsed+1
            lines=commands.read_text().splitlines();assert commands.stat().st_size<65536
            if seen<len(lines):
                command=json.loads(lines[seen]);seen+=1
                assert len(command.get('keys',[]))<=16 and 0.1<=command.get('gap',.7)<=4
                hold=command.get('hold',.18)
                assert .05<=hold<=3
                assert 0<=command.get('wait',2.5)<=30
                motion=command.get('mouse')
                if motion is not None:
                    assert isinstance(motion,list) and len(motion)==2
                    assert all(type(value) is int and -4096<=value<=4096 for value in motion)
                budget=len(command.get('keys',[]))*(hold+command.get('gap',.7))+command.get('wait',2.5)+(.1 if motion is not None else 0)
                if elapsed+budget+(22 if command.get('restart') else 0)>args.seconds:break
                if command.get('stop'):break
                if command.get('restart'):
                    before=profiles();close()
                    report.append(dict(restart_saved_profiles=profiles(),before_exit_profiles=before,exit_code=process.returncode))
                    assert process.returncode==0,'Game did not exit cleanly'
                    index+=1;assert index<=3;launch(index)
                if motion is not None:
                    state.write_text(f'0 0 {seen} {motion[0]} {motion[1]}\n')
                    time.sleep(.1)
                hold_samples=[];key_events=[]
                for key in command.get('keys',[]):
                    down_ns=time.perf_counter_ns()
                    state.write_text(f'{key} 1\n')
                    held_at=time.perf_counter()
                    if command.get('sample_hold'):
                        while time.perf_counter()-held_at<hold:
                            hold_samples.append(dict(seconds=round(time.perf_counter()-start,6),
                                held_seconds=round(time.perf_counter()-held_at,6),key=key,
                                **snapshot()))
                            time.sleep(min(.05,max(0,hold-(time.perf_counter()-held_at))))
                    else:time.sleep(hold)
                    state.write_text('0 0\n')
                    key_events.append(dict(key=key,down_ns=down_ns,up_ns=time.perf_counter_ns()))
                    time.sleep(command.get('gap',.7))
                if command.get('window')=='toggle':user.PostMessageW(window,0x104,13,1<<29)
                if command.get('window')=='focus':
                    user.ShowWindow(window,6);time.sleep(1)
                    lost=user.GetForegroundWindow()!=window
                    user.ShowWindow(window,9);user.SetForegroundWindow(window);time.sleep(1)
                    report.append(dict(focus_lost=lost,focus_restored=user.GetForegroundWindow()==window))
                time.sleep(command.get('wait',2.5))
                item=dict(seconds=round(time.perf_counter()-start,3),observed_ns=time.perf_counter_ns(),
                    command=command,key_events=key_events,**snapshot(),fullscreen=not bool(user.GetWindowLongW(window,-16)&0x00C00000))
                if hold_samples:item['hold_samples']=hold_samples
                if 'label' in command:capture(command['label'])
                report.append(item);(run/'report.json').write_text(json.dumps(report,indent=2))
                print(json.dumps(item),flush=True)
            time.sleep(.1)
    finally:
        close();lock.unlink()
        (run/'states.json').write_text(json.dumps(samples))
        (run/'report.json').write_text(json.dumps(report,indent=2))
        recycle_disks(ROOT,run);check_fixture(ROOT)
        saved=profiles()
        (run/'saved-profiles.json').write_text(json.dumps(saved,indent=2))
        print('Profiles changed:',saved!=original,'Signatures valid; fixture preserved',flush=True)
        for frame in run.glob('frame*.bmp'):
            assert not frame.is_symlink() and frame.resolve().parent==run.resolve();frame.unlink()
    for log in run.glob('run*.err'):
        text=log.read_text(errors='replace')
        assert '[SURFACE-TRACE] Failed:' not in text and '[SURFACE-TRACE] Cannot open' not in text,log
        assert '[SURFACE-FLOAT] Unsupported' not in text,log
        assert '[SURFACE-CAPTURE] Stopped:' not in text,log
        assert '[CRASH]' not in text and '[ICALL] unresolved' not in text,log
    assert process.returncode==0,'Final game exit was not clean'
    assert not motion_errors,motion_errors
    if args.scenario=='weapon-set':print('Weapon-set settled FPS:',weapon_set_metrics(run),flush=True)
    if args.scenario in ('surface-feedback','surface-feedback-gpu'):
        captured=(run/'feedback.bin').read_bytes()
        assert captured[:8]==b'TSWATER1' and len(captured)==8+16*(92+3*128*128*4)
        print('Captured 16 completed GPU feedback triplets; shader comparison still required',flush=True)
        if args.scenario=='surface-feedback-gpu':
            computed=(run/'feedback.bin.gpu').read_bytes()
            assert computed[:8]==b'TSWGPU1\0' and len(computed)==8+16*(92+2*128*128*16)
            print('Captured 16 live GPU solver midpoint/endpoint pairs; independent comparison still required',flush=True)
    print('PASS: bounded session completed; inspect report and captures for scenario coverage',flush=True)

if __name__=='__main__':main()
