#!/usr/bin/env python3
"""End-to-end native launcher/game probe using the existing bounded save runner."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
SOURCE=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native_runner',SOURCE/'tools/test-native-mac.py')
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-root',type=Path,default=SOURCE)
    parser.add_argument('--launcher',type=Path,required=True)
    parser.add_argument('--seconds',type=int,default=8)
    parser.add_argument('--discord',action='store_true',help='Verify optional Rich Presence with the running Discord desktop client')
    parser.add_argument('--mode',choices=('quit','crash'),required=True)
    parser.add_argument('--story',action='store_true',help='Replay synthetic SDL input through Test Lab and the first story briefing')
    args=parser.parse_args();root=args.runtime_root.resolve();test=root/'private/test-saves/native-mac';session=test/'launcher-session'
    if not 3<=args.seconds<=120:parser.error('seconds must be 3..120')
    assert hashlib.sha256((root/'private/game/default.xbe').read_bytes()).hexdigest()==native.EXPECTED_XBE
    protected=[root/'config/pc-controls.json',root/'config/pc-display.ini',root/'config/pc-discord.ini',root/'private/native-settings/runtime.conf']
    before={str(p):p.read_bytes() if p.exists() else None for p in protected}
    manifest=json.loads((root/'build/macos/BUILD.json').read_text())
    assert hashlib.sha256((root/'build/macos/futureperfect_recomp').read_bytes()).hexdigest()==manifest['executable_sha256']
    with native.test_session(root,test,True) as env:
        assert not session.is_symlink();session.mkdir(exist_ok=True)
        for name in ('result.json','export.zip'):
            path=session/name;assert not path.is_symlink();path.unlink(missing_ok=True)
        keys=test/'test-keys.txt';assert not keys.is_symlink();keys.write_text('')
        discord=test/'pc-discord.ini';assert not discord.is_symlink();discord.write_text(f'TSFP_DISCORD_V1 {int(args.discord)} 1561627743353962566\n')
        logs=session/'logs';assert not logs.is_symlink();logs.mkdir(exist_ok=True)
        env.update(TSFP_NATIVE_TEST_KEYS=str(keys),TSFP_DISCORD_CONFIG=str(discord),TSFP_CRASH_DIR=str(session/'Logs/Crashes'),RECOMP_LOG_FILE=str(logs/'native-play.err'))
        env.update(native.capture_environment(root))
        if manifest['renderer']=='vulkan':
            import vulkan_sdk
            env=vulkan_sdk.with_probe_gpu_environment(env,vulkan_sdk.verify_install(root),vulkan_sdk.select_driver(manifest))
            env['RECOMP_VK_VALIDATION']='1'
        plan=session/'plan.json';assert not plan.is_symlink();plan.write_text(json.dumps(dict(runtime_root=str(root),environment=env,seconds=args.seconds,mode=args.mode)))
        stdout=root/'logs/mac-launcher-e2e.out';stderr=root/'logs/mac-launcher-e2e.err'
        print(f'Isolated saves: {env["RECOMP_SAVE_DIR"]}\nMode: {args.mode}; limit: {args.seconds+35}s',flush=True)
        with stdout.open('wb') as out,stderr.open('wb') as err:
            proc=subprocess.Popen([str(args.launcher.resolve()),'--test-session',str(plan),'--root',str(session)],stdout=out,stderr=err,start_new_session=True)
            try:
                deadline=time.monotonic()+args.seconds+35
                sequence=[(5,'E'),(9,'E'),(12,'Down'),(14,'E'),(17,'E'),(20,'E'),(23,'E'),(26,'E'),(40,'Escape'),(58,'Escape')]
                events=[(at+offset,prefix+key) for at,key in sequence for offset,prefix in ((0,'+'),(.2,'-'))]
                while proc.poll() is None:
                    if time.monotonic()>deadline or stdout.stat().st_size+stderr.stat().st_size>8*1024*1024:raise RuntimeError('Launcher test exceeded its time/log limit')
                    if args.story:
                        elapsed=time.monotonic()-(deadline-args.seconds-35)
                        for event in list(events):
                            if elapsed>=event[0]:
                                with keys.open('a') as stream:stream.write(event[1]+'\n')
                                events.remove(event)
                    time.sleep(.1)
            finally:
                # Tear down helpers too before the runner recycles virtual disks.
                try:os.killpg(proc.pid,signal.SIGTERM)
                except ProcessLookupError:pass
                try:proc.wait(timeout=3)
                except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait()
                plan.unlink()
        result=json.loads((session/'result.json').read_text());assert result['passed'],result
        text=(logs/'native-play.err').read_text(errors='replace');assert 'present' in text.lower(),text[-2000:]
        if manifest['renderer']=='vulkan':vulkan_sdk.require_clean_validation(text)
        if args.discord:assert 'Discord accepted Rich Presence.' in text,'Discord did not acknowledge Rich Presence'
        result['discord_activities']=[line.split('Activity: ',1)[1] for line in text.splitlines() if '[PC-DISCORD] Activity: ' in line]
        result['discord_accepted']=args.discord and 'Discord accepted Rich Presence.' in text
        result.update(source_revision=manifest['source_revision'],renderer=manifest['renderer'],executable_sha256=manifest['executable_sha256'])
        (root/f'logs/mac-launcher-e2e-{args.mode}.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
    for p in protected:assert (p.read_bytes() if p.exists() else None)==before[str(p)],f'Personal file changed: {p}'
    print('Personal settings unchanged; disposable virtual disks recycled.')
if __name__=='__main__':main()
