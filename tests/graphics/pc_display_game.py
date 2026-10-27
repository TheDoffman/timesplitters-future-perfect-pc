"""Exercise the actual guest Settings menu using recorded game input.

One reusable scratch directory; never touches personal saves/settings.
Captures only three rolling native frames (up to ~100 MiB at 4K).
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
import argparse
from pathlib import Path
import subprocess
import tempfile
import shutil
import time
from PIL import Image

ROOT=_layout_root

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT,help='Checkout containing the verified private/game')
    parser.add_argument('--exe',type=Path,default=None)
    args=parser.parse_args(); root=args.root.resolve()
    import hashlib
    assert hashlib.sha256((root/'private/game/default.xbe').read_bytes()).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    folder=root/'private/display-validation'
    frames=folder/'frames'; frames.mkdir(parents=True,exist_ok=True)
    # Fresh disposable save tree avoids existing profiles changing menu navigation.
    saves=Path(tempfile.mkdtemp(prefix='menu-saves-',dir=folder)).resolve()
    assert saves.parent==folder.resolve() and not saves.is_symlink()
    config=folder/'pc-display.ini'
    # Explicit baseline makes reruns deterministic without changing user files.
    config.write_text('TSFP_DISPLAY_V1 0 0\n')
    events=[]
    def key(at,vk): events.extend([(at,f'{at} key {vk} 1'),(at+180,f'{at+180} key {vk} 0')])
    for at in [15000,18000,23000,27000]: key(at,13)
    for at,vk in [(33000,38),(34000,38),(35000,13),(43000,40),(44000,40),(45000,13),
                  (48000,39),(49000,39),(50000,39), # 4:3, 5:4, 16:9; nearest available height
                  (52000,40),(53000,37),(54000,39), # resolution previous/next
                  (55000,40),(56000,40),(57000,13), # Apply
                  (60000,38),(61000,13), # Keep
                  (63000,40),(64000,39),(65000,40),(66000,40),(67000,13),
                  # Unconfirmed next mode must revert after 15 seconds.
                  (85000,8)]: key(at,vk)
    replay=folder/'input.txt'; replay.write_text('\n'.join(t for _,t in sorted(events))+'\n')
    env={k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_','TSFP_'))}
    controls=folder/'pc-controls.json';controls.write_bytes((root/'tests/fixtures/pc-controls.json').read_bytes())
    runtime=folder/'runtime.conf';runtime.write_text('render_scale = 1\nfullscreen = 0\n')
    env.update(RECOMP_GAME_DIR=str(root/'private/game'),RECOMP_SAVE_DIR=str(saves),
        RECOMP_INPUT_CONFIG=str(controls), RECOMP_DISPLAY_CONFIG=str(runtime), TSFP_DISPLAY_CONFIG=str(config),
        RECOMP_TRACE_BUDGET='0', RECOMP_PC_MOUSE='1',RECOMP_WINDOW_BACKGROUND='1',
        TSFP_PC_INPUT_REPLAY=str(replay),RECOMP_HLE_D3D8_DUMP=str(frames/'frame'),
        RECOMP_HLE_D3D8_DUMP_EVERY='60',RECOMP_HLE_D3D8_DUMP_KEEP_LAST='3')
    for name in ('xdg-config','xdg-data'):
        (folder/name).mkdir(exist_ok=True)
    env.update(XDG_CONFIG_HOME=str(folder/'xdg-config'),XDG_DATA_HOME=str(folder/'xdg-data'))
    exe=(args.exe or root/'build/windows/Release/futureperfect_recomp.exe').resolve()
    shots=set();confirmed=None
    with (folder/'game.out').open('wb') as out,(folder/'game.err').open('wb') as err:
        p=subprocess.Popen([str(exe)],cwd=exe.parent,env=env,stdout=out,stderr=err)
        start=time.monotonic()
        try:
            while time.monotonic()-start<86:
                assert p.poll() is None,'Game exited'
                files=[]
                for frame in frames.glob('*.bmp'):
                    try: files.append((frame.stat().st_mtime_ns,frame))
                    except FileNotFoundError: pass
                files=[frame for _,frame in sorted(files)]
                # These are this test's own disposable frames; preserve only the newest three.
                for f in files[:-3]: f.unlink(missing_ok=True)
                elapsed=time.monotonic()-start
                if elapsed>=62 and confirmed is None: confirmed=config.read_text()
                for at,label in [(47,'original-picker'),(55,'picker'),(59,'confirmation'),(63,'saved'),(84,'reverted')]:
                    if elapsed>=at and label not in shots and files:
                        try:
                            with Image.open(files[-1]) as im: im.save(folder/f'{label}.png')
                            shots.add(label)
                        except OSError: pass # file is still being written
                time.sleep(.1)
        finally:
            if p.poll() is None: p.terminate()
            p.wait(timeout=5)
            # Only this run's newly-created fixture; never the reusable/user saves.
            assert saves.parent==folder.resolve() and saves.name.startswith('menu-saves-')
            assert not saves.is_symlink() and not any(p.is_symlink() for p in saves.rglob('*'))
            shutil.rmtree(saves)
    log=(folder/'game.err').read_text(errors='replace')
    assert '[CRASH]' not in log and '[ICALL] unresolved' not in log
    for expected in ['added to main Settings','Opened,','Native scene target','[PC-DISPLAY] Saved','[PC-DISPLAY-MENU] Reverted','Returned to original menu']:
        assert expected in log,expected+'\n'+log[-8000:]
    assert confirmed is not None and config.read_text()==confirmed
    version,w,h=confirmed.split();w,h=int(w),int(h)
    assert version=='TSFP_DISPLAY_V1' and w and abs(w/h-16/9)<.01
    assert f'[PC-DISPLAY] Saved {w}x{h}' in log
    assert log.count(f'[PC-DISPLAY] Applied {w}x{h}')>=2
    print('PASS: actual guest menu, native scene resize, confirmation/save, timeout rollback, return')

if __name__=='__main__': main()
