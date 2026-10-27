"""Bounded Windows Accept/Cancel/relaunch regression using isolated Test Lab saves.

Run directly after tsfp.ps1 -Action Build. Retains renderer PNGs for visual
review, verifies the signed disk bytes and reads the loaded Controls draft.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import ctypes as c
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=_layout_root
sys.path.insert(0,str(ROOT/'tools'))
from testing_profile import signature,BODY_SIZE,contained,FIXTURE


def main():
    run=ROOT/'private/test-saves/controls-accept'
    assert not (run/'run.lock').exists(),'Another test owns the run directory'
    # prepare_run restores the seed but deliberately retains additional slots.
    # Recycle only this test's known game-created Test Lab copy on repeat runs.
    extra=contained(ROOT,run/'saves/UserData/45410066/1AD19156651F','private/test-saves')
    if extra.exists():
        marker=json.loads((run/'test-run.json').read_text())
        assert marker==dict(schema=1,fixture=str(Path(FIXTURE)))
        assert {p.name for p in extra.iterdir()}=={'SaveMeta.xbx','tsftrpft'}
        assert (extra/'SaveMeta.xbx').read_text(encoding='utf-16').strip()=='Name=Test Lab'
        for name in ('SaveMeta.xbx','tsftrpft'):
            contained(ROOT,extra/name,'private/test-saves').unlink()
        extra.rmdir()
    commands=run/'commands.jsonl'
    # Remove only the old report, otherwise a previous label could satisfy a wait.
    report=run/'report.json'
    for old in (report,run/'verification.json'):
        if old.exists():
            assert not old.is_symlink() and old.resolve().parent==run.resolve()
            old.unlink()
    kernel=c.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.restype=c.c_void_p
    kernel.ReadProcessMemory.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.c_void_p]
    kernel.CloseHandle.argtypes=[c.c_void_p]
    evidence={}

    def profiles():
        found={}
        for p in (run/'saves/UserData/45410066').glob('*/tsftrpft'):
            raw=p.read_bytes()
            assert len(raw)==BODY_SIZE+20 and signature(raw[:BODY_SIZE])==raw[BODY_SIZE:]
            found[p.parent.name]=raw
        assert found
        return found

    def inverse(pid):
        handle=kernel.OpenProcess(0x410,False,pid);assert handle
        try:
            value=c.c_ubyte()
            # US Controls draft byte +2: Inverse Look is bit 0 (row ID 3).
            assert kernel.ReadProcessMemory(handle,0x7BA366+0x10000,c.byref(value),1,None)
            return bool(value.value&1)
        finally:kernel.CloseHandle(handle)

    def command(label,**kwargs):
        with commands.open('a') as f:f.write(json.dumps(dict(label=label,**kwargs))+'\n')
        end=time.monotonic()+50
        while time.monotonic()<end:
            assert process.poll() is None,'Test runner exited early'
            try:
                rows=json.loads(report.read_text())
                for row in rows:
                    if row.get('command',{}).get('label')==label:return row
            except (OSError,ValueError):pass
            time.sleep(.2)
        raise AssertionError('Timed out at '+label)

    with (ROOT/'logs/controls-accept-game.log').open('w') as log:
        process=subprocess.Popen([sys.executable,str(ROOT/'tests/gameplay/stability_transitions_game.py'),
                                  '--run-name','controls-accept'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        try:
            end=time.monotonic()+30
            while not (run/'run.lock').exists():
                assert process.poll() is None and time.monotonic()<end
                time.sleep(.2)
            command('main',wait=22)
            row=command('controls-before',keys=[38,38,38,69,40,40,69],gap=.8,wait=3)
            assert inverse(row['pid'])
            before=profiles();assert len(before)==1
            command('accept-selected',keys=[40,40,39,38,38,38,38],wait=3)
            command('accepted-settings',keys=[69],wait=3)
            accepted=profiles()
            changed={slot:raw for slot,raw in accepted.items() if before.get(slot)!=raw}
            assert len(changed)==1
            active,raw=next(iter(changed.items()))
            seed=next(iter(before.values()))
            assert [(i,a,b) for i,(a,b) in enumerate(zip(seed[:BODY_SIZE],raw[:BODY_SIZE])) if a!=b]==[(6,255,254)]
            evidence['saved_before_quit']=True
            evidence['active_slot']=active
            evidence['accepted_sha256']=hashlib.sha256(raw).hexdigest()
            # The renamed canonical fixture produces a second Test Lab slot on
            # its first game write. Select that slot explicitly on relaunch.
            assert len(accepted)==2
            events=[(5000,69),(10000,69),(14000,40),(16000,40),(18000,69)]
            (run/'input.txt').write_text('\n'.join(f'{t+d} key {k} {v}' for t,k in events
                                                 for d,v in [(0,1),(180,0)])+'\n')
            command('reloaded-main',restart=True,wait=22)
            row=command('accepted-reloaded',keys=[38,38,38,69,40,40,69],gap=.8,wait=3)
            assert not inverse(row['pid'])
            evidence['accept_reloaded_off']=True
            command('cancel-selected',keys=[40,40,39,38,38,38],wait=3)
            command('cancelled-settings',keys=[69],wait=3)
            assert profiles()==accepted,'Cancel wrote the draft'
            command('cancel-reloaded-main',restart=True,wait=22)
            row=command('cancel-reloaded',keys=[38,38,38,69,40,40,69],gap=.8,wait=3)
            assert not inverse(row['pid']) and profiles()==accepted
            evidence['cancel_preserved_save_and_reload']=True
            print('PASS: Accept saved before quit; immediate Settings exit/reload retained OFF; Cancel preserved OFF across another restart')
        finally:
            if process.poll() is None:
                with commands.open('a') as f:f.write('{"stop":true}\n')
                process.wait(45)
        assert process.returncode==0
    (run/'verification.json').write_text(json.dumps(evidence,indent=2)+'\n')


if __name__=='__main__':main()
