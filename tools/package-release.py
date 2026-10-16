"""Build a public Windows prerelease ZIP without game data or personal files."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='v0.1.0-alpha.1')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/releases')
    args = parser.parse_args()
    if not re.fullmatch(r'v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?', args.version):
        parser.error('Use a semantic release version, such as v1.2.3 or v1.2.3-alpha.1.')
    if '-' in args.version and any(part.isdecimal() and len(part) > 1 and part[0] == '0'
                                   for part in args.version.split('-', 1)[1].split('.')):
        parser.error('Numeric prerelease identifiers must not contain leading zeros.')
    name = f'TimeSplitters-Future-Perfect-PC-{args.version}-windows-x64'
    stage = args.output.resolve() / name
    stage.mkdir(parents=True, exist_ok=False)
    upstream = ROOT / 'external/ts3FPrecomp'

    # Compile the dependency-free launcher for every package, so a stale local
    # launcher binary cannot silently enter a new release.
    powershell = Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    subprocess.run([str(powershell), '-NoProfile', '-ExecutionPolicy', 'Bypass',
                    '-File', str(ROOT / 'tools/build-launcher.ps1')], check=True)

    def copy(source, dest):
        target = stage / dest
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    copy(ROOT / 'build/windows/Release/futureperfect_recomp.exe', 'Bin/futureperfect_recomp.exe')
    copy(ROOT / 'build/launcher/TimeSplitters Launcher.exe', 'TimeSplitters Launcher.exe')
    copy(ROOT / 'build/launcher/Bin/TimeSplitters.App.exe', 'Bin/TimeSplitters.App.exe')
    copy(ROOT / 'build/launcher/Support/Launcher/manifest.json', 'Support/Launcher/manifest.json')
    for dll in ['avcodec-62.dll', 'avutil-60.dll', 'swresample-6.dll']:
        copy(upstream / 'third_party/ffmpeg/bin' / dll, 'Bin/' + dll)
    vs = Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'Microsoft Visual Studio'
    candidates = sorted(vs.glob('*/*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT/vcruntime140.dll'))
    if not candidates:
        raise RuntimeError('The x64 MSVC runtime redistributable was not found.')
    copy(candidates[-1], 'Bin/vcruntime140.dll')
    copy(ROOT / 'config/pc-controls.default.json', 'Settings/pc-controls.json')
    copy(ROOT / 'tools/playable-launch.ps1', 'Launch.ps1')
    copy(ROOT / 'tools/start-game.ps1', 'Start-game.ps1')
    for file in ['ImportGame.cs', 'Import-game.ps1', 'Play.cmd']:
        copy(ROOT / 'tools/release' / file, file)
    for source, dest in [
        ('LICENSE', 'Runtime-GPL-3.0.txt'), ('LICENSE.upstream-MIT', 'Runtime-upstream-MIT.txt'),
        ('NOTICE', 'Runtime-NOTICE.txt'), ('LICENSES/LGPL-2.1.txt', 'Runtime-LGPL-2.1.txt'),
        ('third_party/SDL/LICENSE.txt', 'SDL.txt'), ('third_party/ffmpeg/LICENSE.txt', 'FFmpeg-LGPL-3.0.txt'),
    ]:
        copy(upstream / source, 'Licenses/' + dest)
    (stage / 'README.txt').write_text(f'''TimeSplitters: Future Perfect PC {args.version} - experimental prerelease

Requires 64-bit Windows 10/11 and your own US Xbox game files.
Extract this entire ZIP to a writable folder. Open TimeSplitters Launcher.exe
(Play.cmd opens the same launcher). Choose Locate ISO or Use extracted files
to select your Xbox ISO or an extracted folder containing
default.xbe, pak, and xmv. Setup validates the supported executable and copies
the game files to Data. Allow at least 5 GB of free space for setup.
After setup, click Play Game in the launcher. The original ISO is no longer needed.
No Python, Visual Studio, or emulator installation is required.

The launcher checks published GitHub Releases at startup. Main-branch pushes
do not update your game. Stable excludes prereleases; opt into Preview for
published alpha/beta builds. Click Install/Update to download and verify a ZIP.
Before its main window opens, a small splash checks for launcher-only updates
and installs them automatically. This check has an eight-second timeout and
an Open installed launcher button. Launcher and game versions are independent.
Close the game before updating. The launcher reopens when installation finishes.
Saves, settings, Data and optional Textures are kept. Restore previous returns
to the preceding installed release. Launcher diagnostics: .launcher/launcher.log.
Keep .launcher for rollback and interrupted-update recovery.

WASD: move. Mouse: look. Left/right mouse: fire/aim. R: reload.
E/Space/Enter: use/confirm. Escape: pause/release mouse. Click: recapture mouse.
F1: show/hide the PC Settings menu bar (also PC Settings > Hide Menu Bar).
F2: keyboard/mouse bindings and sensitivity. F12: Debug settings.
F10: switch Classic/4K textures live (optional DDS pack in Textures).
Alt+Enter: fullscreen (also available under PC Settings).
Button prompts show your first assigned keyboard/mouse binding. Controller input
restores controller icons; using the keyboard/mouse switches them back.
The first controller not assigned to another player also controls player 1.
Pause before opening F2: the editor blocks input but does not pause the game.
Settings and paused Options include Display settings for render resolution.
Confirmed display choices are saved in Settings/pc-display.ini.

Settings: Settings/pc-controls.json. Saves: Saves. Diagnostics: Logs.
Use the launcher's Update release button for upgrades. For manual upgrades,
keep Data, Settings, Saves and Textures; do not overwrite personal settings.

This is an experimental native recompilation, not the original source code.
Startup and short first-mission gameplay have been checked; a full campaign
playthrough has not. Expect compatibility issues. Report the steps to reproduce
and relevant Logs at https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues

Game data, the ISO, saves, and personal settings are not included in this ZIP.
Supported default.xbe SHA-256:
3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc

Source and build instructions:
https://github.com/TheDoffman/timesplitters-future-perfect-pc
Runtime source: https://github.com/nemonicicon/ts3FPrecomp/tree/d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845
SDL source: https://github.com/libsdl-org/SDL/tree/fa2c02bb6e21974a89ea9824bc53c9932abe5f9c
FFmpeg 8.1 shared LGPL build: https://github.com/BtbN/FFmpeg-Builds
FFmpeg source: https://github.com/FFmpeg/FFmpeg/tree/n8.1
Third-party notices are in Licenses. Microsoft Visual C++ runtime is bundled.
''', encoding='utf-8')
    info = {
        'version': args.version,
        'launcher_version': json.loads((ROOT / 'build/launcher/Support/Launcher/manifest.json').read_text(encoding='utf-8-sig'))['version'],
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'upstream_commit': subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip(),
        'ffmpeg_archive_sha256': '1689da96399f372cfee985b3f2b9d74685892078059bc37764388f49e3118bdb',
        'files': {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in sorted(stage.rglob('*')) if p.is_file()},
    }
    (stage / 'BUILD.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
    archive_path = stage.with_name(name + '.zip')
    with zipfile.ZipFile(archive_path, 'x', zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        for p in sorted(stage.rglob('*')):
            if p.is_file():
                archive.write(p, name + '/' + p.relative_to(stage).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
        assert not any('/Data/' in n or '/Saves/' in n or n.lower().endswith(('.iso', '.xbe', '.pak', '.img', '.pdb')) for n in archive.namelist())
    digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
    archive_path.with_suffix('.zip.sha256').write_text(f'{digest}  {archive_path.name}\n', encoding='ascii')
    print(json.dumps({'archive': str(archive_path), 'bytes': archive_path.stat().st_size, 'sha256': digest, 'stage': str(stage)}, indent=2))


if __name__ == '__main__':
    main()
