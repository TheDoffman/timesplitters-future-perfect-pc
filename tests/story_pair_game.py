"""Bounded Story lobby precondition, or explicit experimental --load-probe.

Uses the production Story entry and companion on two disposable Test Lab copies,
one loopback directory, and independent bridge ports/settings/logs. Review the
two pair-wait.png captures as well as the numeric roster/role evidence. Only
--load-probe sends GO and movement; its success still requires visual review and
does not certify Story scripts/NPC authority or the complete feature checklist.
"""
import hashlib
import argparse
import json
import math
import os
import shutil
from pathlib import Path
import subprocess
import sys
import threading
import time
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from lobby_server import Server
from online_client import Client


def read_json(path,default=None):
    try:return json.loads(path.read_text(encoding='utf-8'))
    except (OSError,ValueError):return default


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--load-probe',action='store_true',help='Opt-in experimental Story start and movement check')
    parser.add_argument('--lobby-controls',action='store_true',help='Select Scotland/Hard, verify guest settings and own Progress before loading')
    parser.add_argument('--lobby-only',action='store_true',help='Stop after changed lobby controls; no GO/load/movement')
    parser.add_argument('--players',type=int,choices=[2,4],default=2,help='Actual instances/players admitted to the isolated lobby')
    parser.add_argument('--fire-probe',action='store_true',help='Bounded primary-fire inputs and captures; visual interaction review still required')
    parser.add_argument('--script-observe-seconds',type=int,default=0,choices=range(31),metavar='0..30',help='Passive diagnostic wait if a peer is script-suspended; does not replace failed startup evidence')
    parser.add_argument('--receipt-id',help='Caller nonce for interpreter-independent receipt identity')
    args=parser.parse_args()
    assert not args.lobby_controls or args.load_probe
    assert not args.lobby_only or args.lobby_controls
    assert not args.fire_probe or (args.load_probe and not args.lobby_only)
    assert not args.script_observe_seconds or (args.load_probe and not args.lobby_only)
    assert args.players==2 or args.load_probe
    roles=['host','guest']+(['guest2','guest3'] if args.players==4 else [])
    if args.receipt_id:
        assert len(args.receipt_id)==32 and all(c in '0123456789abcdef' for c in args.receipt_id)
    assert os.name=='nt','Windows native transport required'
    seal=read_json(ROOT/'build/windows/generated/network-build.json')
    exe=ROOT/'build/windows/Release/futureperfect_recomp.exe'
    assert seal and hashlib.sha256(exe.read_bytes()).hexdigest()==seal['executable_sha256']
    identity=read_json(ROOT/'build/windows/generated/network-identity.json')
    assert identity and identity['compatibility']==seal['compatibility']
    for name,digest in identity['files']:
        if name.startswith(('src/pc_story','src/pc_network','src/pc_online','src/pc_display_menu')) or name=='tools/online.py':
            assert hashlib.sha256((ROOT/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest, 'Rebuild required: '+name
    evidence_root=ROOT/'private/network-validation'/('story-four' if args.players==4 else 'story-load-pair' if args.load_probe else 'story-pair')
    evidence_root.mkdir(parents=True,exist_ok=True)
    # Each child owns a different run lock and every save/settings path. Never
    # reset either profile if another runner is active in that directory.
    runs={role:ROOT/'private/test-saves'/(('online-story-four-' if args.players==4 else 'online-story-load-pair-' if args.load_probe else 'online-story-pair-')+role) for role in roles}
    assert all(not (run/'run.lock').exists() for run in runs.values()),'Pair runner already active; inspect its PID'
    result=dict(scope='Story lobby precondition only; no GO or gameplay',status='running',
        build=seal,parent_pid=os.getpid(),processes={},admitted_players=args.players)
    if args.receipt_id:result['receipt_id']=args.receipt_id
    if args.load_probe:result['scope']='Experimental Story synchronized load and two actor movement; not full goal acceptance'
    if args.lobby_only:result['scope']='Story lobby controls and visual layout only; no GO or gameplay'
    if args.load_probe:
        # A later build may replace the regular executable/PDB archive. Retain
        # the EXACT sealed pair before spawning either process, not after crash.
        archive=ROOT/'build/windows/crash-symbols.zip'
        with zipfile.ZipFile(archive) as bundle:
            symbols=json.loads(bundle.read('BUILD.json'))
            assert symbols['files']['futureperfect_recomp.exe']==seal['executable_sha256'],'Crash symbols do not match probe build'
        saved=evidence_root/'probe-symbols.zip'
        shutil.copyfile(archive,saved)
        result['symbols_archive']=saved.relative_to(ROOT).as_posix()
        result['symbols_archive_sha256']=hashlib.sha256(saved.read_bytes()).hexdigest()
    record=evidence_root/'pair-evidence.json'
    record.write_text(json.dumps(result,indent=2)+'\n')
    directory=Server(('127.0.0.1',0))
    thread=threading.Thread(target=directory.serve_forever,daemon=True);thread.start()
    address=f'http://127.0.0.1:{directory.server_port}'
    client=Client(address)
    children={};streams=[]
    deadline=time.monotonic()+(330 if args.load_probe else 210)

    def launch(role,code=None):
        command=[sys.executable,str(ROOT/'tests/stability_transitions_game.py'),
            '--run-name',runs[role].name,'--scenario','story-pair','--pair-role',('host' if role=='host' else 'guest'),'--directory',address]
        if code:command+=['--lobby-code',code]
        if args.load_probe:command+=['--pair-load-probe']
        output=(evidence_root/(role+'-runner.log')).open('wb');streams.append(output)
        child=subprocess.Popen(command,cwd=ROOT,stdout=output,stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW)
        children[role]=child;result['processes'][role]=dict(runner_pid=child.pid,run=runs[role].relative_to(ROOT).as_posix())
        record.write_text(json.dumps(result,indent=2)+'\n')
        print(f'{role} runner PID {child.pid}',flush=True)

    def wait_for(check,description):
        while time.monotonic()<deadline:
            for role,child in children.items():
                assert child.poll() is None,f'{role} runner exited {child.returncode}; inspect {role}-runner.log'
            value=check()
            if value:return value
            time.sleep(.2)
        raise TimeoutError(description)

    def append(role,**step):
        with (runs[role]/'commands.jsonl').open('a',encoding='utf-8') as stream:
            stream.write(json.dumps(step)+'\n')

    def captured(role,label):
        return next((r for r in reversed(read_json(runs[role]/'report.json',[]))
                     if r.get('command',{}).get('label')==label),None)

    try:
        launch('host')
        def host_ready():
            state=read_json(runs['host']/'current.json',{})
            if state.get('network_phase')!=11 or state.get('role')!=1:return None
            reply=client.request('GET','/v2/lobbies?compatibility='+seal['compatibility']+'&mode=story')
            lobbies=reply['lobbies']
            assert len(lobbies)<=1,'Unexpected lobby on isolated directory'
            return lobbies[0]['code'] if lobbies else None
        code=wait_for(host_ready,'Host did not create its Story lobby')
        result['directory_lobby_code']=code
        for role in roles[1:]:launch(role,code)
        if args.load_probe:
            def roster_present():
                states={role:read_json(run/'current.json',{}) for role,run in runs.items()}
                return states if all(s.get('player_slots',0)>=args.players for s in states.values()) else None
            wait_for(roster_present,'Both original network rosters did not join')
            for role in runs:append(role,label='pair-wait',wait=3)
        def joined():
            states={role:read_json(run/'current.json',{}) for role,run in runs.items()}
            phases={role:(11 if role=='host' else 12) for role in roles}
            if any(s.get('network_phase')!=phases[role] or s.get('player_slots',0)<args.players for role,s in states.items()):return None
            if any(states[role].get('role')!=(1 if role=='host' else 2) for role in roles):return None
            # Each retained capture must come from a settled step observing two
            # slots, not an old PNG left by a previous run or startup frame.
            for role,run in runs.items():
                reports=read_json(run/'report.json',[])
                if not any(r.get('command',{}).get('label')=='pair-wait' and r.get('player_slots',0)>=args.players
                           and r.get('network_phase')==phases[role] for r in reports):return None
            return states
        result['joined_states']=wait_for(joined,'Both Story game instances did not reach the shared two-player lobby')
        result['status']='lobby-precondition-passed'
        if args.load_probe:
            if args.lobby_controls:
                append('host',keys=[89],label='story-options',wait=4)
                wait_for(lambda:captured('host','story-options'),'Host Options did not complete')
                append('host',keys=[40,69],label='story-difficulty',wait=4)
                wait_for(lambda:captured('host','story-difficulty'),'Host level choice did not complete')
                append('host',keys=[40,40,69],label='story-settings-return',wait=4)
                selected=wait_for(lambda:captured('host','story-settings-return'),'Host difficulty choice did not complete')
                expected=selected['frontend_config']
                assert expected['scene']<100 and expected['rule']==10 and expected['difficulty']==2,selected
                def replicated():
                    guests=[read_json(runs[role]/'current.json',{}).get('frontend_config',{}) for role in roles[1:]]
                    return guests[0] if all(all(g.get(k)==expected[k] for k in ('scene','rule','variant','difficulty')) for g in guests) else None
                result['replicated_lobby_settings']=wait_for(replicated,'Guest did not receive host Story selection')
                append('guest',keys=[82],label='guest-progress',wait=4)
                wait_for(lambda:captured('guest','guest-progress'),'Guest Progress input not completed')
                append('guest',keys=[70],label='guest-progress-return',wait=4)
                returned=wait_for(lambda:captured('guest','guest-progress-return'),'Guest Progress Back input not completed')
                assert returned['network_phase']==12 and returned['player_slots']==args.players,returned
            if args.lobby_controls:
                append('guest',keys=[69],label='guest-confirm-ignored',wait=3)
                ignored=wait_for(lambda:captured('guest','guest-confirm-ignored'),'Guest authority input not completed')
                assert ignored['network_phase']==12 and ignored['scene']==101,ignored
            else:
                append('guest',keys=[69],label='guest-ready',wait=3)
                wait_for(lambda:captured('guest','guest-ready'),'Guest ready input not completed')
            if args.lobby_only:
                result['status']='lobby-controls-passed'
            else:
                append('host',keys=[69],label='story-go',wait=25)
                wait_for(lambda:captured('host','story-go'),'Host GO input not completed')
                # Direct gameplay load has no cinematic to skip. Sending E here
                # retried the Game Over menu and obscured the initial failure.
                for role in runs:append(role,label='load-before',wait=8)
                before=wait_for(lambda:{role:captured(role,'load-before') for role in runs}
                    if all(captured(role,'load-before') for role in runs) else None,'Missing pre-movement captures')
                result['loaded_states']=before
                if args.script_observe_seconds and any(s.get('result_flags',0)&0x02000000 or 256 in s.get('context_states',[]) for s in before.values()):
                    # Observe whether the original restore runs naturally. Keep
                    # the original settled-start assertions/evidence unchanged.
                    for role in runs:append(role,label='script-observe',wait=args.script_observe_seconds)
                    result['script_wait_states']=wait_for(lambda:{role:captured(role,'script-observe') for role in runs}
                        if all(captured(role,'script-observe') for role in runs) else None,'Script observation captures missing')
                for role,s in before.items():
                    assert s['active_mode']==4 and s['network_phase']==15, (role,'One-view Story/network phase',s)
                    assert s['paused']==0,(role,'Story paused or failure menu before movement; inspect load-before.png and states.json',s)
                    assert s.get('context_count')==args.players and len(s.get('context_states',[]))==args.players and all(
                        value==2 for value in s['context_states']), (role,'player context not active',s)
                    assert s.get('result_flags')==0,(role,'Story result flags before movement',s)
                    assert s['scene']<100 and s['active_config']['players']==args.players and s['active_config']['local_players']==1,(role,s)
                    assert len(s['actors'])==args.players and all(a['entity'] for a in s['actors']),(role,'missing actor',s)
                    assert len({a['entity'] for a in s['actors']})==args.players,(role,'aliased actors',s)
                    assert all(a.get('health') is not None and math.isfinite(a['health']) and a['health']>0 for a in s['actors']),(role,'nonliving actor before movement',s)
                assert before['host']['scene']==before['guest']['scene'],'Story scenes differ'
                if args.lobby_controls:
                    for role,s in before.items():
                        assert all(s['active_config'][k]==expected[k] for k in ('scene','rule','variant','difficulty')),(role,'Loaded selection differs from host lobby',s)
                for role,key in zip(roles,[87,68,83,65]):
                    append(role,keys=[key],hold_seconds=1,label='move-own-player',wait=2)
                wait_for(lambda:all(captured(role,'move-own-player') for role in runs),'Movement input not completed')
                for role in runs:append(role,label='load-after',wait=4)
                after=wait_for(lambda:{role:captured(role,'load-after') for role in runs}
                    if all(captured(role,'load-after') for role in runs) else None,'Missing post-movement captures')
                result['after_movement']=after
                for role in runs:
                    assert after[role]['scene']==before[role]['scene'] and after[role]['network_phase']==15,(role,after[role])
                    assert not after[role]['paused'] and after[role]['context_states']==[2]*args.players and after[role]['result_flags']==0,(role,'Story not active after movement',after[role])
                    for slot in range(args.players):
                        a=before[role]['actors'][slot]['position'];b=after[role]['actors'][slot]['position']
                        assert len(a)==len(b)==3 and all(v is not None and math.isfinite(v) for v in a+b),(role,slot,'invalid position')
                        assert sum((x-y)**2 for x,y in zip(a,b))>.01,(role,slot,'actor did not move')
                        # Inputs have ended and the capture waits four seconds;
                        # allow one world unit for residual interpolation/contact.
                        host_position=after['host']['actors'][slot]['position']
                        assert sum((x-y)**2 for x,y in zip(b,host_position))<=1.,(role,slot,'Peer positions diverged',b,host_position)
                if args.fire_probe:
                    for role in runs:
                        # Mouse 1 is the common input model's default primary
                        # fire binding, delivered through the isolated state file.
                        append(role,keys=[1],hold_seconds=1,label='fire-own-player',wait=1)
                    fired=wait_for(lambda:{role:captured(role,'fire-own-player') for role in runs}
                        if all(captured(role,'fire-own-player') for role in runs) else None,'Firing captures missing')
                    result['after_fire_input']=fired
                    result['fire_scope']='Input delivered and captures saved; firing, hits and world synchronization require separate evidence'
                    for role,s in fired.items():
                        assert s['scene']==before[role]['scene'] and s['network_phase']==15 and not s['paused'],(role,'Lost live Story during firing input',s)
                result['status']='load-probe-passed'
    except BaseException as error:
        result['status']='failed';result['error']=str(error)
        raise
    finally:
        # Signal runners, then let their existing finally blocks close ONLY
        # their own game/companion and recycle their known disposable disks.
        for role,child in children.items():
            if child.poll() is None and runs[role].exists():(runs[role]/'stop-request').write_text('stop\n')
        for role,child in children.items():
            try:child.wait(timeout=70)
            except subprocess.TimeoutExpired:
                result['status']='cleanup-pending';result['error']='Runner still live: '+role
            result['processes'][role]['exit_code']=child.poll()
            if child.poll() not in (None,0):result['status']='failed'
        # Preserve handles/PIDs in the evidence if shutdown is not terminal;
        # never kill unrelated Python/game processes or reset active profiles.
        directory.shutdown();thread.join(timeout=3);directory.server_close()
        for output in streams:output.close()
        record.write_text(json.dumps(result,indent=2)+'\n')
    assert result['status']==('lobby-controls-passed' if args.lobby_only else 'load-probe-passed' if args.load_probe else 'lobby-precondition-passed'),result
    if args.lobby_only:
        print('PASS: bounded Story lobby controls; REVIEW fresh lobby captures; no gameplay checked.');return
    print('PASS: experimental Story load and actor movement; REVIEW both load-before/after.png; full goal NOT certified.'
          if args.load_probe else 'PASS: two-instance Story lobby precondition; inspect both pair-wait.png captures. Story load/play NOT tested.')


if __name__=='__main__':main()
