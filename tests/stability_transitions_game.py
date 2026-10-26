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
from testing_profile import prepare_run,recycle_disks,check_fixture,XBE_SHA256,signature,BODY_SIZE,publish_input_state
from native_effect_trace import snapshot_effects

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
    parser.add_argument('--native-fps',type=int,choices=(0,60,120,144,180,240),
        help='0 selects Original60; positive rates select the native fractional clock')
    parser.add_argument('--manual-start',action='store_true')
    parser.add_argument('--fullscreen',action='store_true',help='Start this isolated session borderless on its monitor; actual client size is reported separately')
    parser.add_argument('--display-size',type=int,nargs=2,default=(1280,720),metavar=('WIDTH','HEIGHT'),help='Isolated render resolution; actual window state is reported separately')
    parser.add_argument('--fov',type=int,choices=range(40,101),default=55)
    parser.add_argument('--no-native-trace',action='store_true',help='Disable per-update CSV when measuring capture-free pacing')
    parser.add_argument('--lan-role',choices=('host','client'),help='Use isolated loopback peers 127.0.0.2/3 for a bounded LAN session')
    parser.add_argument('--sample-network-ms',type=int,choices=range(2,21),help='Read both actor positions in a background sampler; records observation timing, not rendered frames')
    parser.add_argument('--trace-network-poses',action='store_true',help='Record completed gameplay-update positions at the shared clock boundary')
    parser.add_argument('--trace-local-poses',action='store_true',help='Record bounded completed local aim/camera/animation state; diagnostic overhead, not clean pacing')
    parser.add_argument('--no-captures',action='store_true',help='Disable renderer dumps and label screenshots for timing measurements')
    parser.add_argument('--profile-cpu',action='store_true',help='Enable the existing 200 Hz stack sampler; diagnostic only, not clean pacing')
    parser.add_argument('--profile-depth',type=int,choices=range(1,33),default=8,help='Captured caller frames for CPU diagnostics (default 8)')
    parser.add_argument('--profile-pc-symbol',help='Exact executable symbol for bounded native instruction/PDB-line reporting; requires --profile-cpu')
    parser.add_argument('--texture-mode',choices=('classic','enhanced'),default='classic',help='Texture selection in this isolated session')
    parser.add_argument('--crowd-weapons',choices=('recommended','rockets'),default='recommended',help='Weapon preset selected through the normal crowd-match menu')
    parser.add_argument('--trace-textures',action='store_true',help='Enable bounded texture-stage CPU timing; diagnostic only')
    parser.add_argument('--profile-presents',action='store_true',help='Record bounded host Present intervals for original or native timing')
    parser.add_argument('--profile-delay-seconds',type=int,choices=range(601),default=0,help='Delay Present CSV rows after first successful Present; requires a compatible build')
    parser.add_argument('--watch-hud',action='store_true',help='Observe bounded HUD message slots and their elapsed time')
    parser.add_argument('--watch-actors',action='store_true',help='Observe local actor positions/health; proximity is not visibility and readings can cross an update')
    parser.add_argument('--watch-native-clock',action='store_true',help='Read timing-call counter and game ticks with QPC bounds; not completed simulation or GPU frames')
    parser.add_argument('--watch-effects',action='store_true',help='Read bounded effect lists at checkpoints and requested samples; asynchronous observations, not GPU draws')
    parser.add_argument('--trace-surface-switches',action='store_true',help='Record 90 positive-time images per FPS segment, at most 360 total')
    surfaces=parser.add_mutually_exclusive_group()
    surfaces.add_argument('--native-surfaces',action='store_true',default=None,help='Explicitly enable native water (the common-renderer beta default)')
    surfaces.add_argument('--original-surfaces',action='store_false',dest='native_surfaces',help='Diagnostic original water at any FPS')
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
    parser.add_argument('--scenario',choices=['rocket-impact','weapon-set','surface-feedback','surface-feedback-gpu','surface-native','surface-native-switch','surface-native-long','crowd-venice','crowd-hotel','crowd-disco','crowd-temple'])
    args=parser.parse_args()
    if args.scenario=='rocket-impact' and not (args.watch_actors and args.watch_effects):
        parser.error('rocket-impact requires --watch-actors and --watch-effects for acceptance')
    if args.profile_delay_seconds and not args.profile_presents:
        parser.error('--profile-delay-seconds requires --profile-presents')
    if args.crowd_weapons!='recommended' and not (args.scenario or '').startswith('crowd-'):
        parser.error('--crowd-weapons requires a crowd scenario')
    if not all(240<=v<=8192 for v in args.display_size):parser.error("Display dimensions must be between 240 and 8192")
    if args.scenario in ('surface-feedback','surface-feedback-gpu') and (args.live_surfaces or args.bias_surfaces or args.float_surfaces or args.fractional_surfaces):
        parser.error('surface-feedback captures the original feedback; disable the older surface experiments')
    if args.scenario=='surface-native-switch' and not (args.native_fps==240 and args.native_surfaces is not False and args.trace_surface_images and args.trace_surface_switches):
        parser.error('surface-native-switch requires native240, default/enabled native water and both surface trace flags')
    if args.native_surfaces and (args.live_surfaces or args.bias_surfaces or args.float_surfaces or args.fractional_surfaces):
        parser.error('--native-surfaces cannot combine with older surface experiments')
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
    # Bounded foreground tests must not compete with another checkout or play
    # session. Never close the other process; leave it to its owner instead.
    import csv
    inventory=subprocess.run(['tasklist.exe','/FI','IMAGENAME eq futureperfect_recomp.exe',
                              '/FO','CSV','/NH'],capture_output=True,text=True,check=True,timeout=10)
    existing=[row[1] for row in csv.reader(inventory.stdout.splitlines())
              if len(row)>1 and row[0].lower()=='futureperfect_recomp.exe']
    assert not existing, f'Another game is running (PIDs {", ".join(existing)}); wait for its owner before testing'
    if args.profile_pc_symbol and (not args.profile_cpu or len(args.profile_pc_symbol)>=96 or any(c in args.profile_pc_symbol for c in '\t\r\n')):
        parser.error('--profile-pc-symbol requires --profile-cpu and a symbol shorter than 96 characters without tabs/newlines')
    assert hashlib.sha256((ROOT/'private/game/default.xbe').read_bytes()).hexdigest()==XBE_SHA256
    if args.lan_role:
        subprocess.run([sys.executable,str(ROOT/'tools/network-identity.py'),'--verify',
                        '--executable',str(ROOT/'build/windows/Release/futureperfect_recomp.exe')],check=True)
    run=ROOT/'private/test-saves'/args.run_name;env=prepare_run(ROOT,run)
    env['TSFP_FRAME_PROFILE_DELAY_MS']=str(args.profile_delay_seconds*1000)
    env['TSFP_TEXTURE_MODE']='4k' if args.texture_mode=='enhanced' else 'classic'
    if args.lan_role:
        env.update(TSFP_NETWORK='1',
                   TSFP_NET_BIND='127.0.0.2' if args.lan_role=='host' else '127.0.0.3',
                   TSFP_NET_PEERS='127.0.0.3' if args.lan_role=='host' else '127.0.0.2')
    # This owned, unlocked test directory starts from the requested CLI rate.
    # In-session restarts retain any preference saved by the menu under test.
    (run/'pc-frame-rate.ini').unlink(missing_ok=True)
    (run/'runtime.ini').write_text(f'frame_target = 144\nframe_cap = 60\nframe_interp = 1\nfullscreen = {int(args.fullscreen)}\nvrr = 1\n')
    (run/'display.ini').write_text(f'TSFP_DISPLAY_V2 {args.display_size[0]} {args.display_size[1]} {args.fov}\n')
    (run/'discord.ini').write_text('TSFP_DISCORD_V1 0 0\n')
    commands=run/'commands.jsonl';commands.write_text('')
    if args.scenario=='rocket-impact':
        route=json.loads((ROOT/'tests/fixtures/rocket-impact-commands.json').read_text())
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
    if args.scenario=='weapon-set':
        route=[dict(label='main-menu',wait=24),
            dict(keys=[40,69,69,69,69,38,38],gap=1.3,label='before-weapons',wait=12),
            dict(keys=[69],label='weapons',wait=20),
            dict(keys=[40,40,40],label='weapons-selection',wait=10),
            dict(keys=[70],label='after-weapons',wait=15),dict(stop=True)]
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
    if args.scenario in ('crowd-venice','crowd-hotel','crowd-disco','crowd-temple'):
        route=json.loads((ROOT/'tests/fixtures/crowd-venice-commands.json').read_text())
        if args.scenario in ('crowd-hotel','crowd-disco','crowd-temple'):
            # Verified list: Vietnam, Hotel, Venice, Temple, Disco.
            assert route[1]['label']=='match-setup'
            moves={'crowd-hotel':1,'crowd-temple':3,'crowd-disco':4}[args.scenario]
            route[1]['keys']=[40,69,69,69]+[40]*moves+[69]
            assert route[7]['label']=='spawn'
            # Five-second idle waits usually observe a death camera. Keep
            # ordinary movement/fire/respawn input frequent without editing health.
            route=route[:8]
            for block in range(32):
                step=dict(keys=[69,87,1],hold=.6,gap=.1,wait=.1)
                if block in (0,14,28):step['label']={0:'combat-one',14:'combat-four',28:'combat-six'}[block]
                if block and block%6==0:step['mouse']=[180,0]
                route.append(step)
            route.append(dict(stop=True))
        if args.crowd_weapons=='rockets':
            # Calibrated from Recommended: six right presses select Rockets.
            # Return to GO before continuing the existing score/bot setup.
            route[2:2]=[
                dict(keys=[38,38,69],gap=.3,wait=.7,label='weapon-menu'),
                dict(keys=[39]*6,gap=.3,wait=.7,label='rocket-set'),
                dict(keys=[70,40,40],gap=.3,wait=.7,label='rocket-setup')]
        route.insert(1,dict(window='focus',wait=1))
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
    if args.scenario in ('surface-native','surface-native-switch','surface-native-long'):
        key={'surface-native':'scotland','surface-native-switch':'scotland_switch','surface-native-long':'scotland_long'}[args.scenario]
        route=json.loads((ROOT/'tests/fixtures/procedural-surface-commands.json').read_text())[key]
        if args.scenario=='surface-native-long':
            # Background rendering is not foreground gameplay evidence.
            marker=next(i for i,step in enumerate(route) if step.get('label')=='water-stable-start')
            route.insert(marker,dict(window='focus',wait=1,label='water-focus'))
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
    env.pop('TSFP_SURFACE_NATIVE_TEST',None)
    if args.native_surfaces is not None:env['TSFP_SURFACE_NATIVE_TEST']='1' if args.native_surfaces else '0'
    state=run/'input-state.txt';publish_input_state(ROOT,state,'0 0\n')
    initial=[(5000,69),(10000,69),(14000,40),(18000,69)]
    replay=run/'input.txt';replay.write_text('\n'.join(f'{at+d} key {k} {v}' for at,k in initial for d,v in [(0,1),(180,0)])+'\n')
    env.update(TSFP_PC_INPUT_REPLAY=str(replay),TSFP_PC_INPUT_STATE=str(state),
        RECOMP_WINDOW_BACKGROUND='0' if args.fullscreen else '1',RECOMP_PC_MOUSE='1',TSFP_DISCORD_CONFIG=str(run/'discord.ini'),
        RECOMP_HLE_D3D8_DUMP=str(run/'frame'),RECOMP_HLE_D3D8_DUMP_EVERY='30',
        RECOMP_HLE_D3D8_DUMP_KEEP_LAST='3',RECOMP_FPS='5',RECOMP_INTERP_DEBUG='1')
    if args.no_captures:
        for key in ('RECOMP_HLE_D3D8_DUMP','RECOMP_HLE_D3D8_DUMP_EVERY','RECOMP_HLE_D3D8_DUMP_KEEP_LAST'):
            env.pop(key,None)
    if args.native_fps is not None:
        env.update(TSFP_NATIVE_FPS=str(args.native_fps),RECOMP_FRAME_TARGET='0',RECOMP_FRAME_INTERP='1',
                   **({} if args.no_native_trace else dict(TSFP_NATIVE_TRACE=str(run/'native-frames.csv'))))
    env['TSFP_SURFACE_TRACE_SWITCHES']='1' if args.trace_surface_switches else '0'
    env['TSFP_SURFACE_LIVE_TEST']='1' if args.live_surfaces else '0'
    env['TSFP_SURFACE_BIAS_TEST']='1' if args.bias_surfaces else '0'
    env['TSFP_SURFACE_FLOAT_TEST']='1' if args.float_surfaces else '0'
    env['TSFP_SURFACE_FRACTIONAL_TEST']='1' if args.fractional_surfaces else '0'
    if args.watch_input:
        env.update(RECOMP_WATCH_VA='0x6B9408', RECOMP_TRACE_ONLY='sub_00042830,sub_00042870,sub_000428D0', RECOMP_TRACE_ONLY_MAX='32')
    if args.trace_vehicle_contact:
        env.update(RECOMP_TRACE_ONLY='sub_0017CAE0,sub_0017BC90,sub_00174EC0',RECOMP_TRACE_ONLY_MAX='64')
    if args.profile_cpu:
        env.update(RECOMP_SAMPLE='200',RECOMP_SAMPLE_REPORT='10',RECOMP_SAMPLE_DEPTH=str(args.profile_depth),
                   RECOMP_SAMPLE_DUMP=str(run/'cpu-sample.txt'))
        if args.profile_pc_symbol:env['RECOMP_SAMPLE_PC_SYMBOL']=args.profile_pc_symbol
    if args.trace_textures:env['TSFP_TEXTURE_UPLOAD_TRACE']=str(run/'texture-stages.csv')
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
    def read_bytes(address,size):
        # snapshot_effects bounds all addresses and allocations before calling.
        buffer=c.create_string_buffer(size)
        if not kernel.ReadProcessMemory(handle,address+0x10000,buffer,size,None):return None
        return buffer.raw
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
    def actor_snapshot():
        actors=[];pool=word(0x7356d8);roster=word(0x7de400) or 0
        humans,bots=roster&255,(roster>>16)&255
        if pool is not None and 0x80000000<=pool<=0x84000000-16*0xea0:
            for slot in range(min(16,humans+bots)):
                entity=word(pool+slot*0xea0)
                if entity is not None and 0x80000000<=entity<=0x84000000-0x1d0:
                    actors.append(dict(slot=slot,entity=entity,position=floats(entity+0x64,3),
                                       health=floats(entity+0x1cc,1)))
        return dict(pool=pool,pool_after=word(0x7356d8),human_slots=humans,bot_slots=bots,actors=actors)
    def network_snapshot():
        return dict(state=word(0x76b1a4),role=word(0x76b140),
                    peers=[i for i in range(16) if word(0x666040+i*4)],**actor_snapshot())
    user.GetClientRect.argtypes=[c.c_void_p,c.POINTER(ui.w.RECT)]
    user.IsWindowVisible.argtypes=[c.c_void_p]
    user.IsIconic.argtypes=[c.c_void_p]
    def window_snapshot():
        rect=ui.w.RECT()
        readable=bool(user.GetClientRect(window,c.byref(rect)))
        return dict(visible=bool(user.IsWindowVisible(window)),minimized=bool(user.IsIconic(window)),
            foreground=user.GetForegroundWindow()==window,
            client_size=[rect.right-rect.left,rect.bottom-rect.top] if readable else None)
    def snapshot():
        pointer=word(0x761C48)
        return dict(pid=process.pid,scene=word(0x79094C),flags=word(0x7DE458),
            paused=word(pointer)&1 if pointer else 0,mission=word(0x78AD48),
            held=word(0x6B93DC),pressed=word(0x6B93E0),suppression=word(0x6B9408),
            raw_ticks=word(0x4E7940),game_ticks=word(0x4E7944),level_ticks=word(0x7DE318),
            match_state=word(0x7DE30C),countdown=word(0x4D2878),countdown_ticks=word(0x4D287C),
            roster=word(0x7de400),window=window_snapshot(),
            **(dict(actors=actor_snapshot()) if args.watch_actors else {}),
            **(dict(effects=snapshot_effects(read_bytes)) if args.watch_effects else {}),
            player=player_snapshot(),**(dict(hud=hud_snapshot()) if args.watch_hud else {}),
            **(dict(network=network_snapshot()) if args.lan_role else {}),
            **(dict(surfaces=surface_snapshot()) if args.watch_surfaces else {}),
            **(dict(native_clock=native_clock_snapshot()) if args.watch_native_clock else {}))
    def native_clock_snapshot():
        # 7497B8 increments once in the native timing routine before returning
        # to gameplay. This is separate from the RHI's accepted-Present count.
        begin=time.perf_counter_ns();scene=word(0x79094c);match=word(0x7de30c)
        calls=word(0x7497b8);ticks=word(0x7de318)
        stable=(scene,match)==(word(0x79094c),word(0x7de30c))
        return dict(begin_ns=begin,end_ns=time.perf_counter_ns(),scene=scene,
                    match_state=match,identity_stable=stable,timing_calls=calls,level_ticks=ticks)
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
        publish_input_state(ROOT,state,'0 0\n')
        if args.trace_surface_images:
            env['TSFP_SURFACE_TRACE']=str(run/f'run{index}-surface.csv')
        if args.profile_presents:
            env['TSFP_FRAME_PROFILE']=str(run/f'run{index}-presents.csv')
        if args.trace_network_poses:
            env['TSFP_NATIVE_POSE_TRACE']=str(run/f'run{index}-network-poses.csv')
        if args.trace_local_poses:
            env['TSFP_NATIVE_LOCAL_TRACE']=str(run/f'run{index}-local-poses.csv')
        else:
            env.pop('TSFP_NATIVE_LOCAL_TRACE',None)
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
        initial_commands_sha256=hashlib.sha256(commands.read_bytes()).hexdigest(),
        input_transport='startup replay plus polled state file',input_poll_ms=16,
        watch_native_clock=args.watch_native_clock,trace_local_poses=args.trace_local_poses,
        display_size=list(args.display_size),fullscreen=args.fullscreen,fov=args.fov,texture_mode=args.texture_mode,crowd_weapons=args.crowd_weapons,native_trace=not args.no_native_trace,profile_cpu=args.profile_cpu,profile_depth=args.profile_depth if args.profile_cpu else None,watch_actors=args.watch_actors,trace_textures=args.trace_textures,
        native_surface_mode="default" if args.native_surfaces is None else ("enabled" if args.native_surfaces else "disabled"),
        profile_delay_seconds=args.profile_delay_seconds,
        profile_pc_symbol=args.profile_pc_symbol,
        native_fps=args.native_fps or 0,native_surfaces=args.native_surfaces,scenario=args.scenario,seconds=args.seconds,captures=not args.no_captures,watch_effects=args.watch_effects,watch_hud=args.watch_hud,watch_surfaces=args.watch_surfaces,trace_surface_images=args.trace_surface_images,fractional_surfaces=args.fractional_surfaces,float_surfaces=args.float_surfaces,bias_surfaces=args.bias_surfaces,live_surfaces=args.live_surfaces,trace_surface_switches=args.trace_surface_switches,minimized=args.minimized,profile_presents=args.profile_presents,lan_role=args.lan_role,sample_network_ms=args.sample_network_ms,trace_network_poses=args.trace_network_poses,observation_clock=time.get_clock_info('perf_counter').implementation,clock_resolution=time.get_clock_info('perf_counter').resolution,started_monotonic=start),indent=2))
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
                    publish_input_state(ROOT,state,f'0 0 {seen} {motion[0]} {motion[1]}\n')
                    time.sleep(.1)
                hold_samples=[];key_events=[]
                for key in command.get('keys',[]):
                    publish_input_state(ROOT,state,f'{key} 1\n')
                    down_ns=time.perf_counter_ns()
                    held_at=time.perf_counter()
                    if command.get('sample_hold'):
                        while time.perf_counter()-held_at<hold:
                            hold_samples.append(dict(seconds=round(time.perf_counter()-start,6),
                                held_seconds=round(time.perf_counter()-held_at,6),key=key,
                                **snapshot()))
                            time.sleep(min(.05,max(0,hold-(time.perf_counter()-held_at))))
                    else:time.sleep(hold)
                    publish_input_state(ROOT,state,'0 0\n')
                    key_events.append(dict(key=key,down_ns=down_ns,up_ns=time.perf_counter_ns()))
                    time.sleep(command.get('gap',.7))
                if command.get('window')=='toggle':user.PostMessageW(window,0x104,13,1<<29)
                if command.get('window')=='focus':
                    user.ShowWindow(window,6);time.sleep(1)
                    lost=user.GetForegroundWindow()!=window
                    user.ShowWindow(window,9);user.SetForegroundWindow(window);time.sleep(1)
                    report.append(dict(focus_lost=lost,focus_restored=user.GetForegroundWindow()==window))
                wait_samples=[];wait_started=time.perf_counter();wait_seconds=command.get('wait',2.5)
                if command.get('sample_wait'):
                    while time.perf_counter()-wait_started<wait_seconds:
                        began=time.perf_counter()
                        observation=snapshot()
                        wait_samples.append(dict(seconds=began-start,end_seconds=time.perf_counter()-start,**observation))
                        time.sleep(min(.05,max(0,wait_seconds-(time.perf_counter()-wait_started))))
                else:time.sleep(wait_seconds)
                item=dict(seconds=round(time.perf_counter()-start,3),observed_ns=time.perf_counter_ns(),
                    command=command,key_events=key_events,**snapshot(),fullscreen=not bool(user.GetWindowLongW(window,-16)&0x00C00000))
                if hold_samples:item['hold_samples']=hold_samples
                if wait_samples:item['wait_samples']=wait_samples
                if 'label' in command:capture(command['label'])
                if args.profile_cpu and command.get('label') in ('ten-bots','spawn','combat-four'):
                    source=run/'cpu-sample.txt'
                    if source.exists():
                        with source.open('rb') as stream: diagnostic=stream.read(4*1024*1024+1)
                        assert len(diagnostic)<=4*1024*1024,'CPU diagnostic exceeded snapshot limit'
                        # The sampler rewrites this file; a snapshot can be partial.
                        # Check completeness before comparing cumulative reports.
                        (run/(command['label']+'-cpu.txt')).write_bytes(diagnostic)
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
    if args.scenario=='rocket-impact':
        from rocket_impact_stats import analyze
        result=analyze(report)
        (run/'rocket-impact-results.json').write_text(json.dumps(result,indent=2)+'\n')
        print('PASS: single-rocket lethality, explosion lifetime/cleanup and respawn/walk; not general damage parity',flush=True)
    if args.scenario=='weapon-set':print('Weapon-set settled FPS:',weapon_set_metrics(run),flush=True)
    if args.scenario in ('crowd-venice','crowd-hotel','crowd-disco','crowd-temple'):
        scene={'crowd-hotel':39,'crowd-venice':46,'crowd-temple':49,'crowd-disco':56}[args.scenario]
        checkpoints=[i for i in report if i.get('command',{}).get('label') in ('spawn','combat-one','combat-four','combat-six')]
        assert len(checkpoints)==4,'Incomplete crowd route'
        for item in checkpoints:
            assert item['scene']==scene and item['match_state']==7 and not item['paused'],item
            assert item['roster'] is not None and (item['roster']>>16)&255==10,item
        print(f'PASS: {args.scenario} active checkpoints retain ten configured bots; inspect window and pacing evidence',flush=True)

    if args.scenario in ('surface-feedback','surface-feedback-gpu'):
        captured=(run/'feedback.bin').read_bytes()
        assert captured[:8]==b'TSWATER1' and len(captured)==8+16*(92+3*128*128*4)
        print('Captured 16 completed GPU feedback triplets; shader comparison still required',flush=True)
        if args.scenario=='surface-feedback-gpu':
            computed=(run/'feedback.bin.gpu').read_bytes()
            assert computed[:8]==b'TSWGPU1\0' and len(computed)==8+16*(92+2*128*128*16)
            print('Captured 16 live GPU solver midpoint/endpoint pairs; independent comparison still required',flush=True)
    if args.scenario=='surface-native-switch':
        from procedural_surface_stats import load_capture,validate_switch_cycle,measure_segments
        rows,pixels=load_capture(run/'run0-surface.csv')
        validate_switch_cycle(rows)
        results=measure_segments(rows,pixels)
        resource=int(rows[0]['resource'])
        assert all(int(row['resource'])==resource for row in rows),'Water resource changed during preset cycle'
        creations=[int(frame) for identity,frame in re.findall(
            r'\[SURFACE-NATIVE\] Created owner resource=([0-9A-F]+) frame=(\d+)',
            (run/'run0.err').read_text(errors='replace')) if int(identity,16)==resource]
        assert len(creations)==1 and creations[0]<int(rows[0]['frame']),creations
        results['owner_creations']=creations
        (run/'surface-results.json').write_text(json.dumps(results,indent=2)+'\n')
        print('PASS: complete native240/native180/Original60/native120 image cycle',flush=True)
    print('PASS: bounded session completed; inspect report and captures for scenario coverage',flush=True)

if __name__=='__main__':main()
