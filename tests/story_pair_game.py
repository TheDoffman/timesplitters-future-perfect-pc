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
import re
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


def npc_health_agrees(host,peer):
    """Retired bodies and nonpositive health are equivalent only after death."""
    if host is not None and not math.isfinite(host):return False
    if peer is not None and not math.isfinite(peer):return False
    if host is None or host<=0:return peer is None or peer<=0
    return peer is not None and abs(peer-host)<=.05


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--load-probe',action='store_true',help='Opt-in experimental Story start and movement check')
    parser.add_argument('--lobby-controls',action='store_true',help='Select mission/difficulty, verify guest settings and own Progress before loading')
    parser.add_argument('--mission',type=int,choices=range(13),default=0,help='Story mission index; 0 is Time to Split')
    parser.add_argument('--difficulty',type=int,choices=range(3),default=1,help='0 Easy, 1 Normal, 2 Hard')
    parser.add_argument('--lobby-only',action='store_true',help='Stop after changed lobby controls; no GO/load/movement')
    parser.add_argument('--players',type=int,choices=[2,4],default=2,help='Actual instances/players admitted to the isolated lobby')
    parser.add_argument('--fire-probe',action='store_true',help='Check first-level firearm and ammunition consumption; save captures for interaction review')
    parser.add_argument('--combat-probe',action='store_true',help='After firearm checks, walk an added guest forward and test host acceptance of a hit on a live enemy')
    parser.add_argument('--restart-probe',action='store_true',help='After opening/fire checks, test host-only level restart and fresh handoff on every peer')
    parser.add_argument('--skip-intro',action='store_true',help='Diagnostic host skip after fresh intro captures; default lets the first-level intro finish naturally')
    parser.add_argument('--script-observe-seconds',type=int,default=0,choices=range(31),metavar='0..30',help='Passive diagnostic wait if a peer is script-suspended; does not replace failed startup evidence')
    parser.add_argument('--receipt-id',help='Caller nonce for interpreter-independent receipt identity')
    args=parser.parse_args()
    assert not args.lobby_controls or args.load_probe
    assert not args.lobby_only or args.lobby_controls
    assert not args.fire_probe or (args.load_probe and not args.lobby_only)
    assert not args.restart_probe or (args.fire_probe and args.mission==0 and not args.lobby_only)
    assert not args.combat_probe or (args.fire_probe and args.mission==0 and args.players==4)
    assert not args.script_observe_seconds or (args.load_probe and not args.lobby_only)
    assert not args.skip_intro or (args.load_probe and args.mission==0 and not args.lobby_only)
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
        if name.startswith(('src/pc_story','src/pc_network','src/pc_online','src/pc_display_menu','src/pc_character_pose.h')) or name=='tools/online.py':
            assert hashlib.sha256((ROOT/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest, 'Rebuild required: '+name
    evidence_root=ROOT/'private/network-validation'/('story-four' if args.players==4 else 'story-load-pair' if args.load_probe else 'story-pair')
    evidence_root.mkdir(parents=True,exist_ok=True)
    # Each child owns a different run lock and every save/settings path. Never
    # reset either profile if another runner is active in that directory.
    runs={role:ROOT/'private/test-saves'/(('online-story-four-' if args.players==4 else 'online-story-load-pair-' if args.load_probe else 'online-story-pair-')+role) for role in roles}
    assert all(not (run/'run.lock').exists() for run in runs.values()),'Pair runner already active; inspect its PID'
    result=dict(scope='Story lobby precondition only; no GO or gameplay',status='running',
        build=seal,parent_pid=os.getpid(),processes={},admitted_players=args.players,
        requested_mission=args.mission,requested_difficulty=args.difficulty)
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
    deadline=time.monotonic()+(600 if args.restart_probe else 450 if args.load_probe and args.mission==0 and not args.skip_intro else 330 if args.load_probe else 210)

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
                append('host',keys=[40]*args.mission+[69],label='story-difficulty',wait=4)
                wait_for(lambda:captured('host','story-difficulty'),'Host level choice did not complete')
                append('host',keys=[40]*args.difficulty+[69],label='story-settings-return',wait=4)
                selected=wait_for(lambda:captured('host','story-settings-return'),'Host difficulty choice did not complete')
                expected=selected['frontend_config']
                assert expected['scene']<100 and expected['rule']==10 and expected['difficulty']==args.difficulty,selected
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
                append('host',keys=[69],label='story-go',wait=8)
                wait_for(lambda:captured('host','story-go'),'Host GO input not completed')
                if args.mission==0:
                    def intro_loaded():
                        states={role:read_json(run/'current.json',{}) for role,run in runs.items()}
                        return states if all(s.get('flags',0)&0x20000000 and s.get('network_phase') in (10,14,15)
                                             for s in states.values()) else None
                    wait_for(intro_loaded,'All peers did not enter the original first-level intro')
                    for role in runs:append(role,label='intro-before',wait=2,world_probe=True)
                    intros=wait_for(lambda:{role:captured(role,'intro-before') for role in runs}
                        if all(captured(role,'intro-before') for role in runs) else None,'Missing intro captures')
                    assert all(s['flags']&0x20000000 and s['scene']==intros['host']['scene'] for s in intros.values()),intros
                    result['intro_states']=intros
                    if args.skip_intro:
                        # Only skip after observing a real cinematic; never retry
                        # a failure menu or obscure an initial loading fault.
                        append('host',keys=[69],label='intro-host-handoff',wait=20)
                        wait_for(lambda:captured('host','intro-host-handoff'),'Host intro handoff did not complete')
                    else:
                        for role in runs:append(role,label='intro-progress',wait=25)
                        result['intro_progress']=wait_for(lambda:{role:captured(role,'intro-progress') for role in runs}
                            if all(captured(role,'intro-progress') for role in runs) else None,'Missing long-intro captures')
                    def gameplay_after_intro():
                        states={role:read_json(run/'current.json',{}) for role,run in runs.items()}
                        if not all(states.values()):return None
                        assert all(s.get('network_enabled') and s.get('role') in (1,2) for s in states.values()),('Peer disconnected during Story intro',states)
                        return states if all(not s.get('flags',0)&0x20000000 and s.get('scene',101)<100 and
                                             s.get('network_phase')==15 for s in states.values()) else None
                    result['first_gameplay_states']=wait_for(gameplay_after_intro,'Host intro did not hand off every peer to gameplay')
                    # Retain world identities/positions even if the scripted opening stalls.
                    for role in runs:
                        append(role,label='opening-world',wait=8,world_probe=True)
                        append(role,label='opening-later',wait=20,world_probe=True)
                    # The original first-level opening has its own scripted
                    # firearm handoff. Observe it, without forcing control flags.
                    opening_deadline=min(deadline,time.monotonic()+60)
                    def opening_ready():
                        states={role:read_json(run/'current.json',{}) for role,run in runs.items()}
                        if not all(states.values()):return None
                        assert all(s.get('game_state')!=8 and not (s.get('result_flags',0)&0x10000) and
                                   s.get('network_phase')==15 for s in states.values()),('Opening failed or left the network',states)
                        ready=all(s.get('context_states')==[2]*args.players and s.get('result_flags')==0 and
                                  (s.get('local_player') or {}).get('equipped',[None,None])[1]==14 for s in states.values())
                        assert ready or time.monotonic()<opening_deadline,('Original opening firearm/control handoff did not complete in 60 seconds',states)
                        return states if ready else None
                    result['opening_ready_states']=wait_for(opening_ready,'Original Story opening did not become playable')
                for role in runs:append(role,label='load-before',wait=8,world_probe=True)
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
                assert all(s['scene']==before['host']['scene'] for s in before.values()),'Story scenes differ'
                if args.lobby_controls:
                    for role,s in before.items():
                        assert all(s['active_config'][k]==expected[k] for k in ('scene','rule','variant','difficulty')),(role,'Loaded selection differs from host lobby',s)
                for role,s in before.items():
                    assert sum(a.get('local_port')==0 for a in s['actors'])==1,(role,'Expected one owned input context',s)
                    assert all(c.get('team')==1 for c in s.get('world',{}).get('characters',[]) if c.get('context')),(role,'Story humans lost their original shared team',s.get('world'))
                # Local first-level reference shows W blocked against the initial
                # wreck, while S produces real movement. Use the verified escape
                # direction one owner at a time; keep real displacement checks.
                result['movement_steps']={}
                for role,key in zip(roles,[83]*len(roles) if args.mission==0 else [87,68,83,65]):
                    append(role,keys=[key],hold_seconds=1,sample_hold=True,label='move-own-player',wait=2)
                    step=wait_for(lambda:captured(role,'move-own-player'),'Owned movement input not completed')
                    result['movement_steps'][role]=step
                wait_for(lambda:all(captured(role,'move-own-player') for role in runs),'Movement input not completed')
                for role in runs:append(role,label='load-after',wait=4,world_probe=True)
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
                    def ammo_total(player):
                        kinds={}
                        for slot in (player or {}).get('ammo',[]):
                            value=kinds.setdefault(slot['type'],[slot['reserve'],0])
                            value[1]+=slot['loaded']
                        return sum(sum(value) for value in kinds.values()) if kinds else None
                    if args.mission==0:
                        for role,s in after.items():
                            local=s.get('local_player') or {}
                            assert local.get('equipped',[None,None])[1]==14,(role,'Opening firearm handoff incomplete',local)
                            assert ammo_total(local) is not None,(role,'Opening ammunition unavailable',local)
                            assert ammo_total(local)==ammo_total(after['host'].get('local_player')),(role,'Opening handoff duplicated or lost ammunition',local)
                    for role in runs:
                        # Mouse 1 is the common input model's default primary
                        # fire binding, delivered through the isolated state file.
                        append(role,keys=[1],hold_seconds=1,label='fire-own-player',wait=1,world_probe=True)
                    fired=wait_for(lambda:{role:captured(role,'fire-own-player') for role in runs}
                        if all(captured(role,'fire-own-player') for role in runs) else None,'Firing captures missing')
                    result['after_fire_input']=fired
                    result['fire_scope']='First-level owned firearm and ammunition consumption checked; hits and world synchronization need separate evidence'
                    for role,s in fired.items():
                        if args.mission==0:
                            remaining=ammo_total(s.get('local_player'))
                            assert remaining is not None and remaining<ammo_total(after[role].get('local_player')),(role,'No firing ammunition consumed',s.get('local_player'))
                        assert s['scene']==before[role]['scene'] and s['network_phase']==15 and not s['paused'],(role,'Lost live Story during firing input',s)
                if args.restart_probe:
                    def restart_capture(role,label,**step):
                        append(role,label=label,world_probe=True,**step)
                        return wait_for(lambda:captured(role,label),'Missing restart capture '+label)
                    # Guest Start opens only its own pause menu; the host owns
                    # reload. Capture both menus for visual authority review.
                    restart_capture('guest','guest-pause',keys=[27],wait=2)
                    restart_capture('guest','guest-unpause',keys=[27],wait=2)
                    result['restart_host_pause']=restart_capture('host','host-pause',keys=[27],wait=2)
                    # Restart Level is the penultimate row, before Quit to Desktop.
                    # Original confirmation defaults to No; Up explicitly selects Yes.
                    restart_capture('host','host-restart-confirm',keys=[38,38,69],wait=2)
                    restart_capture('host','host-restart-yes',keys=[38,69],wait=2)
                    restart_deadline=time.monotonic()+80
                    prior_ticks={role:s['game_tick'] for role,s in fired.items()}
                    def restarted():
                        states={role:read_json(run/'current.json',{}) for role,run in runs.items()}
                        ready=all(s.get('scene')==2 and s.get('network_phase')==15 and
                            s.get('game_state')==7 and s.get('context_states')==[2]*args.players and
                            s.get('result_flags')==0 and s.get('game_tick',2**32)<prior_ticks[role] and
                            (s.get('local_player') or {}).get('equipped',[None,None])[1]==14 and
                            '[PC-STORY-RESTART]' in (runs[role]/'run0.err').read_text(errors='replace')
                            for role,s in states.items())
                        assert ready or time.monotonic()<restart_deadline,('Coordinated Story level restart did not become playable',states)
                        return states if ready else None
                    result['restart_ready_states']=wait_for(restarted,'Story reload did not finish')
                    result['restart_after']={role:restart_capture(role,'restart-after',wait=2) for role in roles}
                    for role,s in result['restart_after'].items():
                        assert ammo_total(s.get('local_player'))==ammo_total(before['host'].get('local_player')),(role,'Reload lost or duplicated opening ammo',s)
                if args.combat_probe:
                    # Inputs only: do not manufacture targets, teleport actors or
                    # write health. Preserve a failed search as missing evidence.
                    def extra_slot(s):
                        pointer=(s.get('local_player') or {}).get('player')
                        return next((a['slot'] for a in s['actors'] if a.get('context')==pointer),-1)
                    shooter=next(role for role in roles if role!='host' and extra_slot(fired[role])>=2)
                    shooter_slot=extra_slot(fired[shooter]);result['combat_shooter']=dict(role=shooter,slot=shooter_slot)
                    def command_capture(role,label,**step):
                        append(role,label=label,world_probe=True,**step)
                        return wait_for(lambda:captured(role,label),'Missing combat capture '+label)
                    current=command_capture(shooter,'combat-before',wait=.3)
                    target=None;walk=[]
                    # The old straight-forward route stopped against the north
                    # canyon wall. Turn through the opening occupied by the
                    # authored soldiers before approaching the far NPC. These
                    # are ordinary input waypoints, never position writes.
                    waypoints=[(0,44),(18,50),(18,32),(30,21),(45,12),(63,0)]
                    waypoint=0
                    controls=read_json(runs[shooter]/'controls.json')['mouse'];sensitivity=controls['sensitivity']
                    for index in range(24):
                        world=current.get('world',{});npcs=[c for c in world.get('characters',[]) if not c.get('context')]
                        for c in npcs:
                            assert c.get('lookup_entities') and c['lookup_entities'][0]==c['entity'],('Original NPC ID decoder resolves a different entity',shooter,c)
                        enemies=[c for c in npcs if c.get('kind',[None])[0]==91 and c.get('health',0)>0 and
                                 (c.get('visibility_flags',0)&0x10) and math.dist(current['local_player']['position'],c['position'])<=10]
                        if enemies:
                            target=min(enemies,key=lambda c:math.dist(current['local_player']['position'],c['position']));break
                        if index==23:break
                        local=current['local_player'];position=local['position']
                        while waypoint<len(waypoints)-1 and math.dist((position[0],position[2]),waypoints[waypoint])<6:
                            waypoint+=1
                        point=waypoints[waypoint]
                        yaw=math.degrees(math.atan2(point[0]-position[0],point[1]-position[2]))
                        dx=round(math.remainder(local['look'][0]-yaw,360)/sensitivity)
                        dy=round(local['look'][1]/sensitivity)*(-1 if controls.get('invert_y') else 1)
                        current=command_capture(shooter,'combat-walk-'+str(index),mouse=[dx,dy],keys=[87],hold_seconds=1.5,sample_hold=True,wait=.3)
                        walk.append(current)
                    result['combat_walk']=walk
                    assert target,('No living enemy found on bounded opening route; combat remains unverified',current)
                    result['combat_target_before']=target
                    controls=read_json(runs[shooter]/'controls.json')['mouse'];sensitivity=controls['sensitivity']
                    for attempt in range(3):
                        candidates=[c for c in current['world']['characters'] if c.get('network_id')==target['network_id'] and c.get('health',0)>0]
                        if not candidates:break
                        point=list(candidates[0]['position']);point[1]+=1.1
                        local=current['local_player'];delta=[a-b for a,b in zip(point,local['position'])]
                        yaw=math.degrees(math.atan2(delta[0],delta[2]));pitch=math.degrees(math.atan2(delta[1],math.hypot(delta[0],delta[2])))
                        dx=round(math.remainder(local['look'][0]-yaw,360)/sensitivity)
                        dy=round((local['look'][1]-pitch)/sensitivity)*( -1 if controls.get('invert_y') else 1)
                        current=command_capture(shooter,'combat-shot-'+str(attempt),mouse=[dx,dy],keys=[1],hold_seconds=1,wait=.3)
                    observations={role:command_capture(role,'combat-after',wait=1) for role in roles}
                    result['combat_after']=observations
                    # Attribute the host's actual damage application to this
                    # guest; an unrelated NPC fight is not a successful hit test.
                    trace=(runs['host']/'run0.err').read_text(errors='replace')
                    pattern=r'\[PC-STORY-HIT\] role=1 incoming=[1-9]\d* target='+str(target['network_id'])+r' slot=\d+ source='+str(shooter_slot)+r' damage=([^ ]+) hp=([^ ]+)->([^ ]+) result=(\d+)'
                    hits=[m.group(0) for m in re.finditer(pattern,trace) if float(m.group(2))>float(m.group(3))]
                    result['combat_host_hits']=hits
                    assert hits,('No host-accepted damage from the guest on the selected NPC',target,observations)
                    def npc_health(s):
                        c=next((c for c in s.get('world',{}).get('characters',[]) if c.get('network_id')==target['network_id']),None)
                        return c.get('health') if c else None
                    expected_health=npc_health(observations['host'])
                    for role,s in observations.items():
                        value=npc_health(s)
                        assert npc_health_agrees(expected_health,value),(role,'NPC health differs from host',value,expected_health)
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
