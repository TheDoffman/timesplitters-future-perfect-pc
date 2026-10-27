"""Make a local, extract-and-play ZIP from the built runtime and local game files."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
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

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = _layout_root
NAME = 'TimeSplitters Future Perfect'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'build/releases/TimeSplitters-Future-Perfect-PC-v0.1.0-alpha.1-windows-x64')
    parser.add_argument('--game', type=Path, default=ROOT / 'private/game')
    parser.add_argument('--output', type=Path, default=ROOT / 'TimeSplitters Future Perfect - Extract and Play.zip')
    args = parser.parse_args()
    expected = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    assert hashlib.sha256((args.game / 'default.xbe').read_bytes()).hexdigest() == expected
    build = json.loads((args.runtime / 'BUILD.json').read_text())
    sources = {}
    for relative, digest in build['files'].items():
        if relative.startswith(('Bin/', 'Settings/', 'Licenses/')) or relative == 'Launch.ps1':
            source = args.runtime / relative
            assert hashlib.sha256(source.read_bytes()).hexdigest() == digest, relative
            sources[relative] = source
    sources['Data/default.xbe'] = args.game / 'default.xbe'
    for directory in ('pak', 'xmv'):
        files = [p for p in (args.game / directory).rglob('*') if p.is_file()]
        assert files, f'Missing {directory} game files'
        for p in files:
            sources['Data/' + p.relative_to(args.game).as_posix()] = p
    generated = {
        'Play.cmd': ('@echo off\r\n"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" '
                     '-NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch.ps1"\r\n'
                     'if errorlevel 1 pause\r\n').encode('ascii'),
        'README.txt': '''TimeSplitters: Future Perfect - Extract and Play

Extract the entire ZIP to a writable folder, then double-click Play.cmd.
Requires Windows 10/11 x64 and approximately 4.2 GB for the extracted package.
All required game files and runtime libraries are included. There is no import
step. No ISO, Python, Visual Studio, or emulator installation is required.

WASD: move. Mouse: look. Left/right mouse: fire/aim. R: reload.
E/Space/Enter: use/confirm. Escape: pause/release mouse. F1: capture/release.
F2: keyboard and mouse settings. Alt+Enter: fullscreen.
Pause the game before editing controls; the F2 dialog does not pause gameplay.

This package starts with default PC controls and creates new saves on first run.
Settings: Settings/pc-controls.json. Saves: Saves. Diagnostic output: Logs.
Keep Settings and Saves when moving or updating this package.

Experimental native PC build: startup, controls, and short first-mission gameplay
have been checked. A complete campaign playthrough has not been verified.
Settings and paused Options include Display settings for render resolution.
Confirmed choices are saved in Settings/pc-display.ini. Notices are in Licenses.
'''.encode('utf-8'),
    }
    manifest = {}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'x', zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        for index, (relative, source) in enumerate(sorted(sources.items()), 1):
            digest = hashlib.sha256()
            with source.open('rb') as src, archive.open(NAME + '/' + relative, 'w', force_zip64=True) as dst:
                for block in iter(lambda: src.read(1024 * 1024), b''):
                    dst.write(block)
                    digest.update(block)
            manifest[relative] = digest.hexdigest()
            if index % 50 == 0:
                print(f'Packed {index}/{len(sources)} files', flush=True)
        for relative, content in generated.items():
            archive.writestr(NAME + '/' + relative, content)
            manifest[relative] = hashlib.sha256(content).hexdigest()
        build.update(package='local-standalone', files=manifest)
        archive.writestr(NAME + '/BUILD.json', json.dumps(build, indent=2) + '\n')
    with args.output.open('rb') as src:
        digest = hashlib.file_digest(src, 'sha256').hexdigest()
    args.output.with_suffix('.zip.sha256').write_text(f'{digest}  {args.output.name}\n', encoding='ascii')
    print(json.dumps({'archive': str(args.output), 'bytes': args.output.stat().st_size,
                      'files': len(manifest) + 1, 'sha256': digest}, indent=2), flush=True)


if __name__ == '__main__':
    main()
