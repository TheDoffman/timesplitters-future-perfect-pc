"""Bounded Windows Unlock Content checks with fresh disposable progress.

Uses the local Test Lab fixture only as a controls/container seed. The copy's
progress and unlock extension are reset before launch; the fixture is unchanged.
--worker exposes stability_transitions_game's bounded commands.jsonl protocol
for visual inspection; normal mode runs the repeatable menu/save/reload route.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
sys.path.insert(0, str(ROOT / 'tests/gameplay'))

import hashlib
import json
import subprocess
import struct
import time

from testing_profile import BODY_SIZE, signature


def worker():
    import stability_transitions_game as runner
    prepare = runner.prepare_run
    def fresh(root, run):
        environment = prepare(root, run)
        profiles = list((run/'saves/UserData/45410066').glob('*/tsftrpft'))
        assert len(profiles) == 1, 'Use a dedicated single-profile test directory'
        path = profiles[0]
        raw = path.read_bytes()
        assert signature(raw[:BODY_SIZE]) == raw[BODY_SIZE:]
        body = bytearray(raw[:BODY_SIZE])
        body[0x1290:0x1edc] = bytes(0x1edc-0x1290)
        body[0x1ee4:0x1ef4] = bytes(16)
        path.write_bytes(body+signature(body))
        (run/'unlock-baseline.bin').write_bytes(body)
        return environment
    runner.prepare_run = fresh
    sys.argv = [str(ROOT/'tests/gameplay/stability_transitions_game.py'),
                '--run-name','unlock-content','--seconds','600','--no-native-trace']
    runner.main()


def main():
    if '--worker' in sys.argv:
        worker();return
    run = ROOT/'private/test-saves/unlock-content'
    report = run/'report.json'
    assert not (run/'run.lock').exists(), 'Another test owns this directory'
    if report.exists():report.unlink()
    evidence={}
    with (ROOT/'logs/unlock-content-game.log').open('w') as log:
        process = subprocess.Popen([sys.executable,__file__,'--worker'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+45
            while not (run/'run.lock').exists():
                assert process.poll() is None and time.monotonic()<deadline
                time.sleep(.2)
            def command(label, **args):
                with (run/'commands.jsonl').open('a') as stream:
                    stream.write(json.dumps(dict(label=label,**args))+'\n')
                deadline=time.monotonic()+55
                while time.monotonic()<deadline:
                    assert process.poll() is None
                    try:
                        for row in json.loads(report.read_text()):
                            if row.get('command',{}).get('label')==label:return row
                    except (OSError,ValueError):pass
                    time.sleep(.2)
                raise AssertionError('Timed out at '+label)
            def saved_flags(expected):
                paths=list((run/'saves/UserData/45410066').glob('*/tsftrpft'))
                assert len(paths)==1
                raw=paths[0].read_bytes();body=raw[:BODY_SIZE]
                assert len(raw)==BODY_SIZE+20 and signature(body)==raw[BODY_SIZE:]
                assert struct.unpack_from('<4I',body,0x1ee4)==(0x43555354,1,expected,expected^0xffffffff)
                baseline=(run/'unlock-baseline.bin').read_bytes()
                # Every original byte, including earned progress, remains unchanged.
                # The game's own save nonce may change on save.
                assert body[:0x1edc]==baseline[:0x1edc]
                assert body[0x1ee0:0x1ee4]==baseline[0x1ee0:0x1ee4]
                assert body[0x1ef4:]==baseline[0x1ef4:]
                return hashlib.sha256(raw).hexdigest()
            command('main',wait=24)
            command('extras-before',keys=[38,38,69],wait=2)
            command('unlocks-off',keys=[40,40,69],wait=2)
            command('cheats-on',keys=[40]*7+[39],wait=2)
            command('extras-cheats',keys=[70],wait=4)
            evidence['cheats_saved']=saved_flags(128)
            command('original-cheats',keys=[40,40,40,69],wait=2)
            command('cheats-exit',keys=[70],wait=2)
            command('unlocks-cheats-only',keys=[38,69],wait=2)
            command('first-categories-on',keys=[39,40]*7,wait=2)
            command('all-categories-on',keys=[40,39],wait=2)
            command('extras-all',keys=[70],wait=4)
            evidence['all_saved']=saved_flags(511)
            command('movies-all',keys=[69],wait=2)
            command('reloaded-main',restart=True,wait=24)
            command('reloaded-extras',keys=[38,38,69],wait=2)
            command('all-reloaded',keys=[40,40,69],wait=2)
            assert saved_flags(511)==evidence['all_saved']
            command('first-categories-off',keys=[37,40]*7,wait=2)
            command('all-categories-off',keys=[37,40,37],wait=2)
            command('extras-reset',keys=[70],wait=4)
            evidence['reset_saved']=saved_flags(0)
            command('movies-reset',keys=[69],wait=2)
            command('reset-reloaded-main',restart=True,wait=24)
            command('reset-reloaded-extras',keys=[38,38,69],wait=2)
            command('reset-reloaded',keys=[40,40,69],wait=2)
            assert saved_flags(0)==evidence['reset_saved']
            evidence['original_profile_bytes_preserved']=True
            evidence['reload_on_and_off']=True
        finally:
            if process.poll() is None:
                with (run/'commands.jsonl').open('a') as stream:stream.write('{"stop":true}\n')
                process.wait(timeout=45)
        assert process.returncode==0
    (run/'verification.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS: independent Cheats/all/reset saves, signed reloads and original profile bytes; inspect menu captures')


if __name__=='__main__':main()
