"""Build real Windows icon resources with tiny executables, never run a game."""


import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
SIZES = {16, 24, 32, 48, 64, 128, 256}
pytestmark = pytest.mark.skipif(os.name != 'nt', reason='Windows icon resource toolchain')


def build(output, *args, success=True):
    shell = Path(os.environ['WINDIR'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    env = dict(os.environ)
    env['PSModulePath'] = str(shell.parent/'Modules')
    result = subprocess.run([str(shell), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                             str(ROOT/'tools/build/build-game-icon.ps1'), '-Output', str(output), *args],
                            capture_output=True, text=True, errors='replace', env=env, timeout=30)
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result


def frames(path):
    from PIL import Image
    import io
    data = path.read_bytes()
    assert struct.unpack_from('<HHH', data) == (0, 1, len(SIZES))
    result = {}
    for index in range(len(SIZES)):
        width, height, _, _, planes, bits, size, offset = struct.unpack_from('<BBBBHHII', data, 6+index*16)
        width, height = width or 256, height or 256
        assert width == height and planes == 1 and bits == 32
        payload = data[offset:offset+size]
        assert len(payload) == size
        image = Image.open(io.BytesIO(payload))
        assert image.size == (width, height) and image.mode == 'RGBA'
        alpha = image.getchannel('A')
        assert alpha.getextrema() == (0, 255)  # Transparent outside, opaque face.
        result[width] = payload
    assert set(result) == SIZES
    return result


def test_fallback_has_all_transparent_sizes_and_matching_provenance(tmp_path):
    build(tmp_path, '-Fallback')
    frames(tmp_path/'game.ico')
    record = json.loads((tmp_path/'game-icon-build.json').read_text())
    assert record['mode'] == 'tsfp-monogram' and set(record['sizes']) == SIZES
    assert record['icon_sha256'] == hashlib.sha256((tmp_path/'game.ico').read_bytes()).hexdigest()
    assert 'logo_sha256' not in record


def test_original_logo_is_preserved_and_small_sizes_keep_the_compact_mark(tmp_path):
    from PIL import Image, ImageDraw
    logo = tmp_path/'synthetic logo.png'
    image = Image.new('RGBA', (500, 187))
    ImageDraw.Draw(image).rectangle((12, 12, 488, 175), fill=(245, 245, 245, 255))
    image.save(logo)
    before = hashlib.sha256(logo.read_bytes()).hexdigest()
    build(tmp_path/'logo', '-LogoPath', str(logo), '-RequireLogo')
    build(tmp_path/'fallback', '-Fallback')
    branded, fallback = frames(tmp_path/'logo/game.ico'), frames(tmp_path/'fallback/game.ico')
    assert hashlib.sha256(logo.read_bytes()).hexdigest() == before
    assert all(branded[size] == fallback[size] for size in SIZES if size <= 64)
    assert all(branded[size] != fallback[size] for size in (128, 256))
    assert json.loads((tmp_path/'logo/game-icon-build.json').read_text())['logo_sha256'] == before


def test_required_missing_logo_does_not_replace_an_existing_icon(tmp_path):
    icon = tmp_path/'game.ico'
    icon.write_bytes(b'keep existing icon')
    build(tmp_path, '-LogoPath', str(tmp_path/'missing.png'), '-RequireLogo', success=False)
    assert icon.read_bytes() == b'keep existing icon'


def test_subtitle_has_its_own_larger_readable_line(tmp_path):
    from PIL import Image, ImageDraw
    import io
    logo = tmp_path/'subtitle marker.png'
    image = Image.new('RGBA', (500, 187))
    ImageDraw.Draw(image).rectangle((263, 150, 490, 171), fill=(255, 0, 0, 255))
    image.save(logo)
    build(tmp_path/'output', '-LogoPath', str(logo), '-RequireLogo')
    variants = frames(tmp_path/'output/game.ico')
    for size in (128, 256):
        pixels = Image.open(io.BytesIO(variants[size])).load()
        red = [(x, y) for y in range(int(size*.55), size) for x in range(size)
               if pixels[x, y][0] > 200 and pixels[x, y][1] < 80 and pixels[x, y][2] < 80]
        assert red
        assert max(x for x, y in red)-min(x for x, y in red) >= size*.75
        assert max(y for x, y in red)-min(y for x, y in red) >= size*.055


def test_real_cmake_rc_build_embeds_every_icon_frame_in_a_unicode_path(tmp_path):
    import pefile
    from native_compiler import native_compiler
    source = tmp_path/'game with spaces \u00fc'
    source.mkdir()
    _, env = native_compiler(tmp_path)
    cmake = None
    if importlib.util.find_spec('cmake'):
        import cmake as package
        cmake = str(Path(package.CMAKE_BIN_DIR)/'cmake.exe')
    cmake = cmake or shutil.which('cmake', path=env.get('PATH'))
    assert cmake, 'CMake is required for the real resource-link test'
    ninja = shutil.which('ninja', path=env.get('PATH'))
    if not ninja and importlib.util.find_spec('ninja'):
        import ninja as package
        ninja = str(Path(package.BIN_DIR)/'ninja.exe')
    assert ninja, 'Ninja is required to preserve Unicode resource paths'
    output = source/'icon output'
    build(output)  # Use the real private wordmark locally; CI exercises the fallback.
    patch = (ROOT/'patches/platform/1150-game-icon.patch').read_text()
    snippet = '\n'.join(line[1:] for line in patch.splitlines() if line.startswith('+') and not line.startswith('+++'))
    (source/'main.c').write_text('int main(void) { return 0; }\n')
    (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(game_icon_probe C)\n'
        'add_executable(${PROJECT_NAME} main.c)\n'+snippet+'\n', encoding='utf-8')
    binary = source/'build'
    result = subprocess.run([cmake, '-S', str(source), '-B', str(binary), '-G', 'Ninja',
                             '-DCMAKE_MAKE_PROGRAM='+ninja, '-DCMAKE_BUILD_TYPE=Release', '-DTSFP_GAME_ICON_RESOURCE='+str(output/'game-icon.rc')],
                            capture_output=True, text=True, errors='replace', env=env, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([cmake, '--build', str(binary)], capture_output=True, text=True,
                            errors='replace', env=env, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    expected = set(frames(output/'game.ico').values())
    with pefile.PE(str(binary/'game_icon_probe.exe')) as pe:
        icons = next(item for item in pe.DIRECTORY_ENTRY_RESOURCE.entries if item.id == 3)
        payloads = [entry.directory.entries[0].data.struct for entry in icons.directory.entries]
        assert {pe.get_data(item.OffsetToData, item.Size) for item in payloads} == expected
        groups = next(item for item in pe.DIRECTORY_ENTRY_RESOURCE.entries if item.id == 14)
        assert len(groups.directory.entries) == 1 and groups.directory.entries[0].id == 1
        item = groups.directory.entries[0].directory.entries[0].data.struct
        assert struct.unpack_from('<HHH', pe.get_data(item.OffsetToData, item.Size)) == (0, 1, len(SIZES))
    # Ask Windows to load each icon size. DATAFILE mode never executes the stub.
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    user = ctypes.WinDLL('user32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = wintypes.HMODULE
    kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
    user.LoadImageW.argtypes = [wintypes.HINSTANCE, ctypes.c_void_p, wintypes.UINT,
                               ctypes.c_int, ctypes.c_int, wintypes.UINT]
    user.LoadImageW.restype = wintypes.HANDLE
    user.DestroyIcon.argtypes = [wintypes.HANDLE]
    module = kernel.LoadLibraryExW(str(binary/'game_icon_probe.exe'), None, 0x22)
    assert module, ctypes.get_last_error()
    try:
        for size in SIZES:
            icon = user.LoadImageW(module, ctypes.c_void_p(1), 1, size, size, 0)
            assert icon, (size, ctypes.get_last_error())
            assert user.DestroyIcon(icon)
    finally:
        assert kernel.FreeLibrary(module)


def test_multiplayer_icon_is_packaged_and_used_by_both_windows(tmp_path):
    import sys
    sys.path.insert(0, str(ROOT/'tools/network'))
    from bundle_online import bundle_icon
    copied = {}
    def copy(source, destination):
        target = tmp_path/destination
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        copied[destination] = target
    bundle_icon(ROOT, copy)
    icon = copied['Support/Online/multiplayer.ico']
    variants = frames(icon)
    assert len(set(variants.values())) == len(SIZES)
    # Exercise the packaged lookup in its own Tcl interpreter. No game data,
    # settings, network operations or global desktop preferences are involved.
    for name in ('online_theme.py',):
        shutil.copy2(ROOT/'tools/network'/name, icon.parent/name)
    probe = tmp_path/'probe.py'
    probe.write_text(
        'import sys, tkinter as tk\n'
        'sys.path.insert(0,sys.argv[1])\n'
        'from online_theme import OnlineTheme\n'
        'import ctypes\n'
        'from ctypes import wintypes as w\n'
        'user=ctypes.windll.user32\n'
        'user.GetAncestor.argtypes=[w.HWND,w.UINT];user.GetAncestor.restype=w.HWND\n'
        'user.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];user.SendMessageW.restype=w.LPARAM\n'
        'def icon(window):\n'
        '    window.update()\n'
        '    return user.SendMessageW(user.GetAncestor(window.winfo_id(),2),0x7f,1,0)\n'
        'root=tk.Tk();original=icon(root)\n'
        'theme=OnlineTheme(root,lambda:True)\n'
        'assert icon(root) and icon(root)!=original\n'
        'popup=tk.Toplevel(root);theme.track(popup)\n'
        'assert icon(popup)\n'
        'root.destroy()\n', encoding='utf-8')
    result = subprocess.run([sys.executable, str(probe), str(icon.parent), str(icon)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
