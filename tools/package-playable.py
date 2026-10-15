"""Stage the playable files and personal settings/saves, without build tools."""
from pathlib import Path
import hashlib
import json
import shutil
import argparse
import os

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--crt-dll', type=Path, help='Path to the x64 MSVC vcruntime140.dll redistributable')
args = parser.parse_args()
crt = args.crt_dll
if crt is None:
    vs = Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'Microsoft Visual Studio'
    candidates = sorted(vs.glob('*/*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT/vcruntime140.dll'))
    if candidates:
        crt = candidates[-1]
if crt is None or not crt.is_file():
    parser.error('Supply --crt-dll with the x64 Visual C++ vcruntime140.dll redistributable.')
ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / 'private/playable-package/TimeSplitters Future Perfect'
STAGE.mkdir(parents=True, exist_ok=False)
manifest = {}

def copy(source, relative):
    target = STAGE / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    entry = {'size': source.stat().st_size}
    if entry['size'] < 16 * 1024 * 1024:
        entry['sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
    manifest[str(relative).replace('\\', '/')] = entry

for source in (ROOT / 'private/game').rglob('*'):
    if source.is_file() and source.suffix.lower() != '.json':
        copy(source, Path('Data') / source.relative_to(ROOT / 'private/game'))
copy(ROOT / 'build/windows/Release/futureperfect_recomp.exe', Path('Bin/futureperfect_recomp.exe'))
for name in ['avcodec-62.dll', 'avutil-60.dll', 'swresample-6.dll']:
    copy(ROOT / 'external/ts3FPrecomp/third_party/ffmpeg/bin' / name, Path('Bin') / name)
copy(crt, Path('Bin/vcruntime140.dll'))
copy(ROOT / 'config/pc-controls.json', Path('Settings/pc-controls.json'))
for source in (ROOT / 'private/saves').rglob('*'):
    if source.is_file():
        copy(source, Path('Saves') / source.relative_to(ROOT / 'private/saves'))
for source, name in [
    ('external/ts3FPrecomp/LICENSE', 'Runtime-GPL-3.0.txt'),
    ('external/ts3FPrecomp/third_party/ffmpeg/LICENSE.txt', 'FFmpeg.txt'),
    ('external/ts3FPrecomp/third_party/SDL/LICENSE.txt', 'SDL.txt'),
]:
    copy(ROOT / source, Path('Licenses') / name)
copy(ROOT / 'tools/playable-launch.ps1', Path('Launch.ps1'))
(STAGE / 'Play.cmd').write_text('@echo off\n"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch.ps1"\nif errorlevel 1 pause\n', encoding='ascii')
(STAGE / 'README.txt').write_text('''TimeSplitters: Future Perfect - Windows playable package

Extract the entire ZIP to a writable folder, then double-click Play.cmd.
Requires 64-bit Windows 10/11. Allow approximately 9 GB for extraction.
Python, Visual Studio, the ISO, and the development workspace are not needed.

Your keyboard/mouse settings are in Settings/pc-controls.json.
Your existing game saves are in Saves. Keep that folder when moving the game.
Press F2, or PC Settings > Keyboard & Mouse, to change bindings.
Pause a mission before opening the separate settings window.
F1 shows/hides the PC Settings menu bar; Alt+Enter switches fullscreen.
F10 switches Classic/4K textures live (optional DDS pack in Textures).
Escape releases the mouse; click the game to recapture it.
Button prompts show your assigned keys and switch to controller icons when you
use a controller. The first unassigned controller also controls player 1.

Includes the current experimental PC build with the original game menus.
Logs are written locally to Logs when the game starts.

Runtime: https://github.com/nemonicicon/ts3FPrecomp
Bundled movie decoder: FFmpeg 8.1 shared build (BtbN).
License notices are in Licenses.
''', encoding='utf-8')
(STAGE.parent / 'source-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(f'Staged {len(manifest)} files plus Play.cmd and README.txt: {STAGE}', flush=True)
