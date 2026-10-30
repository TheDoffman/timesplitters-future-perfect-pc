"""Check the actual portable launcher and package contents after ZIP creation."""

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

from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
import subprocess
import time
import zipfile
import argparse
import pc_settings_game as ui

ROOT = _layout_root
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--stage', type=Path, default=ROOT / 'private/playable-package/TimeSplitters Future Perfect')
parser.add_argument('--archive', type=Path, default=ROOT.parent / 'TimeSplitters Future Perfect - Playable.zip')
parser.add_argument('--release', action='store_true')
parser.add_argument('--standalone', action='store_true')
args = parser.parse_args()
STAGE, ARCHIVE = args.stage.resolve(), args.archive.resolve()
if args.release or args.standalone:
    build = json.loads((STAGE / 'BUILD.json').read_text())
    manifest = {p: {'size': (STAGE / p).stat().st_size, 'sha256': digest} for p, digest in build['files'].items()}
else:
    manifest = json.loads((STAGE.parent / 'source-manifest.json').read_text())
with zipfile.ZipFile(ARCHIVE) as archive:
    names = set(archive.namelist())
    for relative, entry in manifest.items():
        name = STAGE.name + '/' + relative
        assert archive.getinfo(name).file_size == entry['size'], name
        if 'sha256' in entry:
            assert hashlib.sha256(archive.read(name)).hexdigest() == entry['sha256'], name
    assert not any(n.lower().endswith(('.iso', '.pdb', '.obj')) for n in names)
    assert not any('/.venv/' in n or '/external/' in n or '/tests/' in n for n in names)
    if args.release:
        assert not any('/Data/' in n or '/Saves/' in n or n.lower().endswith(('.xbe', '.pak', '.img')) for n in names)
print('PASS: ZIP manifest, settings/save data hashes, and development-file exclusions', flush=True)

ps = r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
env = dict(os.environ)
env['PSModulePath'] = (r'C:\Windows\System32\WindowsPowerShell\v1.0\Modules;'
                       r'C:\Program Files\WindowsPowerShell\Modules')
launch_command = ([os.environ['COMSPEC'], '/d', '/c', str(STAGE / 'Play.cmd')] if args.release or args.standalone else
                  [ps, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(STAGE / 'Launch.ps1')])
launcher = subprocess.Popen(launch_command,
                            cwd=STAGE, env=env, creationflags=0x08000000,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
pid = None
game_handle = None
kernel = c.WinDLL('kernel32', use_last_error=True)
kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
kernel.OpenProcess.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
kernel.GetExitCodeProcess.argtypes = [w.HANDLE, c.POINTER(w.DWORD)]
try:
    wanted = str(STAGE / 'Bin/futureperfect_recomp.exe').replace("'", "''")
    for attempt in range(20):
        result = subprocess.run([ps, '-NoProfile', '-Command',
            f"Get-Process futureperfect_recomp -ErrorAction SilentlyContinue | Where-Object {{ $_.Path -eq '{wanted}' }} | Select-Object -ExpandProperty Id"],
            env=env, creationflags=0x08000000, text=True, capture_output=True).stdout.strip()
        if result:
            pid = int(result)
            break
        if launcher.poll() is not None and (not args.release or launcher.returncode != 0):
            raise AssertionError(launcher.communicate())
        time.sleep(.25)
    assert pid, 'Portable game did not launch'
    game_handle = kernel.OpenProcess(0x100000 | 0x1000, False, pid)
    assert game_handle
    if args.release:
        launcher.wait(timeout=5)
        assert launcher.returncode == 0, 'Play.cmd did not exit after launching'
    game = ui.wait_window(pid, 'TimeSplitters FP')
    ui.user.ShowWindow(game, 0)
    time.sleep(15)
    assert kernel.WaitForSingleObject(game_handle, 0) == 258, 'Portable game exited early'
    ui.user.PostMessageW(game, 0x100, 0x71, 0)
    editor = ui.wait_window(pid, 'Keyboard & Mouse -')
    assert not ui.user.IsWindowEnabled(game)
    ui.user.PostMessageW(editor, 0x111, 2, 0)
    time.sleep(.3)
    assert ui.user.IsWindowEnabled(game)
    # Verify video libraries came from this package rather than the development checkout.
    kernel.K32EnumProcessModulesEx.argtypes = [w.HANDLE, c.POINTER(w.HMODULE), w.DWORD, c.POINTER(w.DWORD), w.DWORD]
    kernel.K32GetModuleFileNameExW.argtypes = [w.HANDLE, w.HMODULE, w.LPWSTR, w.DWORD]
    handle = kernel.OpenProcess(0x410, False, pid)
    assert handle
    try:
        modules = (w.HMODULE * 1024)()
        needed = w.DWORD()
        assert kernel.K32EnumProcessModulesEx(handle, modules, c.sizeof(modules), c.byref(needed), 3)
        paths = []
        for module in modules[:needed.value // c.sizeof(w.HMODULE)]:
            path = c.create_unicode_buffer(32768)
            kernel.K32GetModuleFileNameExW(handle, module, path, len(path))
            paths.append(path.value.lower())
        for name in ['avcodec-62.dll', 'avutil-60.dll', 'swresample-6.dll', 'vcruntime140.dll']:
            assert str(STAGE / 'Bin' / name).lower() in paths, name
    finally:
        kernel.CloseHandle(handle)
    ui.user.PostMessageW(game, 0x10, 0, 0)
    assert kernel.WaitForSingleObject(game_handle, 10000) == 0, 'Game did not shut down'
    game_exit = w.DWORD()
    assert kernel.GetExitCodeProcess(game_handle, c.byref(game_exit))
    assert game_exit.value == 0, game_exit.value
    output, errors = launcher.communicate(timeout=10)
    assert launcher.returncode == 0, (launcher.returncode, output, errors)
    log = max((STAGE / 'Logs').glob('*.err'), key=lambda p: p.stat().st_mtime).read_text(errors='replace')
    assert '[CRASH]' not in log and '[ICALL] unresolved' not in log
    assert '[XMV] FFmpeg avcodec-62.dll loaded' in log
    assert str(STAGE / 'Saves') in log
    expected_config = ROOT / ('config/pc-controls.default.json' if args.release or args.standalone else 'config/pc-controls.json')
    assert (STAGE / 'Settings/pc-controls.json').read_bytes() == expected_config.read_bytes()
    print('PASS: standalone launcher, packaged movie DLLs, F2 settings, local saves, and clean shutdown', flush=True)
finally:
    if game_handle:
        if kernel.WaitForSingleObject(game_handle, 0) == 258:
            subprocess.run(['taskkill', '/PID', str(pid), '/F'], creationflags=0x08000000, capture_output=True)
        kernel.CloseHandle(game_handle)
    if launcher.poll() is None:
        launcher.terminate()
        launcher.wait(timeout=5)
