"""Bounded first-mission aspect/render-resolution check using an isolated profile.

Run once per resolution; recycle private/display-validation/saves between fresh
profile runs. Keeps at most three temporary BMPs, plus the selected PNG.
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

import os
from pathlib import Path
import subprocess
import sys
import time
from PIL import Image
import pc_settings_game as ui

ROOT=_layout_root

def main():
    w,h=map(int,sys.argv[1:3]); tag=f'{w}x{h}'
    folder=ROOT/'private/display-validation';frames=folder/'frames'
    frames.mkdir(parents=True,exist_ok=True)
    config=folder/'pc-display.ini';config.write_text(f'TSFP_DISPLAY_V1 {w} {h}\n')
    events=[]
    for at in [15000,18000,23000,27000,31000,35000,39000,43000]:
        events.extend([f'{at} key 13 1',f'{at+180} key 13 0'])
    events.extend(['80000 key 27 1','80300 key 27 0'])
    # Enter paused Options, then its new Display settings item.
    for at,vk in [(155000,27),(158000,40),(159000,40),(161000,13),
                  (164000,40),(165000,40),(167000,13)]:
        events.extend([f'{at} key {vk} 1',f'{at+180} key {vk} 0'])
    replay=folder/'gameplay-input.txt';replay.write_text('\n'.join(events)+'\n')
    env={k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_','TSFP_'))}
    env.update(RECOMP_GAME_DIR=str(ROOT/'private/game'),RECOMP_SAVE_DIR=str(folder/'saves'),
        RECOMP_INPUT_CONFIG=str(ROOT/'tests/fixtures/pc-controls.json'),TSFP_DISPLAY_CONFIG=str(config),
        RECOMP_TRACE_BUDGET='0',RECOMP_PC_MOUSE='1',RECOMP_WINDOW_BACKGROUND='1',
        TSFP_PC_INPUT_REPLAY=str(replay),RECOMP_HLE_D3D8_DUMP=str(frames/'gameplay'),
        RECOMP_HLE_D3D8_DUMP_EVERY='240',RECOMP_HLE_D3D8_DUMP_FROM='7800',RECOMP_HLE_D3D8_DUMP_KEEP_LAST='24')
    exe=ROOT/'build/windows/Release/futureperfect_recomp.exe'
    if '--textures' in sys.argv:
        env['TSFP_TEXTURE_PACK_DIR']=str(ROOT/'private/texture-remaster/pack')
    if '--shader-probe' in sys.argv:
        env['RECOMP_D3D8_VS_DUMP']='1'
    with (folder/f'{tag}.out').open('wb') as out,(folder/f'{tag}.err').open('wb') as err:
        p=subprocess.Popen([str(exe)],cwd=exe.parent,env=env,stdout=out,stderr=err)
        start=time.monotonic();shot=False
        try:
            game=ui.wait_window(p.pid,'TimeSplitters FP');ui.user.ShowWindow(game,0)
            while time.monotonic()-start<174:
                assert p.poll() is None,'Game exited'
                files=sorted(frames.glob('*.bmp'),key=lambda f:f.stat().st_mtime_ns)
                for f in files[:-3]: f.unlink()
                if not shot and time.monotonic()-start>150 and files:
                    try:
                        with Image.open(files[-1]) as im:
                            assert im.size==((w,h) if w else (640,480)),im.size
                            im.save(folder/f'gameplay-{tag}.png')
                        shot=True
                    except OSError: pass
                time.sleep(.1)
        finally:
            if p.poll() is None:p.terminate()
            p.wait(timeout=5)
    log=(folder/f'{tag}.err').read_text(errors='replace')
    assert '[CRASH]' not in log and '[ICALL] unresolved' not in log
    assert shot,'No gameplay capture'
    assert '[PC-DISPLAY] Camera horizontal factor' in log,'Camera hook not reached'
    assert 'added to paused Options' in log,'Paused menu not reached'
    print(f'PASS: {tag} startup, first mission capture, paused Options injection; inspect visual evidence')

if __name__=='__main__':main()
