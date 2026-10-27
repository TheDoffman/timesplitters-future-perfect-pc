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
import math
import os
import re
from pathlib import Path
import subprocess
import struct
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
    parser.add_argument('--pair-role',choices=['host','guest'])
    parser.add_argument('--directory')
    parser.add_argument('--lobby-code')
    parser.add_argument('--pair-load-probe',action='store_true')
    parser.add_argument('--story-presentation-probe',action='store_true')
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
    parser.add_argument('--scenario',choices=['story-entry','story-host','story-host-controls','story-pair','story-local-load','rocket-impact','weapon-set','surface-feedback','surface-feedback-gpu','surface-native','surface-native-switch','surface-native-long','crowd-venice','crowd-hotel','crowd-disco','crowd-temple'])
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
    assert not args.pair_load_probe or args.scenario=='story-pair'
    if args.scenario=='story-pair':
        from urllib.parse import urlsplit
        endpoint=urlsplit(args.directory or '')
        assert args.pair_role and endpoint.scheme=='http' and endpoint.hostname=='127.0.0.1'
        assert endpoint.port and not endpoint.username and not endpoint.password
        assert endpoint.path in ('','/') and not endpoint.query and not endpoint.fragment
        if args.pair_role=='guest':
            assert args.lobby_code and len(args.lobby_code)==16 and all(c in '0123456789abcdefABCDEF' for c in args.lobby_code)
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
    stop_request=run/'stop-request'
    assert not stop_request.is_symlink()
    stop_request.unlink(missing_ok=True)
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
    elif args.scenario=='story-entry':
        route=[dict(label='main-menu',wait=24),
            dict(keys=[69],label='story-chooser',wait=5),
            dict(keys=[69],label='local-story',wait=5),
            dict(keys=[70],label='local-back',wait=5),
            dict(keys=[70],label='chooser-back',wait=5),
            dict(keys=[69],label='story-chooser-again',wait=5),
            dict(keys=[40,69],cancel_online=True,label='online-cancelled',wait=5),
            dict(stop=True)]
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
    elif args.scenario=='story-local-load':
        route=[dict(label='main-menu',wait=24),dict(keys=[69],label='story-chooser',wait=4),
            dict(keys=[69],label='local-levels',wait=4),
            dict(keys=[69],label='local-difficulty',wait=4),
            dict(keys=[69],label='local-briefing',wait=4),
            dict(keys=[69],label='local-loading',wait=25),
            dict(keys=[69],label='local-skip',wait=5),
            dict(keys=[69],label='local-game',wait=5),
            dict(label='local-settled',wait=15,world_probe=True),
            dict(keys=[87],hold_seconds=1,sample_hold=True,label='local-move',wait=2,world_probe=True),
            dict(keys=[83],hold_seconds=1,sample_hold=True,label='local-back',wait=2,world_probe=True),
            dict(keys=[1],hold_seconds=1,label='local-fire',wait=2,world_probe=True),dict(stop=True)]
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
    env['TSFP_ONLINE_SETTINGS']=str(run/'online-settings.json')
    # Typed Story now supplies normal start behavior. Instrumentation must not
    # enable a hidden feature switch that an ordinary player does not receive.
    env.pop('TSFP_STORY_LOAD_PROBE',None)
    directory_server=None;directory_thread=None
    if args.scenario=='story-local-load':
        env['TSFP_STORY_TRACE']='1';env['TSFP_STORY_TRACE_LOCAL']='1'
    if args.scenario in ('story-host','story-host-controls','story-pair'):
        env['TSFP_STORY_TRACE']='1'
        env['TSFP_BOT_TRACE']=str(run/'story-hit-trace.log')
        import socket
        from lobby_server import Server
        if args.scenario!='story-pair':
            directory_server=Server(('127.0.0.1',0))
            directory_thread=threading.Thread(target=directory_server.serve_forever,daemon=True)
            directory_thread.start()
        directory_address=args.directory if args.scenario=='story-pair' else f'http://127.0.0.1:{directory_server.server_port}'
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as probe:
            probe.bind(('127.0.0.1',0));host_port=probe.getsockname()[1]
        command_root=ROOT/'private/network-validation'/args.run_name
        command_root.mkdir(parents=True,exist_ok=True)
        online_command=command_root/'online-command.json'
        online_command.write_text(json.dumps(dict(directory=directory_address,
            action='join' if args.pair_role=='guest' else 'host',code=args.lobby_code,
            port=host_port,automatic_host=False)))
        env['TSFP_ONLINE_TEST_COMMAND']=str(online_command)
        route=[dict(label='main-menu',wait=24),dict(keys=[69],label='story-chooser',wait=4),
            dict(keys=[40,69],label='network-browser',wait=10),
            dict(keys=[69],label='host-lobby',wait=10),dict(stop=True)]
        if args.scenario=='story-pair':
            if args.pair_role=='guest':
                route[3]=dict(keys=[40,69],label='guest-lobby',wait=10)
            route[-1:]=[] if args.pair_load_probe else [dict(label='pair-wait',wait=10) for _ in range(9)]+[dict(stop=True)]
        if args.scenario=='story-host-controls':
            route[-1:]=[dict(keys=[89],label='story-options',wait=4,expect_menu=0x5238e0),
                dict(keys=[40,69],label='story-difficulty',wait=4,expect_story_difficulty=True),
                dict(keys=[69],label='story-settings-return',wait=4,expect_menu=0x4e99c4),
                dict(keys=[82],label='story-progress',wait=4),
                dict(keys=[70],label='story-progress-return',wait=4),dict(stop=True)]
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
    def real(address):
        bits=word(address)
        return c.c_float.from_buffer_copy(c.c_uint32(bits)).value if bits is not None else None
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
    def player_snapshot(player=None):
        if player is None:player=word(0x7b0c7c)
        if player is None or not 0x80000000<=player<0x84000000:return None
        entity=word(player+0x14)
        actor=word(entity+0x7c) if entity else None
        result=dict(player=player,position=floats(player+0xb4,3),look=floats(player+0x100,2),
                    story_view=dict(state=word(player+0x114),saved_state=word(player+0x110),
                        port=word(player+4),view=word(player+8),firstperson_animation_mode=word(player+0x14f8),
                        camera_script=word(word(player+8)+0x26c) if word(player+8) else None,
                        camera_flags=word(word(player+8)+0x21c) if word(player+8) else None),
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
    def world_snapshot():
        count=word(0x74C37C);characters=[]
        if count is None or not 0<=count<=256:return dict(count=count,characters=[])
        # The original active-character list stores character-data pointers.
        # Their first word is the entity (the existing handoff_actor observer
        # uses this layout); +230 is the original type2 network object ID.
        for index in range(min(count,64)):
            actor=word(0x7A3580+index*4)
            if actor is None or not 0x10000<=actor<0x84000000:continue
            entity=word(actor)
            if entity is None or not 0x10000<=entity<0x84000000:continue
            characters.append(dict(index=index,network_id=word(entity+0x230),
                loadout=word(actor+0x94),character_flags=word(actor+0x24),
                entity=entity,context=word(entity+0x150),flags=word(entity+0x28),
                position=floats(entity+0x64,3),health=real(entity+0x1cc)))
        # Original359390 resolves type2 references by the FIRST matching ID in
        # the entity pool. Pointer-free replica creation must retain that identity.
        entity_pool=word(0x72F5E8);entity_count=word(0x7B4FAC)
        identities={};objects=[]
        if entity_pool and 0x10000<=entity_pool<0x84000000 and entity_count is not None and 0<=entity_count<=8192:
            raw=(c.c_ubyte*(entity_count*0x2e8))()
            if kernel.ReadProcessMemory(handle,entity_pool+0x10000,raw,len(raw),None):
                for index in range(entity_count):
                    key=struct.unpack_from('<I',raw,index*0x2e8+0x230)[0]
                    if key:identities.setdefault(key,[]).append(entity_pool+index*0x2e8)
                    offset=index*0x2e8
                    kind,flags=struct.unpack_from('<I4xI',raw,offset+0x20)
                    health=struct.unpack_from('<f',raw,offset+0x1cc)[0]
                    maximum=struct.unpack_from('<f',raw,offset+0x1d8)[0]
                    if kind!=8 and flags&0x10 and math.isfinite(health) and math.isfinite(maximum) and 0<maximum<100000 and len(objects)<128:
                        objects.append(dict(index=index,type=kind,flags=flags,
                            model_kind=struct.unpack_from('<H',raw,offset+0x94)[0],
                            script=struct.unpack_from('<I',raw,offset+0x60)[0],network_id=key,
                            position=list(struct.unpack_from('<3f',raw,offset+0x64)),health=health,max_health=maximum))
        for entry in characters:
            entity=entry['entity'];actor=word(entity+0x7c);model=word(entity+4)
            entry.update(slot=word(actor+8),visibility_flags=word(actor+0x2c),kind=[short(actor+0x48),short(actor+0x4a)],
                team=word(actor+0x84),last_damage_tick=word(entity+0x1d4),
                clip=word(model+0xd4) if model else None,
                next_clip=word(model+0x12c) if model else None,
                weapon_defs=[word(actor+0xd4),word(actor+0x204)],
                weapon_models=[word(actor+0x18c),word(actor+0x2bc)],
                clip_time=real(model+0xdc) if model else None,
                model_header=[word(model+i*4) for i in range(32)] if model else [],
                bounds_center=floats(entity+0x1a8,3),bounds_radius=floats(entity+0x1c0,3),
                lookup_entities=identities.get(entry['network_id'],[]))
        return dict(count=count,sample_limit=64,characters=characters,entity_pool_count=entity_count,objects=objects)
    def checkpoint_pad(index,table):
        record=table+index*32;pad=word(record+12)
        # Setup rows hold a pad reference at +12, not XYZ. Resolve the
        # original9A420 local table route without executing guest code.
        pool=word(0x715f2c);slot=pad
        if pad is not None and 8000<=pad<8350:pool=word(0x715f34);slot=pad-8000
        elif pad is not None and 8350<=pad<8370:pool=word(0x715f38);slot=pad-8350
        valid=pool and slot is not None and 0<=slot<8192
        return dict(index=index,flags=word(record),pad=pad,
            position=[real(pool+slot*32+12+j*4) for j in range(3)] if valid else [])
    def story_scene_snapshot(world=False):
        setup=word(0x74C36C)
        budget=word(setup+8) if setup else None
        return dict(setup=setup,budget_index=budget,
            budget_count=word(0x53D2D0+budget*0xA4) if budget is not None and budget<256 else None,
            record_counts={hex(offset):word(setup+offset) for offset in (0x10,0x28,0x30,0x88,0xB0,0xE0)} if setup else {},
            checkpoint_pads=[checkpoint_pad(i,word(setup+0x24))
                for i in range(min(word(setup+0x28) or 0,256))]
                if world and setup and word(setup+0x24) else [],
            allocation_counters={'0x7a3500':word(0x7A3500),'0x74c388':word(0x74C388)})
    def snapshot(world=False):
        pointer=word(0x761C48)
        menu=word(0x7613E4);descriptor=word(menu+4) if menu else None
        def config(address):
            return dict(players=(word(address) or 0)&255,
                local_players=(word(address+1) or 0)&255,bots=(word(address+2) or 0)&255,
                scene=(word(address+0x54) or 0)&255,rule=(word(address+0x55) or 0)&255,
                variant=(word(address+0x56) or 0)&255,difficulty=(word(address+0x57) or 0)&255,
                flags=word(address+0x58),
                characters=[(word(address+0x14+i) or 0)&255 for i in range(4)])
        actors=[];pool=word(0x7356D8)
        if pool and (args.pair_load_probe or args.scenario=='story-local-load'):
            for slot in range(min(word(0x790950) or 0,4)):
                actor=pool+slot*0xea0;entity=word(actor)
                # Existing bot replication uses +64 for world position;
                # +1a8 is collision-bounds centre, not movement telemetry.
                position=[real(entity+0x64+i*4) for i in range(3)] if entity else []
                context=(word(0x7B0C48) or 0)+slot*0x1584
                actors.append(dict(slot=slot,entity=entity,position=position,
                    context=context,local_port=word(context+4),context_flags=word(context+0xc),
                    story_view=dict(firstperson_animation_mode=word(context+0x14f8),camera=word(context+8),
                        matrix_buffer=word(word(context+8)+0x208) if word(context+8) else None,
                        script=word(word(context+8)+0x26c) if word(context+8) else None,
                        flags=word(word(context+8)+0x21c) if word(context+8) else None),
                    action_masks=[word(context+0x20+i*4) for i in range(6)],
                    context_position=[real(context+0xb4+i*4) for i in range(3)],
                    # Field contracts used by pc_network_bots_guest_shot and
                    # its snapshot sender; weapon IDs are not ammo counters.
                    weapons=[word(actor+0xd4+hand*0x130) for hand in range(2)],
                    aim=[real(actor+0xb88+i*4) for i in range(3)],
                    muzzle=[real(actor+0xbb4+i*4) for i in range(3)],
                    health=real(entity+0x1cc) if entity else None,
                    entity_flags=word(entity+0x28) if entity else None,
                    life=[word(actor+0x10+i*4) for i in range(4)],
                    actor_flags=[word(actor+0x24+i*4) for i in range(3)]))
        contexts=word(0x7B0C48);local_player=None
        if contexts:
            owned=[contexts+i*0x1584 for i in range(min(word(0x790950) or 0,4))
                   if word(contexts+i*0x1584+4)==0]
            if len(owned)==1:local_player=player_snapshot(owned[0])
        return dict(pid=process.pid,scene=word(0x79094C),flags=word(0x7DE458),
            local_player=local_player,
            paused=word(pointer)&1 if pointer else 0,mission=word(0x78AD48),
            menu_mode=word(0x78AD2C),network_phase=word(0x76B1A4),role=word(0x76B140),
            menu_descriptor=descriptor,menu_builder=word(descriptor) if descriptor else None,
            player_slots=sum(bool((word(0x77728c+i*0x124) or 0)&1) for i in range(16)),
            active_mode=word(0x78AD20),network_enabled=word(0x76B108),actor_pool=word(0x7356D8),
            game_tick=word(0x7DE318),pause_flags=word(pointer) if pointer else 0,
            # Original main-update dispatch uses state-1 at 6887b; state8
            # selects the failure path. Network status2 propagates that state
            # at35d968. Observe both without modifying either state machine.
            game_state=word(0x7DE30C),state_seconds=real(0x7DE300),
            network_end_state=word(0x76B1A0),
            result_flags=word(0x7DE45C),failure_timer=word(0x7DE460),
            context_count=word(0x790950),context_pool=word(0x7B0C48),
            context_states=[word((word(0x7B0C48) or 0)+i*0x1584+0x114)
                for i in range(min(word(0x790950) or 0,4))] if word(0x7B0C48) else [],
            frontend_config=config(0x78AD58),active_config=config(0x7DE400),actors=actor_snapshot() if args.watch_actors else actors,
            input_block=word(0x6B9888),network_input=word(0x76B0FC),
            active_characters=word(0x74C37C),story_stage=word(0x78AD50),
            story_objectives=[dict(index=i,identity=short((word(0x78cecc) or 0)+i*0x4c),
                kind=word((word(0x78cecc) or 0)+i*0x4c+8),
                changed=word((word(0x78cecc) or 0)+i*0x4c+0x34))
                for i in range(min(word(0x78cec8) or 0,128))] if word(0x78cecc) else [],
            story_checkpoints=[word((word(0x75f5d0) or 0)+i*0x238+4)
                for i in range(min(word(0x790950) or 0,4))] if word(0x75f5d0) else [],
            local_axes=[word(0x6B93E8+i*4) for i in range(4)],
            network_pads=[dict(held=word(0x6B94D4+slot*0x3c),
                blocked=word(0x6B9500+slot*0x3c),
                axes=[word(0x6B94E0+slot*0x3c+i*4) for i in range(4)])
                for slot in range(min(word(0x790950) or 0,4))],
            held=word(0x6B93DC),pressed=word(0x6B93E0),suppression=word(0x6B9408),
            raw_ticks=word(0x4E7940),game_ticks=word(0x4E7944),level_ticks=word(0x7DE318),
            match_state=word(0x7DE30C),countdown=word(0x4D2878),countdown_ticks=word(0x4D287C),
            story_config_tail=[word(address) for address in range(0x7DE460,0x7DE4A0,4)]
                if args.pair_load_probe or args.scenario=='story-local-load' else None,
            story_scene=story_scene_snapshot(world) if args.pair_load_probe or args.scenario=='story-local-load' else None,
            **(dict(world=world_snapshot()) if world else {}),
            roster=word(0x7de400),window=window_snapshot(),
            **(dict(effects=snapshot_effects(read_bytes)) if args.watch_effects else {}),
            player=player_snapshot(),**(dict(hud=hud_snapshot()) if args.watch_hud else {}),
            **(dict(network=network_snapshot()) if args.lan_role else {}),
            **(dict(surfaces=surface_snapshot()) if args.watch_surfaces else {}),
            **(dict(native_clock=native_clock_snapshot()) if args.watch_native_clock else {}))
    presentation_frames=set()
    def sample():
        nonlocal next_sample
        elapsed=time.perf_counter()-start
        if elapsed>=next_sample:
            assert len(samples)<2400,'Bounded state-sample limit reached'
            item=dict(seconds=round(elapsed,2),**snapshot());samples.append(item)
            current_pending=run/'current.pending.json'
            current_pending.write_text(json.dumps(item))
            try:current_pending.replace(run/'current.json')
            except PermissionError:pass # A Windows reader may briefly hold it; retain the last complete sample.
            early_story=(item.get('active_mode') in (4,14) and
                item.get('scene') is not None and item['scene']<100 and not item.get('paused') and
                not (item.get('flags',0)&0x20000000))
            if args.story_presentation_probe and early_story and item.get('scene')==2:
                tick=item.get('game_tick',0)
                for target in (60,300,600,900,1200):
                    if target<=tick<target+120 and target not in presentation_frames:
                        capture('opening-auto-'+str(target));presentation_frames.add(target)
            next_sample=elapsed+(.1 if args.pair_load_probe and early_story else
                .5 if args.pair_load_probe else 1)
    def wait_observed(seconds):
        if not args.pair_load_probe:
            time.sleep(seconds);return
        # Capture the first load/spawn/health transition during long command
        # waits, rather than sampling only after Game Over is already visible.
        until=time.monotonic()+seconds
        while time.monotonic()<until:
            if process.poll() is not None:return
            sample();time.sleep(min(.1,max(0,until-time.monotonic())))
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
            # Menu assertions read this log while the game is still running.
            # Neither pipe reads nor file writes may wait for a full 4 KiB.
            while chunk:=pipe.read1(4096):
                out.write(chunk[:left]);out.flush();left=max(0,left-len(chunk))
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
    def cancel_online():
        # Discover only this game's own companion chain: its direct pythonw.exe
        # child and the interpreter a venv launcher redirects to. Do not close
        # another game's browser or inspect command lines or personal
        # configuration.
        class Entry(c.Structure):
            _fields_=[('size',c.c_uint32),('usage',c.c_uint32),('pid',c.c_uint32),
                ('heap',c.c_size_t),('module',c.c_uint32),('threads',c.c_uint32),
                ('parent',c.c_uint32),('priority',c.c_int32),('flags',c.c_uint32),
                ('exe',c.c_wchar*260)]
        kernel.CreateToolhelp32Snapshot.argtypes=[c.c_uint32,c.c_uint32]
        kernel.CreateToolhelp32Snapshot.restype=c.c_void_p
        kernel.Process32FirstW.argtypes=[c.c_void_p,c.POINTER(Entry)]
        kernel.Process32NextW.argtypes=[c.c_void_p,c.POINTER(Entry)]
        until=time.monotonic()+15
        while time.monotonic()<until:
            snap=kernel.CreateToolhelp32Snapshot(2,0)
            assert snap and snap!=c.c_void_p(-1).value
            try:
                records=[]
                entry=Entry();entry.size=c.sizeof(entry)
                more=kernel.Process32FirstW(snap,c.byref(entry))
                while more:
                    records.append((entry.pid,entry.parent,(entry.exe or '').casefold()))
                    more=kernel.Process32NextW(snap,c.byref(entry))
                launchers={pid for pid,parent,exe in records
                    if parent==process.pid and exe.endswith('pythonw.exe')}
                owned=launchers|{pid for pid,parent,exe in records
                    if parent in launchers and exe.endswith('pythonw.exe')}
                for pid in owned:
                    for hwnd,title in ui.windows(pid):
                        if title=='Online Story - TimeSplitters Future Perfect':
                            report.append(dict(online_child=pid,title=title))
                            user.PostMessageW(hwnd,0x10,0,0)
                            return
            finally:kernel.CloseHandle(snap)
            time.sleep(.1)
        raise AssertionError('Story Online companion window was not opened')
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
            if stop_request.exists():break
            assert process.poll() is None,'Unexpected game exit'
            elapsed=time.perf_counter()-start
            sample()
            lines=commands.read_text().splitlines();assert commands.stat().st_size<65536
            if seen<len(lines):
                command=json.loads(lines[seen]);seen+=1
                assert len(command.get('keys',[]))<=16 and 0.1<=command.get('gap',.7)<=4
                hold=command.get('hold_seconds',command.get('hold',.18))
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
                    else:wait_observed(hold)
                    publish_input_state(ROOT,state,'0 0\n')
                    key_events.append(dict(key=key,down_ns=down_ns,up_ns=time.perf_counter_ns()))
                    wait_observed(command.get('gap',.7))
                if command.get('window')=='toggle':user.PostMessageW(window,0x104,13,1<<29)
                if command.get('window')=='focus':
                    user.ShowWindow(window,6);time.sleep(1)
                    lost=user.GetForegroundWindow()!=window
                    user.ShowWindow(window,9);user.SetForegroundWindow(window);time.sleep(1)
                    report.append(dict(focus_lost=lost,focus_restored=user.GetForegroundWindow()==window))
                if command.get('cancel_online'):cancel_online()
                wait_samples=[];wait_started=time.perf_counter();wait_seconds=command.get('wait',2.5)
                if command.get('sample_wait'):
                    while time.perf_counter()-wait_started<wait_seconds:
                        began=time.perf_counter()
                        observation=snapshot()
                        wait_samples.append(dict(seconds=began-start,end_seconds=time.perf_counter()-start,**observation))
                        time.sleep(min(.05,max(0,wait_seconds-(time.perf_counter()-wait_started))))
                else:wait_observed(wait_seconds)
                item=dict(seconds=round(time.perf_counter()-start,3),observed_ns=time.perf_counter_ns(),
                    command=command,key_events=key_events,**snapshot(world=command.get('world_probe',False)),fullscreen=not bool(user.GetWindowLongW(window,-16)&0x00C00000))
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
                if 'expect_menu' in command or command.get('expect_story_difficulty'):
                    trace=(run/f'run{index}.err').read_text(errors='replace')
                    observed=re.findall(r'\[PC-STORY-UI\] descriptor=([0-9A-Fa-f]{8})',
                        trace)
                    expected=command.get('expect_menu')
                    if command.get('expect_story_difficulty'):
                        difficulty=re.findall(r'\[PC-STORY-SELECT\] mission=1 difficulty descriptor=([0-9A-Fa-f]{8})',trace)
                        assert difficulty and 'custom difficulty built' in trace, 'Story difficulty builder was not reached'
                        expected=int(difficulty[-1],16)
                        assert 'custom difficulty built mission=1' in trace, 'Story mission choice was not retained'
                    assert observed and int(observed[-1],16)==expected, (
                        'Unexpected Story menu; stopped before sending another action',command,observed[-1:])
            time.sleep(.1)
    finally:
        close();lock.unlink()
        if directory_server:
            directory_server.shutdown();directory_thread.join(timeout=3);directory_server.server_close()
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
    if args.scenario=='story-entry':
        text=(run/'run0.err').read_text(errors='replace')
        assert text.count('[PC-STORY] Local/Online chooser opened')>=2
        assert '[PC-STORY] Original local Story route selected' in text
        assert any('online_child' in item for item in report)
    if args.scenario=='story-local-load':
        played=[item for item in report if item.get('active_mode') in (4,14) and
            item.get('scene',101)<100 and item.get('actor_pool') and
            not (item.get('flags',0)&0x20000000)]
        assert played,'Local Story gameplay not established; inspect local-*.png and report.json'
        (run/'story-load-baseline.json').write_text(json.dumps(played[-1],indent=2)+'\n')
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
