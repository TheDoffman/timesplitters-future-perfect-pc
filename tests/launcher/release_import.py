"""Exercise malformed image rejection and real release ISO import on Windows."""

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

import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = _layout_root
PS = r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--iso', type=Path, required=True)
    args = parser.parse_args()
    env = dict(os.environ, PSModulePath=r'C:\Windows\System32\WindowsPowerShell\v1.0\Modules;C:\Program Files\WindowsPowerShell\Modules')
    temp = Path(tempfile.mkdtemp(prefix='release-import-', dir=ROOT / 'build'))

    def run(path, validate=True, folder=False):
        command = [PS, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(args.stage / 'Import-game.ps1'),
                   '-Folder' if folder else '-Iso', str(path)]
        if validate:
            command.append('-ValidateOnly')
        return subprocess.run(command, env=env, capture_output=True, text=True, timeout=180)

    for name, entry, expected in [
        ('traversal', b'../escape', 'Unsafe filename'),
        ('device', b'CON', 'Reserved filename'),
        ('alternate-stream', b'file:stream', 'Unsafe filename'),
        ('wrong-version', b'default.xbe', 'supported US Xbox'),
    ]:
        image = bytearray(70 * 2048)
        image[32*2048:32*2048+20] = b'MICROSOFT*XBOX*MEDIA'
        struct.pack_into('<II', image, 32*2048+20, 40, 2048)
        struct.pack_into('<HHIIBB', image, 40*2048, 0, 0, 50, 1, 0, len(entry))
        image[40*2048+14:40*2048+14+len(entry)] = entry
        path = temp / (name + '.iso')
        path.write_bytes(image)
        result = run(path)
        assert result.returncode != 0 and expected in result.stderr, (name, result.stdout, result.stderr)
    truncated = temp / 'truncated.iso'
    truncated.write_bytes(b'not a disc')
    assert run(truncated).returncode != 0
    assert not (args.stage / 'Data').exists()
    result = run(args.iso, validate=False)
    assert result.returncode == 0, (result.stdout, result.stderr)
    source = ROOT / 'private/game'
    files = [p for p in source.rglob('*') if p.is_file() and p.suffix != '.json']
    imported = [p for p in (args.stage / 'Data').rglob('*') if p.is_file()]
    assert len(files) == len(imported) == 412
    for p in files:
        out = args.stage / 'Data' / p.relative_to(source)
        assert hashlib.file_digest(p.open('rb'), 'sha256').digest() == hashlib.file_digest(out.open('rb'), 'sha256').digest(), p
    sentinel = args.stage / 'Data' / 'keep-test.txt'
    sentinel.write_text('keep')
    assert run(args.iso, validate=False).returncode != 0
    assert sentinel.read_text() == 'keep'
    # A separate fresh destination exercises the extracted-folder alternative.
    folder_stage = temp / 'folder-package'
    folder_stage.mkdir()
    for filename in ['Import-game.ps1', 'ImportGame.cs']:
        (folder_stage / filename).write_bytes((args.stage / filename).read_bytes())
    result = subprocess.run([PS, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                             str(folder_stage / 'Import-game.ps1'), '-Folder', str(args.stage / 'Data')],
                            env=env, capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert (folder_stage / 'Data/default.xbe').read_bytes() == (source / 'default.xbe').read_bytes()
    assert not (folder_stage / 'Data/keep-test.txt').exists()
    print('PASS: malformed images rejected, 412 imported files match, existing Data preserved, extracted-folder setup works.')


if __name__ == '__main__':
    main()
