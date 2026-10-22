"""Bounded, operator-steered integration session on an isolated Test Lab copy.

Append JSON lines to commands.jsonl: keys (virtual-key numbers), label, gap,
window (toggle/focus), restart, or stop. Inputs go to the game's replay hook;
window messages target only the child. Captures are renderer output, not scanout.
The entire session is limited to ten minutes, including process restarts.
"""
import argparse
import ctypes as c
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
    parser.add_argument('--scenario',choices=['weapon-set','story-entry','story-host','story-host-controls','story-pair','story-local-load'])
    parser.add_argument('--pair-role',choices=['host','guest'])
    parser.add_argument('--directory')
    parser.add_argument('--lobby-code')
    parser.add_argument('--pair-load-probe',action='store_true')
    args=parser.parse_args()
    assert not args.pair_load_probe or args.scenario=='story-pair'
    if args.scenario=='story-pair':
        from urllib.parse import urlsplit
        endpoint=urlsplit(args.directory or '')
        assert args.pair_role and endpoint.scheme=='http' and endpoint.hostname=='127.0.0.1'
        assert endpoint.port and not endpoint.username and not endpoint.password
        assert endpoint.path in ('','/') and not endpoint.query and not endpoint.fragment
        if args.pair_role=='guest':
            assert args.lobby_code and len(args.lobby_code)==16 and all(c in '0123456789abcdefABCDEF' for c in args.lobby_code)
    assert args.run_name.replace('-','').isalnum() and len(args.run_name)<48
    assert os.name=='nt'
    assert hashlib.sha256((ROOT/'private/game/default.xbe').read_bytes()).hexdigest()==XBE_SHA256
    run=ROOT/'private/test-saves'/args.run_name;env=prepare_run(ROOT,run)
    stop_request=run/'stop-request'
    assert not stop_request.is_symlink()
    stop_request.unlink(missing_ok=True)
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
            dict(keys=[69],label='local-game',wait=5),dict(stop=True)]
        commands.write_text(''.join(json.dumps(step)+'\n' for step in route))
    env['TSFP_ONLINE_SETTINGS']=str(run/'online-settings.json')
    # Typed Story now supplies normal start behavior. Instrumentation must not
    # enable a hidden feature switch that an ordinary player does not receive.
    env.pop('TSFP_STORY_LOAD_PROBE',None)
    directory_server=None;directory_thread=None
    if args.scenario in ('story-host','story-host-controls','story-pair'):
        env['TSFP_STORY_TRACE']='1'
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
    state=run/'input-state.txt';state.write_text('0 0\n')
    initial=[(5000,69),(10000,69),(14000,40),(18000,69)]
    replay=run/'input.txt';replay.write_text('\n'.join(f'{at+d} key {k} {v}' for at,k in initial for d,v in [(0,1),(180,0)])+'\n')
    env.update(TSFP_PC_INPUT_REPLAY=str(replay),TSFP_PC_INPUT_STATE=str(state),
        RECOMP_WINDOW_BACKGROUND='1',RECOMP_PC_MOUSE='1',TSFP_DISCORD_CONFIG=str(run/'discord.ini'),
        RECOMP_HLE_D3D8_DUMP=str(run/'frame'),RECOMP_HLE_D3D8_DUMP_EVERY='120',
        RECOMP_HLE_D3D8_DUMP_KEEP_LAST='3',RECOMP_FPS='5',RECOMP_INTERP_DEBUG='1')
    def profiles():
        result={}
        for profile in (run/'saves/UserData/45410066').glob('*/tsftrpft'):
            raw=profile.read_bytes()
            assert len(raw)==BODY_SIZE+20 and signature(raw[:BODY_SIZE])==raw[BODY_SIZE:]
            result[profile.parent.name]=hashlib.sha256(raw).hexdigest()
        assert result
        return result
    original=profiles();report=[];samples=[];threads=[];process=None;handle=None
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
    def snapshot():
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
        if pool and args.pair_load_probe:
            for slot in range(min(word(0x790950) or 0,4)):
                actor=pool+slot*0xea0;entity=word(actor)
                # Existing bot replication uses +64 for world position;
                # +1a8 is collision-bounds centre, not movement telemetry.
                position=[real(entity+0x64+i*4) for i in range(3)] if entity else []
                actors.append(dict(slot=slot,entity=entity,position=position,
                    # Field contracts used by pc_network_bots_guest_shot and
                    # its snapshot sender; weapon IDs are not ammo counters.
                    weapons=[word(actor+0xd4+hand*0x130) for hand in range(2)],
                    aim=[real(actor+0xb88+i*4) for i in range(3)],
                    muzzle=[real(actor+0xbb4+i*4) for i in range(3)],
                    health=real(entity+0x1cc) if entity else None,
                    entity_flags=word(entity+0x28) if entity else None,
                    life=[word(actor+0x10+i*4) for i in range(4)],
                    actor_flags=[word(actor+0x24+i*4) for i in range(3)]))
        return dict(pid=process.pid,scene=word(0x79094C),flags=word(0x7DE458),
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
            frontend_config=config(0x78AD58),active_config=config(0x7DE400),actors=actors)
    def sample():
        nonlocal next_sample
        elapsed=time.monotonic()-start
        if elapsed>=next_sample:
            assert len(samples)<2400,'Bounded state-sample limit reached'
            item=dict(seconds=round(elapsed,2),**snapshot());samples.append(item)
            (run/'current.json').write_text(json.dumps(item))
            early_story=(item.get('active_mode') in (4,14) and
                item.get('scene') is not None and item['scene']<100 and not item.get('paused'))
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
    def drain(pipe,path):
        with path.open('wb') as out:
            left=4*1024*1024
            # Menu assertions read this log while the game is still running.
            # Neither pipe reads nor file writes may wait for a full 4 KiB.
            while chunk:=pipe.read1(4096):
                out.write(chunk[:left]);out.flush();left=max(0,left-len(chunk))
    def launch(index):
        nonlocal process,handle,window
        state.write_text('0 0\n')
        process=subprocess.Popen([str(ROOT/'build/windows/Release/futureperfect_recomp.exe')],cwd=ROOT,env=env,
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
        handle=kernel.OpenProcess(0x410,False,process.pid);assert handle
        for pipe,ext in [(process.stdout,'out'),(process.stderr,'err')]:
            t=threading.Thread(target=drain,args=(pipe,run/f'run{index}.{ext}'),daemon=True);t.start();threads.append(t)
        window=ui.wait_window(process.pid,'TimeSplitters FP')
        print('Child',index,process.pid,flush=True)
    def close():
        nonlocal handle
        if process and process.poll() is None:
            user.PostMessageW(window,0x10,0,0)
            try:process.wait(10)
            except subprocess.TimeoutExpired:process.terminate();process.wait(10)
        if handle:kernel.CloseHandle(handle);handle=None
        for t in threads:t.join(5)
        threads.clear()
    def capture(label):
        assert label.replace('-','').isalnum() and len(label)<48
        assert (run/(label+'.png')).exists() or len(list(run.glob('*.png')))<48
        for _ in range(20):
            frames=sorted(run.glob('frame*.bmp'),key=lambda p:p.stat().st_mtime_ns)
            if frames:
                try:
                    with Image.open(frames[-1]) as im:im.save(run/(label+'.png'))
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
    window=None;index=0;seen=0;start=time.monotonic();next_sample=0
    lock=run/'run.lock'
    with lock.open('x') as f:f.write(str(os.getpid()))
    try:
        launch(index)
        while time.monotonic()-start<600:
            if stop_request.exists():break
            assert process.poll() is None,'Unexpected game exit'
            elapsed=time.monotonic()-start
            sample()
            lines=commands.read_text().splitlines();assert commands.stat().st_size<65536
            if seen<len(lines):
                command=json.loads(lines[seen]);seen+=1
                assert len(command.get('keys',[]))<=16 and 0.1<=command.get('gap',.7)<=4
                hold=command.get('hold_seconds',.18);assert .1<=hold<=3
                assert 0<=command.get('wait',2.5)<=30
                budget=len(command.get('keys',[]))*(hold+command.get('gap',.7))+command.get('wait',2.5)
                if elapsed+budget+(22 if command.get('restart') else 0)>600:break
                if command.get('stop'):break
                if command.get('restart'):
                    before=profiles();close()
                    report.append(dict(restart_saved_profiles=profiles(),before_exit_profiles=before,exit_code=process.returncode))
                    assert process.returncode==0,'Game did not exit cleanly'
                    index+=1;assert index<=3;launch(index)
                for key in command.get('keys',[]):
                    state.write_text(f'{key} 1\n');wait_observed(hold);state.write_text('0 0\n');wait_observed(command.get('gap',.7))
                if command.get('window')=='toggle':user.PostMessageW(window,0x104,13,1<<29)
                if command.get('window')=='focus':
                    user.ShowWindow(window,6);time.sleep(1)
                    lost=user.GetForegroundWindow()!=window
                    user.ShowWindow(window,9);user.SetForegroundWindow(window);time.sleep(1)
                    report.append(dict(focus_lost=lost,focus_restored=user.GetForegroundWindow()==window))
                if command.get('cancel_online'):cancel_online()
                wait_observed(command.get('wait',2.5))
                item=dict(seconds=round(time.monotonic()-start,2),command=command,**snapshot(),fullscreen=not bool(user.GetWindowLongW(window,-16)&0x00C00000))
                if 'label' in command:capture(command['label'])
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
        assert '[CRASH]' not in text and '[ICALL] unresolved' not in text,log
    assert process.returncode==0,'Final game exit was not clean'
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
    print('PASS: bounded session completed; inspect report and captures for scenario coverage',flush=True)

if __name__=='__main__':main()
