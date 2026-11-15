"""Relocatable server runtime source/loader boundary tests; no system installs."""
import importlib.util
import os
from pathlib import Path
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('host_runtime_images', ROOT / 'tools/mods/hosting/runtime-images-mac.py')
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


def test_load_commands_exclude_identity_and_preserve_space_in_path():
    dependencies, rpaths = runtime.commands('''
cmd LC_ID_DYLIB
name /old/self.dylib (offset 24)
cmd LC_LOAD_DYLIB
name @rpath/lib one.dylib (offset 24)
cmd LC_RPATH
path @loader_path/../lib (offset 12)
cmd LC_LOAD_WEAK_DYLIB
name /usr/lib/libSystem.B.dylib (offset 24)
''')
    assert dependencies == ['@rpath/lib one.dylib', '/usr/lib/libSystem.B.dylib']
    assert rpaths == ['@loader_path/../lib']


@pytest.mark.parametrize('text', ['', 'not otool output', 'cmd LC_DYLD_ENVIRONMENT'])
def test_missing_commands_and_loader_environment_refused(text):
    with pytest.raises(ValueError):
        runtime.commands(text)


@pytest.mark.parametrize('name', ['/usr/lib/libSystem.B.dylib', '/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation'])
def test_only_apple_system_dependencies_are_external(name):
    assert runtime.resolve_dependency(name, Path('/ignored'), []) is None


@pytest.mark.skipif(os.name != 'posix', reason='POSIX loader paths and source ownership')
def test_relative_loader_dependency_resolves_only_inside_cellar(tmp_path, monkeypatch):
    monkeypatch.setattr(runtime, 'CELLAR', tmp_path)
    root = tmp_path / 'formula/1.0'
    (root / 'bin').mkdir(parents=True)
    (root / 'lib').mkdir()
    library = root / 'lib/libsample.dylib'
    library.write_bytes(b'library')
    library.chmod(0o644)
    image = root / 'bin/sample'
    assert runtime.resolve_dependency('@rpath/libsample.dylib', image, ['@loader_path/../lib']) == library
    with pytest.raises(ValueError, match='outside Homebrew'):
        runtime.source(tmp_path.parent)
    with pytest.raises(ValueError, match='Unsupported'):
        runtime.resolve_dependency('@executable_path/unknown', image, [])


@pytest.mark.skipif(os.name != 'posix', reason='POSIX loader paths and source ownership')
def test_ambiguous_rpath_is_never_guessed(tmp_path, monkeypatch):
    monkeypatch.setattr(runtime, 'CELLAR', tmp_path)
    paths = [tmp_path / 'one/1/lib', tmp_path / 'two/1/lib']
    for p in paths:
        p.mkdir(parents=True)
        (p / 'library').write_bytes(b'lib')
        (p / 'library').chmod(0o644)
    with pytest.raises(ValueError, match='ambiguous'):
        runtime.resolve_dependency('@rpath/library', paths[0] / 'image', [str(p) for p in paths])


@pytest.mark.skipif(os.name != 'posix', reason='POSIX nofollow copy and source ownership')
def test_builder_caps_copy_and_does_not_replace_existing_file(tmp_path, monkeypatch):
    source = tmp_path / 'cellar/formula/1/input'
    source.parent.mkdir(parents=True)
    source.write_bytes(b'input')
    source.chmod(0o644)
    monkeypatch.setattr(runtime, 'CELLAR', tmp_path / 'cellar')
    output = tmp_path / 'output'
    output.mkdir()
    builder = runtime.Builder(output)
    builder.copy(source, Path('safe/input'))
    assert (output / 'safe/input').read_bytes() == b'input'
    with pytest.raises(ValueError, match='Duplicate'):
        builder.copy(source, Path('safe/input'))
    with pytest.raises(ValueError, match='Escaped'):
        builder.copy(source, Path('../escape'))
    monkeypatch.setattr(runtime, 'MAX_BYTES', builder.total)
    with pytest.raises(ValueError, match='budget'):
        builder.copy(source, Path('other'))
    assert not (output / 'other').exists()


def test_staging_never_runs_as_root_or_non_mac(monkeypatch):
    monkeypatch.setattr(runtime.sys, 'platform', 'darwin')
    monkeypatch.setattr(runtime.os, 'getuid', lambda: 0, raising=False)
    with pytest.raises(ValueError, match='without sudo'):
        runtime.build(SimpleNamespace(output='/not-created'))
