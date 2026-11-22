#!/usr/bin/env python3
"""Exercise saved Vulkan/OpenGL selection through the real native launcher UI.

Uses an owned, minimal installation fixture and the normal game/launcher binary.
Original assets are read separately by the child; this is not package relocation
or physical window/scanout acceptance. No personal settings or saves are used.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import runpy
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
from testing_profile import prepare_run,check_fixture
from native_mac_common import recycle_test_disks


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def unlinked(path,scope):
    path=Path(path)
    if not path.resolve().is_relative_to(scope.resolve()):raise ValueError('Test path escaped isolation')
    for item in (path,*path.parents):
        if item.is_symlink():raise ValueError('Linked test path')
    return path


def stage(root,scope,manifest):
    session=unlinked(scope/'launcher-session',scope)
    marker=session/'renderer-test-install.json'
    owner={'schema':1,'owner':'linux-launcher-renderers'}
    if session.exists() and (not marker.is_file() or json.loads(marker.read_text())!=owner):
        raise ValueError('Refuse an installation not owned by this test')
    session.mkdir(exist_ok=True)
    for name in ('Bin','Settings','Data/pak','Data/xmv','logs'):
        unlinked(session/name,scope).mkdir(parents=True,exist_ok=True)
    marker.write_text(json.dumps(owner)+'\n')
    source=root/'build/linux/game/futureperfect_recomp'
    target=unlinked(session/'Bin/futureperfect_recomp',scope)
    target.unlink(missing_ok=True)  # Remove only this owned hardlink before relinking.
    os.link(source,target)
    shutil.copyfile(root/'private/game/default.xbe',session/'Data/default.xbe')
    for name in ('pak','xmv'):(session/'Data'/name/'fixture.marker').write_text('Assets are read separately by the isolated child.\n')
    build={key:manifest[key] for key in ('source_revision','renderer','supported_renderers','executable_sha256')}
    build['files']={'Bin/futureperfect_recomp':digest(target)}
    (session/'BUILD.json').write_text(json.dumps(build)+'\n')
    controls=session/'Settings/pc-controls.json'
    model=json.loads((controls if controls.exists() else root/'tests/fixtures/pc-controls.json').read_text())
    model.setdefault('launcher',{})['renderer']='opengl'
    model['renderer_test_unknown']={'retained':True}
    controls.write_text(json.dumps(model)+'\n')
    return session


def run_case(args,scope,session,backend,evidence,native,rhi):
    env=native['runtime_environment']()
    if backend=='vulkan' and args.driver_manifest:rhi['linux_vulkan_sdk'].select_driver(env,args.driver_manifest)
    if backend=='opengl':env=rhi['configure_gl_environment'](env,args.gl_driver,args.gl_adapter)
    selected=prepare_run(ROOT,scope/'launcher-selector-profile')
    for key in ('RECOMP_SAVE_DIR','RECOMP_INPUT_CONFIG','RECOMP_ICALL_FEEDBACK_PATH','TSFP_SAVE_LOCATION_LOCK','TSFP_TEXTURE_PACK_DIR','TSFP_TEXTURE_MODE'):
        env[key]=selected[key]
    # Keep every writable path in this test scope. Original assets are read only.
    paths={'RECOMP_DISPLAY_CONFIG':'runtime.conf','TSFP_DISPLAY_CONFIG':'pc-display.ini',
           'TSFP_DISCORD_CONFIG':'pc-discord.ini','TSFP_CRASH_DIR':'launcher-session/Logs/Crashes',
           'XDG_CONFIG_HOME':'xdg-config','XDG_DATA_HOME':'xdg-data','XDG_CACHE_HOME':'xdg-cache',
           'TSFP_NATIVE_TEST_KEYS':'test-keys.txt','RECOMP_LOG_FILE':'launcher-session/logs/native-play.err'}
    env.update({key:str(unlinked(scope/name,scope)) for key,name in paths.items()})
    env.update(RECOMP_GAME_DIR=str(ROOT/'private/game'),RECOMP_XMV_PLAY='0',RECOMP_VK_VALIDATION='0',
               RECOMP_SHADER_CACHE='1')
    if args.sdl_driver:env['SDL_VIDEODRIVER']=args.sdl_driver
    if args.wayland_display:env['WAYLAND_DISPLAY']=args.wayland_display
    captures=unlinked(scope/'selector-captures'/args.label/backend,scope);captures.mkdir(parents=True,exist_ok=True)
    env.update(RECOMP_HLE_D3D8_DUMP=str(captures/'frame'),RECOMP_HLE_D3D8_DUMP_EVERY='60',RECOMP_HLE_D3D8_DUMP_KEEP_LAST='2')
    env={key:value for key,value in env.items() if key.startswith(('RECOMP_','TSFP_','XDG_','VK_','SDL_','GALLIUM_','MESA_')) or key in ('LD_LIBRARY_PATH','WAYLAND_DISPLAY','DISPLAY')}
    env.pop('RECOMP_D3D8_BACKEND',None)  # The actual UI/StartInfo must supply this.
    for name,value in [('runtime.conf','# Isolated native launcher test\n'),('pc-display.ini','TSFP_DISPLAY_V2 640 480 55\n'),('pc-discord.ini','TSFP_DISCORD_V1 0 1561627743353962566\n')]:
        path=scope/name
        if not path.exists():path.write_text(value)
    keys=scope/'test-keys.txt';keys.write_text('')
    (session/'result.json').unlink(missing_ok=True)
    plan=session/'plan.json'
    plan.write_text(json.dumps(dict(runtime_root=str(ROOT),environment=env,renderer=backend,seconds=120,mode='quit'))+'\n')
    actions=native['input_script'](ROOT/'tests/fixtures/linux-story-benchmark.json',120)
    states=[];memory=None;offset=None;child_pid=None;started=None;skipped=False;resumed=False
    parent_env={k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_','TSFP_','VK_','SDL_VULKAN','DYLD_'))}
    with (evidence/'launcher.log').open('w') as output:
        process=subprocess.Popen([str(args.launcher),'--root',str(session),'--test-session',str(plan)],env=parent_env,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
        deadline=time.monotonic()+155
        try:
            while process.poll() is None:
                if time.monotonic()>deadline:raise TimeoutError('Bounded native launcher session exceeded155s')
                record=session/'.launcher/game-session.json'
                if child_pid is None and record.is_file():
                    data=json.loads(record.read_text())
                    if data.get('launcher_pid')==process.pid and data.get('game_pid'):
                        candidate=int(data['game_pid'])
                        status=Path(f'/proc/{candidate}/status').read_text()
                        if int(re.search(r'^PPid:\s+(\d+)',status,re.M)[1])!=process.pid:raise ValueError('Game is not this launcher child')
                        child_pid=candidate;started=time.monotonic();memory=os.open(f'/proc/{candidate}/mem',os.O_RDONLY|os.O_CLOEXEC)
                if child_pid:
                    elapsed=time.monotonic()-started
                    if offset is None:
                        path=session/'logs/native-play.out'
                        match=re.search(r'Xbox memory offset: (0x[0-9a-fA-F]+)',path.read_text(errors='replace')) if path.exists() else None
                        if match:offset=int(match[1],16)
                    if offset is not None:
                        def word(address):return struct.unpack('<I',os.pread(memory,4,offset+address))[0]
                        state=native['story_snapshot'](word);state['seconds']=round(elapsed,3);states.append(state)
                        if elapsed>=50 and state['flags']&0x20000000 and not skipped:
                            skipped=True;actions.extend([(elapsed,'+Escape\n'),(elapsed+.2,'-Escape\n')]);actions.sort()
                        if skipped and state['paused'] and not resumed:
                            resumed=True;actions.extend([(elapsed+.5,'+E\n'),(elapsed+.7,'-E\n')]);actions.sort()
                    while actions and elapsed>=actions[0][0]:
                        _,commands=actions.pop(0)
                        with keys.open('a') as out:out.write(commands)
                time.sleep(.1)
            if process.returncode:raise RuntimeError('Native launcher failed; inspect '+str(evidence/'launcher.log'))
        finally:
            if memory is not None:os.close(memory)
            try:os.killpg(process.pid,signal.SIGTERM)
            except ProcessLookupError:pass
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
    result=json.loads((session/'result.json').read_text())
    result.update(game_pid=child_pid,story_samples=states,active_story_observed=sum(s['active'] for s in states)>=100)
    text=(session/'logs/native-play.err').read_text(errors='replace')
    result['selected_backend_presented']='[PRESENT] backend='+backend in text
    result['no_fallback']='attempting OpenGL startup recovery' not in text
    controls=json.loads((session/'Settings/pc-controls.json').read_text())
    result['unknown_setting_retained']=controls.get('renderer_test_unknown')=={'retained':True}
    result['passed']=result['passed'] and all(result[k] for k in ('active_story_observed','selected_backend_presented','no_fallback','unknown_setting_retained'))
    for source in (session/'result.json',session/'BUILD.json',plan,session/'Settings/pc-controls.json',session/'logs/native-play.err',session/'logs/native-play.out',*captures.glob('*.bmp')):
        shutil.copy2(source,evidence/source.name)
    (evidence/'verified.json').write_text(json.dumps(result,indent=2)+'\n')
    recycle_test_disks(ROOT,scope,save_directory=Path(env['RECOMP_SAVE_DIR']))
    if not result['passed']:raise RuntimeError('Native selector/game checks failed')
    print(backend,'PASS saved UI selection, game rendering/Story and launcher lifecycle; physical window observation is separate',flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--launcher',type=Path,required=True);p.add_argument('--label',required=True)
    p.add_argument('--driver-manifest',type=Path,help='Optional child-only Vulkan ICD')
    p.add_argument('--gl-driver',choices=('system','software','d3d12'),default='system')
    p.add_argument('--gl-adapter',help='Optional child-only D3D12 GL adapter preference')
    p.add_argument('--sdl-driver',choices=('wayland','x11'),help='Optional game-only SDL display driver')
    p.add_argument('--wayland-display',help='Optional game-only Wayland socket')
    args=p.parse_args()
    if platform.system()!='Linux':p.error('Native Linux only')
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,47}',args.label):p.error('Use a safe unique evidence label')
    native=runpy.run_path(str(ROOT/'tools/testing/test-native-linux.py'));rhi=runpy.run_path(str(ROOT/'tools/testing/test-rhi-linux.py'))
    if digest(ROOT/'private/game/default.xbe')!=native['EXPECTED']:raise ValueError('Unsupported game executable')
    manifest=json.loads((ROOT/'build/linux/game/BUILD.json').read_text())
    if digest(ROOT/'build/linux/game/futureperfect_recomp')!=manifest['executable_sha256']:raise ValueError('Game differs from its build receipt')
    scope=unlinked(ROOT/'private/test-saves/native-linux',ROOT/'private/test-saves');scope.mkdir(parents=True,exist_ok=True)
    evidence=ROOT/'logs'/args.label;evidence.mkdir(exist_ok=False)
    with (scope/'run.lock').open('a+') as owner:
        fcntl.flock(owner,fcntl.LOCK_EX|fcntl.LOCK_NB)
        session=stage(ROOT,scope,manifest)
        for backend in ('vulkan','opengl'):
            output=evidence/backend;output.mkdir()
            run_case(args,scope,session,backend,output,native,rhi)
        check_fixture(ROOT)


if __name__=='__main__':main()
