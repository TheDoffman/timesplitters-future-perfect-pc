"""Build a public Windows game release ZIP without game data or personal files."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import zipfile
from bundle_online import bundle
from release_channels import identity as release_identity
from build_textures import ensure_pack, optional_identity, zip_entries, write_zip, write_source_files

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='v'+(ROOT/'VERSION').read_text().strip())
    parser.add_argument('--output', type=Path, default=ROOT / 'build/releases')
    parser.add_argument('--with-textures', action='store_true', help='Explicitly bundle the audited enhanced pack (offline distribution)')
    parser.add_argument('--without-textures', action='store_true', help='Make a smaller package without enhanced textures')
    parser.add_argument('--texture-pack', type=Path, help='Use this audited compiled pack instead of the committed repository pack')
    args = parser.parse_args()
    try:
        release_identity(args.version, publishing=True)
    except ValueError as error:
        parser.error(str(error))
    if args.without_textures and (args.texture_pack or args.with_textures):
        parser.error('--texture-pack cannot be combined with --without-textures')
    if not re.fullmatch(r'v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?', args.version):
        parser.error('Use a semantic release version, such as v1.2.3 or v1.2.3-beta.1.')
    if '-' in args.version and any(part.isdecimal() and len(part)>1 and part[0]=='0' for part in args.version.split('-',1)[1].split('.')):
        parser.error('Numeric prerelease identifiers must not contain leading zeros.')
    name = f'TimeSplitters-Future-Perfect-PC-{args.version}-windows-x64'
    # Only a successfully built executable with matching content can be packaged.
    subprocess.run([os.sys.executable, str(ROOT/'tools/network-identity.py'), '--verify',
                    '--executable', str(ROOT/'build/windows/Release/futureperfect_recomp.exe')], check=True)
    texture_rows, textures = [], optional_identity(ROOT)
    if args.with_textures or args.texture_pack:
        _, texture_rows, textures = ensure_pack(ROOT, args.texture_pack)
    stage = args.output.resolve() / name
    stage.mkdir(parents=True, exist_ok=False)
    upstream = ROOT / 'external/ts3FPrecomp'

    powershell = Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    subprocess.run([str(powershell), '-NoProfile', '-ExecutionPolicy', 'Bypass',
                    '-File', str(ROOT/'tools/build-launcher.ps1'), '-RequireArtwork', '-Test'], check=True)

    def copy(source, dest):
        target = stage / dest
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    copy(ROOT / 'build/windows/Release/futureperfect_recomp.exe', 'Bin/futureperfect_recomp.exe')
    copy(ROOT / 'build/windows/Release/tsfp_crash_helper.exe', 'Bin/tsfp_crash_helper.exe')
    copy(ROOT/'build/launcher/TimeSplitters Launcher.exe', 'TimeSplitters Launcher.exe')
    copy(ROOT/'build/launcher/Bin/TimeSplitters.App.exe', 'Bin/TimeSplitters.App.exe')
    copy(ROOT/'build/launcher/Support/Launcher/manifest.json', 'Support/Launcher/manifest.json')
    for dll in ['avcodec-62.dll', 'avutil-60.dll', 'swresample-6.dll']:
        copy(upstream / 'third_party/ffmpeg/bin' / dll, 'Bin/' + dll)
    installations = [Path(os.environ.get(variable, default))/'Microsoft Visual Studio'
                     for variable, default in [('ProgramFiles', r'C:\Program Files'),
                                               ('ProgramFiles(x86)', r'C:\Program Files (x86)')]]
    candidates = sorted(path for vs in installations for path in
                        vs.glob('*/*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT/vcruntime140.dll'))
    if not candidates:
        raise RuntimeError('The x64 MSVC runtime redistributable was not found.')
    for path in candidates[-1].parent.glob('*.dll'):
        copy(path, 'Bin/'+path.name)
    copy(ROOT / 'config/pc-controls.default.json', 'Settings/pc-controls.json')
    # Keep optional defaults out of personal Settings and use paths understood
    # by existing launchers. An absent Discord preference already means off.
    copy(ROOT / 'config/pc-discord.default.ini', 'Support/Discord/pc-discord.default.ini')
    copy(ROOT / 'docs/discord-presence.md', 'Support/Discord/README.txt')
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
    copy(ROOT / 'tools/launcher/GameMonogram.NOTICE.txt', 'Licenses/Game-icon-lettering.txt')
    online = bundle(ROOT, stage, copy)
    # Include the exact local source edits used for this test package, since
    # those may not yet exist at the source_commit recorded below.
    source_dir = stage/'Source'
    source_dir.mkdir()
    source_files = subprocess.check_output(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    with zipfile.ZipFile(source_dir/'pc-port-source.zip', 'w', zipfile.ZIP_DEFLATED) as source:
        write_source_files(source, ROOT, source_files)
        local_patch = subprocess.check_output(['git', '-C', str(upstream), 'diff', '--binary'])
        source.writestr('runtime-local.patch', local_patch)
    (source_dir/'README.txt').write_text(
        'pc-port-source.zip contains the project source and local runtime patch for this build.\n'
        'Enhanced textures are an optional launcher download; source archives omit them.\n'
        'Source builds may use assets/textures; explicit offline bundles include Textures.\n'
        f'Base runtime: https://github.com/nemonicicon/ts3FPrecomp/tree/'
        f'{subprocess.check_output(["git", "-C", str(upstream), "rev-parse", "HEAD"], text=True).strip()}\n'
        'See the project README and requirements-release.txt for build and packaging instructions.\n',
        encoding='utf-8')
    (stage / 'README.txt').write_text(f'''TimeSplitters: Future Perfect PC {args.version} - {release_identity(args.version)['name']}

Requires 64-bit Windows 10/11 and your own US Xbox game files.
Extract this entire ZIP to a writable folder. Open TimeSplitters Launcher.exe.
Play.cmd opens the same launcher. Game settings changes controls and graphics
before play; resolution arrows include the primary display modes and Original.
On first launch choose your Xbox ISO or an extracted folder containing
default.xbe, pak, and xmv. Setup validates the supported executable and copies
the game files to Data. Allow at least 5 GB of free space for setup.
After setup, click Play.cmd to play. The original ISO is no longer needed.
Python/Tk and Online certificate support are bundled under Support.
No Python, Git, Visual Studio, or emulator installation is required.

WASD: move. Mouse: look. Left/right mouse: fire/aim. R: reload.
E/Space/Enter: use/confirm. Escape: pause/release mouse. Click: recapture mouse.
F1: show/hide the PC Settings menu bar (also PC Settings > Hide Menu Bar).
F2: keyboard/mouse bindings and sensitivity. F12: Debug settings.
F10: switch Classic/Enhanced textures live.
Enhanced textures: {'included in Textures; partial overhaul' if texture_rows else 'optional download in launcher settings'}.
Alt+Enter: fullscreen (also available under PC Settings).
Button prompts show your first assigned keyboard/mouse binding. Controller input
restores controller icons; using the keyboard/mouse switches them back.
The first controller not assigned to another player also controls player 1.
Pause before opening F2: the editor blocks input but does not pause the game.
Settings and paused Options include Display settings for render resolution.
Confirmed display choices are saved in Settings/pc-display.ini.
Optional Discord Rich Presence: enable Game settings > Discord with the Discord
desktop app running. See Support/Discord/README.txt. Fresh installs leave this disabled.

Online multiplayer: choose Online / System Link > Online in the game.
Host: Start New Match detects your public IPv4 and starts the HTTPS directory.
Forward TCP 30820 and UDP 30821 to the host PC and allow both through Windows
Firewall. Copy the invitation from the Online window and send it to friends.
Guests paste it into Invitation / lobby code and click Join by code.
Return to the game: host creates a match, guests select it and Ready, host Go.
Guests do not need port forwarding. Hosting behind carrier-grade NAT requires
a reachable player to host instead. The game does not configure your router.
Real internet connections and full-match stability remain experimental.
Online preferences and hosting certificates are stored under Settings.

Settings: Settings/pc-controls.json. Saves: Saves. Diagnostics: Logs.
Keep Data, Settings, Saves, and Textures when upgrading. Extract upgrades to a new folder
and copy those four folders over. Do not overwrite your settings with defaults.

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
        'release_channel': release_identity(args.version),
        'auto_display_resolution': True,
        'launcher_version': json.loads((stage/'Support/Launcher/manifest.json').read_text())['version'],
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_changes': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).splitlines(),
        'online': online,
        'textures': textures,
        'upstream_commit': subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip(),
        'ffmpeg_archive_sha256': '1689da96399f372cfee985b3f2b9d74685892078059bc37764388f49e3118bdb',
        'files': {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in sorted(stage.rglob('*')) if p.is_file()},
    }
    info['files'].update({name: sha for name, _, sha in zip_entries(texture_rows)})
    (stage / 'BUILD.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
    archive_path = stage.with_name(name + '.zip')
    with zipfile.ZipFile(archive_path, 'x', zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        for p in sorted(stage.rglob('*')):
            if p.is_file():
                archive.write(p, name + '/' + p.relative_to(stage).as_posix())
        # Stream directly from the single active pack: no second staging copy.
        write_zip(archive, name+'/', texture_rows)
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
        assert not any('/Data/' in n or '/Saves/' in n or n.lower().endswith(('.iso', '.xbe', '.pak', '.img', '.pdb')) for n in archive.namelist())
    digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
    archive_path.with_suffix('.zip.sha256').write_text(f'{digest}  {archive_path.name}\n', encoding='ascii')
    # Keep exact executable/PDB outside the player package for later analysis.
    from importlib.util import spec_from_file_location, module_from_spec
    spec = spec_from_file_location('crash_symbols', Path(__file__).with_name('crash-symbols.py'))
    symbols = module_from_spec(spec); spec.loader.exec_module(symbols)
    symbols.archive(args.output.resolve() / (name + '-symbols.zip'), root=ROOT)
    print(json.dumps({'archive': str(archive_path), 'bytes': archive_path.stat().st_size, 'sha256': digest, 'stage': str(stage)}, indent=2))


if __name__ == '__main__':
    main()
