"""Offline source reconstruction, exact Git identity and existing-work safety."""
import base64
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('runtime_snapshot', ROOT / 'tools/prepare-runtime.py')
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


@pytest.fixture
def snapshot(tmp_path):
    source = tmp_path / 'snapshot/source'
    source.mkdir(parents=True)
    entries = []
    for name, mode, data in [('src/test.c', '100644', b'int x;\r\n'),
                             ('scripts/test.sh', '100755', b'#!/bin/sh\necho test\n')]:
        path = source / name
        path.parent.mkdir(parents=True)
        path.write_bytes(data)
        entries.append(dict(path=name, mode=mode, oid=runtime.object_id('blob', data),
                            sha256=hashlib.sha256(data).hexdigest()))
    entries.append(dict(path='third_party/example', mode='160000', oid='1' * 40))
    tree = runtime.tree_id(entries)
    commit = (f'tree {tree}\nparent {"2" * 40}\n'
              'author Test <test@example.invalid> 1 +0000\n'
              'committer Test <test@example.invalid> 1 +0000\n\nSnapshot test\n').encode()
    record = dict(version=1, revision=runtime.object_id('commit', commit), tree=tree,
                  commit_base64=base64.b64encode(commit).decode(), entries=entries)
    (source.parent / 'snapshot.json').write_text(json.dumps(record))
    return source.parent, record


def test_offline_roundtrip_and_reuse_preserve_all_local_work(tmp_path, snapshot, monkeypatch):
    source, record = snapshot
    monkeypatch.setenv('GIT_ALLOW_PROTOCOL', '')  # All Git network/file transports disabled.
    target = tmp_path / 'runtime'
    assert runtime.prepare(target, source, record['revision'])
    assert runtime.git(target, 'rev-parse', 'HEAD').decode().strip() == record['revision']
    assert runtime.git(target, 'status', '--porcelain') == b''
    assert runtime.git(target, 'remote') == b''
    runtime.git(target, 'fsck', '--full')
    assert (target / 'third_party/example').is_dir()
    if os.name != 'nt':
        assert (target / 'scripts/test.sh').stat().st_mode & 0o111
    (target / 'src/test.c').write_bytes(b'local edit\r\n')
    (target / '.git/tsfp-patches.json').write_bytes(b'patch ledger')
    (target / 'generated.c').write_bytes(b'private generated code')
    assert not runtime.prepare(target, source, record['revision'])
    assert (target / 'src/test.c').read_bytes() == b'local edit\r\n'
    assert (target / '.git/tsfp-patches.json').read_bytes() == b'patch ledger'
    assert (target / 'generated.c').read_bytes() == b'private generated code'
    assert not list(tmp_path.glob('.tsfp-runtime-*'))


@pytest.mark.parametrize('mutation', ['corrupt', 'missing', 'extra', 'traversal', 'mode', 'tree', 'commit'])
def test_invalid_snapshot_fails_before_destination_creation(tmp_path, snapshot, mutation):
    source, record = snapshot
    if mutation == 'corrupt':
        (source / 'source/src/test.c').write_bytes(b'changed')
    elif mutation == 'missing':
        (source / 'source/src/test.c').unlink()
    elif mutation == 'extra':
        (source / 'source/secret.txt').write_text('unexpected')
    elif mutation == 'traversal':
        record['entries'][0]['path'] = '../outside'
    elif mutation == 'mode':
        record['entries'][0]['mode'] = '120000'
    elif mutation == 'tree':
        record['entries'][-1]['oid'] = '3' * 40
    else:
        record['revision'] = '4' * 40
    (source / 'snapshot.json').write_text(json.dumps(record))
    with pytest.raises((ValueError, OSError)):
        runtime.prepare(tmp_path / 'runtime', source)
    assert not (tmp_path / 'runtime').exists()
    assert not list(tmp_path.glob('.tsfp-runtime-*'))


def test_mismatched_pin_or_non_checkout_is_never_overwritten(tmp_path, snapshot):
    source, record = snapshot
    target = tmp_path / 'runtime'
    with pytest.raises(ValueError, match='shared pin'):
        runtime.prepare(target, source, '0' * 40)
    target.mkdir()
    (target / 'keep.txt').write_text('keep')
    with pytest.raises(ValueError, match='not a Git checkout'):
        runtime.prepare(target, source)
    assert (target / 'keep.txt').read_text() == 'keep'


def test_failed_git_import_is_atomic(tmp_path, snapshot, monkeypatch):
    source, _ = snapshot
    original = runtime.git
    def fail(root, *args, **kwargs):
        if args[0] == 'write-tree':
            raise subprocess.CalledProcessError(1, 'git write-tree')
        return original(root, *args, **kwargs)
    monkeypatch.setattr(runtime, 'git', fail)
    with pytest.raises(subprocess.CalledProcessError):
        runtime.prepare(tmp_path / 'runtime', source)
    assert not (tmp_path / 'runtime').exists()
    assert not list(tmp_path.glob('.tsfp-runtime-*'))


def test_existing_wrong_revision_is_preserved(tmp_path, snapshot):
    source, record = snapshot
    target = tmp_path / 'runtime'
    runtime.prepare(target, source)
    original = base64.b64decode(record['commit_base64'])
    other = runtime.write_object(target, 'commit', original + b'Other revision\n')
    runtime.git(target, 'update-ref', '--no-deref', 'HEAD', other)
    with pytest.raises(ValueError, match='shared pin'):
        runtime.prepare(target, source)
    assert runtime.git(target, 'rev-parse', 'HEAD').decode().strip() == other


def test_real_bundled_revision_has_usable_pristine_history_and_source_archive(tmp_path, monkeypatch):
    monkeypatch.setenv('GIT_ALLOW_PROTOCOL', '')
    record, commit = runtime.read_snapshot(runtime.SNAPSHOT)
    target = tmp_path / 'runtime'
    runtime.prepare(target)
    assert runtime.git(target, 'cat-file', 'commit', 'HEAD') == commit
    assert runtime.git(target, 'status', '--porcelain') == b''
    runtime.git(target, 'fsck', '--full')
    # Historical patch tests use this exact Git archive interface.
    data = runtime.git(target, 'archive', 'HEAD', '--', 'src/kernel/kernel_bridge.c')
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        assert archive.extractfile('src/kernel/kernel_bridge.c').read() == (
            runtime.SNAPSHOT / 'source/src/kernel/kernel_bridge.c').read_bytes()
    assert record['revision'] in (ROOT / 'tsfp.ps1').read_text()
    assert runtime.git(target, 'remote') == b''
    assert (target / 'LICENSE').is_file() and (target / 'NOTICE').is_file()
    runtime.git(target, 'submodule', 'init', 'third_party/SDL', 'third_party/XbSymbolDatabase')
    assert b'https://github.com/libsdl-org/SDL.git' in runtime.git(
        target, 'config', 'submodule.third_party/SDL.url')
    patch_spec = importlib.util.spec_from_file_location('bundled_patch_loader', ROOT / 'tools/apply-patches.py')
    patches = importlib.util.module_from_spec(patch_spec)
    patch_spec.loader.exec_module(patches)
    assert patches.apply_patches(target, ROOT / 'patches')
    assert patches.apply_patches(target, ROOT / 'patches') == []
    ledger = json.loads((target / '.git/tsfp-patches.json').read_text())
    assert len(ledger['patches']) == len(patches.active_patch_paths(ROOT / 'patches'))


def test_build_and_ci_entry_points_use_bundled_source():
    windows = (ROOT / 'tsfp.ps1').read_text()
    mac = (ROOT / 'tools/build-native-mac.py').read_text()
    ci = (ROOT / '.github/workflows/runtime-patches.yml').read_text()
    for text in (windows, mac, ci):
        assert 'prepare-runtime.py' in text
        assert 'https://github.com/nemonicicon/ts3FPrecomp.git' not in text
        assert 'repository: nemonicicon/ts3FPrecomp' not in text
    assert 'tests/test_runtime_snapshot.py' in ci


def test_release_source_writer_includes_snapshot_and_preserves_bytes(tmp_path):
    import sys
    sys.path.insert(0, str(ROOT / 'tools'))
    from build_textures import write_source_files
    rows = ['vendor/ts3FPrecomp/snapshot.json',
            'vendor/ts3FPrecomp/source/src/kernel/kernel_bridge.c',
            'vendor/ts3FPrecomp/source/LICENSE', 'tools/prepare-runtime.py']
    path = tmp_path / 'source.zip'
    with zipfile.ZipFile(path, 'w') as archive:
        write_source_files(archive, ROOT, rows)
    with zipfile.ZipFile(path) as archive:
        for row in rows:
            assert archive.read(row) == (ROOT / row).read_bytes()
