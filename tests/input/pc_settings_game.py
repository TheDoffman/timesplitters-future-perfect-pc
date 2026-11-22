"""Bounded Windows integration check of the real game's PC settings entry points."""

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

ROOT = _layout_root
user = c.WinDLL("user32", use_last_error=True)
user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
user.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
user.GetDlgItem.argtypes = [w.HWND, c.c_int]
user.GetDlgItem.restype = w.HWND
user.GetMenu.argtypes = [w.HWND]
user.GetMenu.restype = w.HMENU
user.GetMenuItemCount.argtypes = [w.HMENU]
user.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
user.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
user.SendMessageW.restype = c.c_ssize_t
user.IsWindowEnabled.argtypes = [w.HWND]
user.GetWindowLongW.argtypes = [w.HWND, c.c_int]
CALLBACK = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
user.EnumWindows.argtypes = [CALLBACK, w.LPARAM]


def windows(pid):
    result = []

    @CALLBACK
    def visit(hwnd, _):
        process = w.DWORD()
        user.GetWindowThreadProcessId(hwnd, c.byref(process))
        if process.value == pid:
            title = c.create_unicode_buffer(256)
            user.GetWindowTextW(hwnd, title, len(title))
            result.append((hwnd, title.value))
        return True

    user.EnumWindows(visit, 0)
    return result


def wait_window(pid, prefix, timeout=12):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        for hwnd, title in windows(pid):
            if title.startswith(prefix):
                return hwnd
        time.sleep(.05)
    raise AssertionError(f"Window did not appear: {prefix}; found {windows(pid)}")


def main():
    exe = ROOT / "build/windows/Release/futureperfect_recomp.exe"
    config = ROOT / "config/pc-controls.default.json"
    original = config.read_bytes()
    controls = json.loads(original)
    env = dict(os.environ)
    for key in list(env):
        if key.startswith(("RECOMP_", "TSFP_")):
            env.pop(key)
    captures = ROOT / "private/captures/pc-settings-integration" / str(time.time_ns())
    captures.mkdir(parents=True, exist_ok=True)
    config = captures / "controls.json"
    config.write_bytes(original)
    env.update(
        RECOMP_GAME_DIR=str(ROOT / "private/game"),
        RECOMP_SAVE_DIR=str(ROOT / "private/test-saves/pc-settings-integration"),
        RECOMP_INPUT_CONFIG=str(config),
        RECOMP_PC_MOUSE=str(int(controls["mouse"]["enabled"])),
        RECOMP_MOUSE_SENSITIVITY=str(controls["mouse"]["sensitivity"]),
        RECOMP_MOUSE_INVERT_Y=str(int(controls["mouse"]["invert_y"])),
        RECOMP_TRACE_BUDGET="0",
        RECOMP_WINDOW_BACKGROUND="1",
        RECOMP_HLE_D3D8_DUMP=str(captures / "frame"),
        RECOMP_HLE_D3D8_DUMP_EVERY="60",
        RECOMP_HLE_D3D8_DUMP_KEEP_LAST="3",
    )
    with (ROOT / "logs/pc-settings-game.out").open("wb") as out, (
        ROOT / "logs/pc-settings-game.err"
    ).open("wb") as err:
        process = subprocess.Popen([str(exe)], cwd=exe.parent, env=env, stdout=out, stderr=err)
        try:
            game = wait_window(process.pid, "TimeSplitters FP")
            if not user.GetMenu(game):
                user.SendMessageW(game, 0x100, 0x70, 0)  # F1 restores saved-hidden bar
                user.SendMessageW(game, 0x101, 0x70, 0)
            assert user.GetMenuItemCount(user.GetMenu(game)) == 1
            original = config.read_bytes()  # F1 itself persists the visibility choice.
            for route in ("F2", "PC Settings menu", "F2 in fullscreen"):
                if route == "F2 in fullscreen":
                    user.PostMessageW(game, 0x104, 0x0d, 1 << 29)  # Alt+Enter
                    until = time.monotonic() + 3
                    while time.monotonic() < until and not (user.GetWindowLongW(game, -16) & 0x80000000):
                        time.sleep(.05)
                    assert user.GetWindowLongW(game, -16) & 0x80000000, "Fullscreen did not activate"
                if route.startswith("F2"):
                    user.PostMessageW(game, 0x100, 0x71, 0)  # WM_KEYDOWN / VK_F2
                else:
                    user.PostMessageW(game, 0x111, 0x7e20, 0)  # WM_COMMAND
                settings = wait_window(process.pid, "Keyboard & Mouse -")
                assert not user.IsWindowEnabled(game), "The settings dialog must own input"
                until = time.monotonic() + 3
                action_count = 0
                while time.monotonic() < until:
                    actions = user.GetDlgItem(settings, 100)
                    action_count = user.SendMessageW(actions, 0x1004, 0, 0)  # LVM_GETITEMCOUNT
                    if action_count == 24:
                        break
                    time.sleep(.05)
                assert action_count == 24, f"Settings action count: {action_count}"
                user.PostMessageW(settings, 0x111, 2, 0)  # Cancel
                until = time.monotonic() + 3
                while time.monotonic() < until and not user.IsWindowEnabled(game):
                    time.sleep(.05)
                assert user.IsWindowEnabled(game), "Game input was not returned after closing settings"
                print(f"PASS: {route} opens all 24 actions; game input is disabled then restored")
                if route == "F2 in fullscreen":
                    user.PostMessageW(game, 0x104, 0x0d, 1 << 29)
            time.sleep(3)
            assert process.poll() is None, "Game exited unexpectedly"
            assert any(captures.glob("frame*.bmp")), "No rendered frame after closing settings"
            assert config.read_bytes() == original, "Cancel modified the user's controls"
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=5)
    log = (ROOT / "logs/pc-settings-game.err").read_text(errors="replace")
    assert "[CRASH]" not in log and "[ICALL] unresolved" not in log
    print("PASS: real game keeps rendering after closing settings; user configuration is unchanged")


if __name__ == "__main__":
    main()
