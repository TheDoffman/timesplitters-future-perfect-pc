"""Brief real-runtime shortcut smoke check; uses one isolated validation profile."""

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
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import subprocess
import time

import pc_settings_game as ui

ROOT=_layout_root
user=ui.user
user.GetSubMenu.argtypes=[w.HMENU,c.c_int]
user.GetSubMenu.restype=w.HMENU
user.GetMenuState.argtypes=[w.HMENU,w.UINT,w.UINT]
user.GetMenuState.restype=w.UINT

def wait_for(condition,description,seconds=12):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        if condition():return
        time.sleep(.05)
    raise AssertionError(description)

def recycle_validation_disks(save_root):
    expected=(ROOT/'private/display-validation/saves').resolve()
    assert save_root.resolve()==expected,'Unexpected validation save directory'
    # All file operations run in one PowerShell process, using literal paths.
    script=r'''
$ErrorActionPreference='Stop'
Add-Type -AssemblyName Microsoft.VisualBasic
$base='SAVE_ROOT'
if (!(Test-Path -LiteralPath $base)) { exit 0 }
$base=(Resolve-Path -LiteralPath $base).ProviderPath
if ((Get-Item -LiteralPath $base).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw 'Validation save directory must not be a link.'
}
foreach ($number in 0..5) {
    $disk=Join-Path $base ('Partition{0}.img' -f $number)
    if (!(Test-Path -LiteralPath $disk)) { continue }
    $item=Get-Item -LiteralPath $disk
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Unexpected validation disk target.'
    }
    [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile(
        $item.FullName,'OnlyErrorDialogs','SendToRecycleBin','ThrowException')
}
'''.replace('SAVE_ROOT',str(expected).replace("'","''"))
    subprocess.run(['powershell.exe','-NoProfile','-Command',script],check=True,
                   creationflags=subprocess.CREATE_NO_WINDOW)

def main():
    folder=ROOT/'private/display-validation'
    folder.mkdir(parents=True,exist_ok=True)
    config=folder/'texture-toggle-display.ini'
    config.write_text('TSFP_DISPLAY_V1 0 0\n')
    controls=folder/'texture-toggle-controls.json'
    controls.write_bytes((ROOT/'config/pc-controls.default.json').read_bytes())
    protected=[ROOT/'config/pc-controls.json',ROOT/'config/pc-display.ini']
    before={p:p.read_bytes() for p in protected if p.exists()}
    env={k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_','TSFP_'))}
    env.update(RECOMP_GAME_DIR=str(ROOT/'private/game'),
        RECOMP_SAVE_DIR=str(folder/'saves'),
        RECOMP_INPUT_CONFIG=str(controls),
        TSFP_DISPLAY_CONFIG=str(config),RECOMP_TRACE_BUDGET='0',
        RECOMP_PC_MOUSE='0',RECOMP_WINDOW_BACKGROUND='1',
        TSFP_TEXTURE_PACK_DIR=str(ROOT/'private/texture-remaster/pack'),
        TSFP_TEXTURE_MODE='classic')
    exe=ROOT/'build/windows/Release/futureperfect_recomp.exe'
    error_log=ROOT/'logs/texture-toggle-game.err'
    with (ROOT/'logs/texture-toggle-game.out').open('wb') as out,error_log.open('wb') as err:
        p=subprocess.Popen([str(exe)],cwd=exe.parent,env=env,stdout=out,stderr=err,
                           creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            game=ui.wait_window(p.pid,'TimeSplitters FP',timeout=30)
            if not user.GetMenu(game):
                user.SendMessageW(game, 0x100, 0x70, 0)  # F1 restores saved-hidden bar
                user.SendMessageW(game, 0x101, 0x70, 0)
            wait_for(lambda:user.GetMenu(game),'PC menu missing')
            menu=user.GetSubMenu(user.GetMenu(game),0)
            def checked():
                state=user.GetMenuState(menu,0x7e24,0)
                assert state!=0xffffffff,'Texture mode command missing'
                return bool(state&8)
            assert not checked(),'Ordinary play must start in Classic'
            time.sleep(3)
            user.PostMessageW(game,0x104,0x79,0) # WM_SYSKEYDOWN / F10
            wait_for(checked,'F10 did not select 4K')
            user.PostMessageW(game,0x104,0x79,1<<30)
            time.sleep(.25);assert checked(),'Held F10 toggled again'
            user.PostMessageW(game,0x105,0x79,0)
            # A visible menu is optional; retained menu state updates on reopen.
            user.PostMessageW(game,0x100,0x70,0)
            wait_for(lambda:not user.GetMenu(game),'F1 failed to hide menu')
            user.PostMessageW(game,0x100,0x79,0)
            time.sleep(.3)
            user.PostMessageW(game,0x101,0x79,0)
            user.PostMessageW(game,0x100,0x70,0)
            wait_for(lambda:bool(user.GetMenu(game)),'F1 failed to restore menu')
            assert not checked(),'Hidden-menu F10 did not return to Classic'
            user.PostMessageW(game,0x104,0x0d,1<<29)
            wait_for(lambda:not(user.GetWindowLongW(game,-16)&0x00c00000),'Fullscreen failed')
            user.PostMessageW(game,0x104,0x79,0)
            wait_for(checked,'Fullscreen F10 failed')
            user.PostMessageW(game,0x105,0x79,0)
            user.PostMessageW(game,0x111,0x7e24,0)
            wait_for(lambda:not checked(),'Menu command did not return to Classic')
            user.PostMessageW(game,0x104,0x0d,1<<29)
            wait_for(lambda:bool(user.GetWindowLongW(game,-16)&0x00c00000),'Windowed restore failed')
            time.sleep(3);assert p.poll() is None,'Runtime exited after toggles'
            user.PostMessageW(game,0x10,0,0)
            p.wait(timeout=15)
            assert p.returncode==0,p.returncode
        finally:
            if p.poll() is None:p.terminate();p.wait(timeout=5)
            recycle_validation_disks(folder/'saves')
    log=error_log.read_text(errors='replace')
    assert '[CRASH]' not in log and '[ICALL] unresolved' not in log
    assert log.count('[TEXTURE-MODE] 4K;')==2,log[-5000:]
    assert log.count('[TEXTURE-MODE] Classic;')==3,log[-5000:]
    assert '[TEXTURE-PACK] loaded ' in log,'No replacement exercised'
    for p,data in before.items():assert p.read_bytes()==data,str(p)+' changed'
    report=dict(result='passed',scope='brief runtime startup smoke; full scene/art validation pending',
                checked=['F10','held-key repeat','hidden menu','fullscreen','menu checkmark',
                         'pack loading','clean exit','personal config preservation'],
                pack_loads=log.count('[TEXTURE-PACK] loaded '))
    (ROOT/'logs/texture-toggle-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: actual runtime F10, repeats, hidden menu, fullscreen, menu state, pack loading, clean exit and personal settings preservation')

if __name__=='__main__':main()
