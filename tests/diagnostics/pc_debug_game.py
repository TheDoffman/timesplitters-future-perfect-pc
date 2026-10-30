"""Exercise Debug settings in the real game using isolated settings and saves."""

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
import base64
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import re
import subprocess
import time

import pc_settings_game as ui

ROOT = _layout_root
user = ui.user
user.SetProcessDPIAware()
user.SetForegroundWindow.argtypes = [w.HWND]
user.GetSubMenu.argtypes = [w.HMENU, c.c_int]
user.GetSubMenu.restype = w.HMENU
user.GetMenuState.argtypes = [w.HMENU, w.UINT, w.UINT]
user.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
user.GetClientRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
user.ClientToScreen.argtypes = [w.HWND, c.POINTER(w.POINT)]
user.GetClipCursor.argtypes = [c.POINTER(w.RECT)]


def window_rect(window):
    rect = w.RECT()
    assert user.GetWindowRect(window, c.byref(rect))
    return rect.left, rect.top, rect.right, rect.bottom


def mouse_confined(window):
    clip, client, origin = w.RECT(), w.RECT(), w.POINT()
    assert user.GetClipCursor(c.byref(clip))
    assert user.GetClientRect(window, c.byref(client))
    assert user.ClientToScreen(window, c.byref(origin))
    return (clip.left, clip.top, clip.right, clip.bottom) == (
        origin.x + client.right // 2, origin.y + client.bottom // 2,
        origin.x + client.right // 2 + 1, origin.y + client.bottom // 2 + 1)


def text(window):
    buffer = c.create_unicode_buffer(8192)
    user.SendMessageW(window, 0x000D, len(buffer), c.cast(buffer, c.c_void_p).value)
    return buffer.value


def wait_for(condition, description, seconds=10):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        if condition():
            return
        time.sleep(.1)
    raise AssertionError(description)


def screenshot(window, path):
    # Capture only this window, even when another application covers it.
    script = r'''
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class DebugCapture { [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; } [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r); [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags); [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); }'
[DebugCapture]::SetProcessDPIAware() | Out-Null
$rect = New-Object DebugCapture+Rect
[DebugCapture]::GetWindowRect([IntPtr]WINDOW_HANDLE, [ref]$rect) | Out-Null
$bitmap = New-Object Drawing.Bitmap ($rect.R-$rect.L),($rect.B-$rect.T)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
try {
    $dc = $graphics.GetHdc()
    try { if (![DebugCapture]::PrintWindow([IntPtr]WINDOW_HANDLE,$dc,2)) { throw 'Window capture failed' } }
    finally { $graphics.ReleaseHdc($dc) }
    $bitmap.Save('OUTPUT_PATH',[Drawing.Imaging.ImageFormat]::Png)
}
finally { $graphics.Dispose(); $bitmap.Dispose() }
'''.replace('WINDOW_HANDLE', str(window)).replace('OUTPUT_PATH', str(path).replace("'", "''"))
    encoded = base64.b64encode(script.encode('utf-16-le')).decode('ascii')
    result = subprocess.run(['powershell.exe', '-NoProfile', '-EncodedCommand', encoded],
                            creationflags=subprocess.CREATE_NO_WINDOW, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def main():
    work = ROOT / 'private/network-validation/host/debug'
    work.mkdir(parents=True, exist_ok=True)
    config = work / 'controls.json'
    config.write_bytes((ROOT / 'config/pc-controls.default.json').read_bytes())
    original = json.loads(config.read_text())
    error_log = work / 'runtime.err'
    save_directory = ROOT / 'private/network-validation/host/saves'
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('RECOMP_', 'TSFP_'))}
    environment.update(RECOMP_GAME_DIR=str(ROOT / 'private/game'),
                       RECOMP_SAVE_DIR=str(save_directory),
                       RECOMP_INPUT_CONFIG=str(config), RECOMP_LOG_FILE=str(error_log),
                       RECOMP_TRACE_BUDGET='0', RECOMP_PC_MOUSE='1')
    exe = ROOT / 'build/windows/Release/futureperfect_recomp.exe'
    with (work / 'runtime.out').open('wb') as output, error_log.open('wb') as errors:
        process = subprocess.Popen([str(exe)], cwd=exe.parent, env=environment,
                                   stdout=output, stderr=errors,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            game = ui.wait_window(process.pid, 'TimeSplitters FP')
            time.sleep(2)
            screenshot(game, work / 'watermark-only.png')
            user.PostMessageW(game, 0x111, 0x7e21, 0)
            debug = ui.wait_window(process.pid, 'Debug -')
            assert not user.IsWindowEnabled(game)
            wait_for(lambda: re.search(r'Game: [1-9][0-9]*\.', text(user.GetDlgItem(debug, 204))),
                     'No live FPS statistics')
            assert 'unavailable' not in text(user.GetDlgItem(debug, 205)).lower()
            assert 'latency: offline' in text(user.GetDlgItem(debug, 204)).lower()
            for item in (200, 201, 202, 203, 209):
                user.SendMessageW(user.GetDlgItem(debug, item), 0xF1, 1, 0)
            user.SetForegroundWindow(debug)
            time.sleep(.3)
            screenshot(debug, work / 'debug-dialog.png')
            user.PostMessageW(debug, 0x111, 1, 0)
            wait_for(lambda: user.IsWindowEnabled(game), 'Debug Save did not return game input')
            updated = json.loads(config.read_text())
            assert all(updated['debug'].values())
            assert updated['controllers'] == original['controllers']
            assert updated['mouse'] == original['mouse']
            wait_for(lambda: '[PC-DEBUG]' in error_log.read_text(errors='replace'),
                     'Performance logging did not apply live')
            user.SetForegroundWindow(game)
            time.sleep(.5)
            screenshot(game, work / 'debug-overlay.png')
            menu = user.GetSubMenu(user.GetMenu(game), 0)

            def fullscreen():
                return not (user.GetWindowLongW(game, -16) & 0x00C00000)

            def check_mode(wanted):
                wait_for(lambda: fullscreen() == wanted, 'Fullscreen mode did not change')
                assert bool(user.GetMenuState(menu, 0x7e22, 0) & 8) == wanted

            # Shortcuts are window messages, without synthesizing desktop keyboard input.
            for full in (False, True):
                if full:
                    user.PostMessageW(game, 0x111, 0x7e22, 0)
                    check_mode(True)
                for repeat in (0, 1 << 30):
                    user.PostMessageW(game, 0x100, 0x7A, repeat)  # F11 must not toggle
                    user.PostMessageW(game, 0x101, 0x7A, 0)
                    time.sleep(.2)
                    assert fullscreen() == full, 'F11 changed fullscreen mode'
                    check_mode(full)
                user.PostMessageW(game, 0x100, 0x72, 0)  # F3 is no longer a shortcut
                user.PostMessageW(game, 0x101, 0x72, 0)
                time.sleep(.2)
                assert not any(title.startswith('Debug -') for _, title in ui.windows(process.pid))
                # F1 changes the menu without moving the window or recapturing
                # a mouse released with Escape. A held key must not flicker it.
                user.PostMessageW(game, 0x100, 0x1B, 0)
                user.PostMessageW(game, 0x101, 0x1B, 0)
                wait_for(lambda: not mouse_confined(game), 'Escape did not release mouse')
                bounds = window_rect(game)
                user.PostMessageW(game, 0x100, 0x70, 0)
                wait_for(lambda: not user.GetMenu(game), 'F1 did not hide menu bar')
                user.SendMessageW(game, 0x100, 0x70, 1 << 30)
                user.SendMessageW(game, 0x101, 0x70, 0)
                assert not user.GetMenu(game), 'Held F1 restored menu bar'
                assert window_rect(game) == bounds, 'Menu toggle changed window bounds'
                assert not mouse_confined(game), 'F1 recaptured the released mouse'
                user.PostMessageW(game, 0x100, 0x71, 0)  # F2 works without menu bar
                keyboard = ui.wait_window(process.pid, 'Keyboard & Mouse -')
                user.PostMessageW(keyboard, 0x111, 2, 0)
                wait_for(lambda: user.IsWindowEnabled(game), 'Hidden-menu F2 did not close')
                user.PostMessageW(game, 0x100, 0x7B, 0)  # F12
                debug = ui.wait_window(process.pid, 'Debug -')
                assert not user.IsWindowEnabled(game)
                user.PostMessageW(debug, 0x111, 2, 0)
                wait_for(lambda: user.IsWindowEnabled(game), 'F12 Debug did not release game input')
                user.PostMessageW(game, 0x100, 0x70, 0)
                user.PostMessageW(game, 0x101, 0x70, 0)
                wait_for(lambda: bool(user.GetMenu(game)), 'F1 did not restore menu bar')
                check_mode(full)
                assert window_rect(game) == bounds
                user.PostMessageW(game, 0x111, 0x7e23, 0)  # Hide Menu Bar command
                wait_for(lambda: not user.GetMenu(game), 'Menu command did not hide bar')
                user.PostMessageW(game, 0x201, 1, (100 << 16) | 100)  # recapture click
                user.PostMessageW(game, 0x202, 0, (100 << 16) | 100)
                wait_for(lambda: mouse_confined(game), 'Click did not recapture mouse')
                user.PostMessageW(game, 0x100, 0x70, 0)
                user.PostMessageW(game, 0x101, 0x70, 0)
                wait_for(lambda: bool(user.GetMenu(game)), 'F1 did not restore hidden menu')
                wait_for(lambda: mouse_confined(game), 'Menu resize did not update mouse clipping')

            # Keyboard & Mouse no longer contains a Debug button.
            user.PostMessageW(game, 0x100, 0x71, 0)
            keyboard = ui.wait_window(process.pid, 'Keyboard & Mouse -')
            assert not user.GetDlgItem(keyboard, 111), 'Removed Debug button is still present'
            user.PostMessageW(keyboard, 0x111, 2, 0)
            wait_for(lambda: user.IsWindowEnabled(game), 'F2 Cancel did not return game input')
            user.PostMessageW(game, 0x100, 0x7B, 0)
            debug = ui.wait_window(process.pid, 'Debug -')
            assert not user.IsWindowEnabled(game)
            before_cancel = config.read_bytes()
            for item in (200, 201, 202, 203, 209):
                assert user.SendMessageW(user.GetDlgItem(debug, item), 0xF0, 0, 0) == 1
                user.SendMessageW(user.GetDlgItem(debug, item), 0xF1, 0, 0)
            user.PostMessageW(debug, 0x111, 2, 0)
            wait_for(lambda: user.IsWindowEnabled(game), 'Debug Cancel did not return game input')
            assert config.read_bytes() == before_cancel
            user.PostMessageW(game, 0x111, 0x7e22, 0)
            check_mode(False)
            for full in (True, False):
                user.PostMessageW(game, 0x104, 0x0D, 1 << 29)  # Alt+Enter still works
                check_mode(full)
            assert process.poll() is None
            user.PostMessageW(game, 0x10, 0, 0)
            assert process.wait(timeout=10) == 0
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            subprocess.run(['powershell.exe', '-NoProfile', '-File', str(ROOT/'tools/network/cleanup-network-tests.ps1')], check=True)
    assert '[CRASH]' not in error_log.read_text(errors='replace')
    print('PASS: F1/menu bar toggle, Escape/click mouse capture, hidden-menu F2/F12, Debug settings, fullscreen controls/checkmarks, and clean exit')
    print(f'Screenshots and isolated test artifacts: {work}')


if __name__ == '__main__':
    main()
