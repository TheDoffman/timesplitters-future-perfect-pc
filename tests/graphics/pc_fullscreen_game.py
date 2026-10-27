"""Check that the desktop actually displays new game frames across fullscreen toggles.

Run on an unlocked Windows desktop; the game must remain in front during capture.
Uses isolated controls/saves and captures the screen, not the renderer's frame dump.
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
from ctypes import wintypes as w
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time

import pc_settings_game as ui

ROOT = _layout_root
user = ui.user
gdi = c.WinDLL('gdi32', use_last_error=True)
user.SetProcessDPIAware()
user.SetForegroundWindow.argtypes = [w.HWND]
user.GetForegroundWindow.restype = w.HWND
user.GetClientRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
user.ClientToScreen.argtypes = [w.HWND, c.POINTER(w.POINT)]
user.GetDC.argtypes = [w.HWND]
user.GetDC.restype = w.HDC
user.ReleaseDC.argtypes = [w.HWND, w.HDC]
gdi.CreateCompatibleDC.argtypes = [w.HDC]
gdi.CreateCompatibleDC.restype = w.HDC
gdi.CreateDIBSection.argtypes = [w.HDC, c.c_void_p, w.UINT, c.POINTER(c.c_void_p), w.HANDLE, w.DWORD]
gdi.CreateDIBSection.restype = w.HANDLE
gdi.SelectObject.argtypes = [w.HDC, w.HANDLE]
gdi.SelectObject.restype = w.HANDLE
gdi.StretchBlt.argtypes = [w.HDC, c.c_int, c.c_int, c.c_int, c.c_int,
                          w.HDC, c.c_int, c.c_int, c.c_int, c.c_int, w.DWORD]
gdi.DeleteObject.argtypes = [w.HANDLE]
gdi.DeleteDC.argtypes = [w.HDC]


def capture(window, path):
    rect, origin = w.RECT(), w.POINT()
    assert user.GetClientRect(window, c.byref(rect))
    assert user.ClientToScreen(window, c.byref(origin))
    width, height = 320, 180
    header = struct.pack('<IiiHHIIiiII', 40, width, -height, 1, 32, 0,
                         width * height * 4, 0, 0, 0, 0)
    info = c.create_string_buffer(header)
    bits = c.c_void_p()
    screen = user.GetDC(None)
    dc = gdi.CreateCompatibleDC(screen)
    bitmap = gdi.CreateDIBSection(screen, info, 0, c.byref(bits), None, 0)
    assert bitmap and dc
    old = gdi.SelectObject(dc, bitmap)
    try:
        assert gdi.StretchBlt(dc, 0, 0, width, height, screen,
                              origin.x, origin.y, rect.right, rect.bottom, 0x00CC0020)
        gdi.GdiFlush()
        pixels = c.string_at(bits, width * height * 4)
        path.write_bytes(struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
                         + header + pixels)
        # Ignore menu, diagnostics, borders and letterboxing. Compare game pixels.
        return b''.join(pixels[(y * width + 80) * 4:(y * width + 240) * 4]
                        for y in range(35, 155))
    finally:
        gdi.SelectObject(dc, old)
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(dc)
        user.ReleaseDC(None, screen)


def check_motion(window, work, label):
    user.SetForegroundWindow(window)
    time.sleep(.5)
    previous = None
    largest = 0
    for index in range(16):
        assert user.GetForegroundWindow() == window, 'Game lost foreground during screen capture'
        pixels = capture(window, work / f'{label}-{index:02}.bmp')
        if previous is not None:
            changed = sum(abs(a - b) > 12 for a, b in zip(previous, pixels)) / len(pixels)
            largest = max(largest, changed)
        previous = pixels
        time.sleep(.4)
    print(f'{label}: largest displayed-frame change {largest:.1%}', flush=True)
    assert largest > .01, f'{label}: displayed game video did not animate'


def main():
    tag = str(time.time_ns())
    work = ROOT / 'private/captures/fullscreen' / tag
    work.mkdir(parents=True)
    config = work / 'controls.json'
    config.write_bytes((ROOT / 'config/pc-controls.default.json').read_bytes())
    saves = ROOT / 'private/test-saves/fullscreen' / tag
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    env.update(RECOMP_GAME_DIR=str(ROOT / 'private/game'), RECOMP_SAVE_DIR=str(saves),
               RECOMP_INPUT_CONFIG=str(config), RECOMP_TRACE_BUDGET='0')
    exe = ROOT / 'build/windows/Release/futureperfect_recomp.exe'
    print(f'Fullscreen test artifacts: {work}', flush=True)
    with (work / 'runtime.out').open('wb') as output, (work / 'runtime.err').open('wb') as errors:
        process = subprocess.Popen([str(exe)], cwd=exe.parent, env=env, stdout=output,
                                   stderr=errors, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            window = ui.wait_window(process.pid, 'TimeSplitters FP')
            time.sleep(2)
            check_motion(window, work, 'windowed')
            for name, message, key, flags in (
                ('menu', 0x111, 0x7e22, 0),
                ('alt-enter', 0x104, 0x0D, 1 << 29),
            ):
                if name == 'alt-enter':
                    user.SendMessageW(window, 0x100, 0x70, 0)  # F1 hides menu bar
                    user.SendMessageW(window, 0x101, 0x70, 0)
                    assert not user.GetMenu(window)
                for fullscreen in (True, False):
                    user.PostMessageW(window, message, key, flags)
                    until = time.monotonic() + 5
                    while time.monotonic() < until:
                        if (not (user.GetWindowLongW(window, -16) & 0x00C00000)) == fullscreen:
                            break
                        time.sleep(.1)
                    assert (not (user.GetWindowLongW(window, -16) & 0x00C00000)) == fullscreen, f"{name}: window style did not change"
                    if name == 'alt-enter':
                        assert not user.GetMenu(window), 'Fullscreen transition restored hidden menu'
                    check_motion(window, work, f'{name}-{"fullscreen" if fullscreen else "windowed"}')
            user.PostMessageW(window, 0x10, 0, 0)
            assert process.wait(timeout=10) == 0
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            if saves.is_dir():
                resolved = saves.resolve()
                allowed = (ROOT / 'private/test-saves/fullscreen').resolve()
                assert resolved != allowed and resolved.is_relative_to(allowed)
                shutil.rmtree(resolved)
    log = (work / 'runtime.err').read_text(errors='replace')
    assert '[CRASH]' not in log
    assert '[RHI] ERROR:' not in log, 'Direct3D reported an error; see runtime.err'
    print('PASS: visible game animation in fullscreen and windowed via PC Settings and Alt+Enter')


if __name__ == '__main__':
    main()
