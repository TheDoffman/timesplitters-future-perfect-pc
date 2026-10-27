"""Archive symbols from the requested configuration, never a stale other build."""


import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('crash_symbols', ROOT/'tools/release/crash-symbols.py')
symbols = importlib.util.module_from_spec(spec)
spec.loader.exec_module(symbols)


def test_selected_configuration_is_archived(tmp_path, monkeypatch):
    release = tmp_path/'build/windows/Release'
    debug = tmp_path/'build/windows/Debug'
    for folder in (release, debug):
        folder.mkdir(parents=True)
        for name in ('futureperfect_recomp.exe', 'futureperfect_recomp.pdb'):
            (folder/name).write_bytes((folder.name + name).encode())
    monkeypatch.setattr(symbols.subprocess, 'check_output',
                        lambda command, **kwargs: 'source-revision\n' if 'rev-parse' in command else '')
    output = tmp_path/'symbols.zip'
    symbols.archive(output, root=tmp_path, build=debug)
    with zipfile.ZipFile(output) as archive:
        metadata = json.loads(archive.read('BUILD.json'))
        for name in ('futureperfect_recomp.exe', 'futureperfect_recomp.pdb'):
            assert archive.read(name) == (debug/name).read_bytes()
            assert metadata['files'][name] == hashlib.sha256((debug/name).read_bytes()).hexdigest()
    symbols.archive(output, root=tmp_path)
    with zipfile.ZipFile(output) as archive:
        assert archive.read('futureperfect_recomp.exe') == (release/'futureperfect_recomp.exe').read_bytes()
