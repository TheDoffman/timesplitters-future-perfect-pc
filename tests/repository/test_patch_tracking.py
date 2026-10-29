"""Patch tracking and real branch switches use tiny source-only repositories."""

from project_paths import source_file as repo_file

from pathlib import Path
import difflib
import hashlib
import importlib.util
import json
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('tracked_patches', ROOT / 'tools/build/apply-patches.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)
BASE = 'alpha=0\nbeta=0\nend\n'


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), '-c', 'user.name=Patch Test',
                                    '-c', 'user.email=patch-test@example.invalid', *args], text=True)


def patch(folder, name, before, after, target='src/sample.c'):
    content = ''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                         fromfile='a/' + target, tofile='b/' + target))
    (folder / name).write_bytes(content.encode('utf-8'))


@pytest.fixture
def fixture(tmp_path):
    project = tmp_path / 'project'
    project.mkdir()
    git(project, 'init', '-q')
    (project / '.gitignore').write_text('/external/\n')
    patches = project / 'patches'
    patches.mkdir()
    runtime = project / 'external/runtime'
    (runtime / 'src').mkdir(parents=True)
    git(runtime, 'init', '-q')
    (runtime / 'src/sample.c').write_text(BASE)
    (runtime / 'src/other.c').write_text(BASE)
    patch(patches, 'base.patch', BASE, BASE.replace('alpha=0', 'alpha=1'))
    return project, runtime, patches


def commit(project, message):
    git(project, 'add', '--all')
    git(project, 'commit', '-qm', message)


def test_validated_patch_content_skips_only_repeated_unwind_apply(fixture, monkeypatch):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    original_git = installer.git
    def no_apply(tree, *args):
        assert args[0] not in ('apply', 'init'), 'Unchanged content was revalidated'
        return original_git(tree, *args)
    monkeypatch.setattr(installer, 'git', no_apply)
    assert installer.apply_patches(runtime, patches) == []


@pytest.mark.parametrize('changed', ['source', 'patch', 'validator', 'git', 'receipt', 'forced'])
def test_patch_receipt_invalidation_runs_full_validation(fixture, monkeypatch, changed):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    installer.apply_patches(runtime, patches)
    original_git = installer.git
    applies = []
    def count_apply(tree, *args):
        result = original_git(tree, *args)
        if args[0] == 'apply': applies.append(args)
        if args[0] == '--version' and changed == 'git': result.stdout += ' changed'
        return result
    monkeypatch.setattr(installer, 'git', count_apply)
    if changed == 'source':
        path = runtime/'src/sample.c'
        stamp = path.stat()
        path.write_text(path.read_text().replace('alpha=1', 'alpha=9'))
        import os
        os.utime(path, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
    elif changed == 'patch':
        patch(patches, 'base.patch', BASE, BASE.replace('alpha=0', 'alpha=2'))
    elif changed == 'validator':
        path = runtime/'validator.py'; path.write_bytes(Path(installer.__file__).read_bytes()+b'\n# new policy\n')
        monkeypatch.setattr(installer, '__file__', str(path))
    elif changed == 'receipt':
        installer.state_path(runtime).with_name('tsfp-patches-validated.sha256').write_bytes(b'\xff')
    if changed == 'source':
        for _ in range(2):
            with pytest.raises(RuntimeError, match='conflicts'):
                installer.apply_patches(runtime, patches)
        assert not installer.state_path(runtime).with_name('tsfp-patches-validated.sha256').exists()
    else:
        installer.apply_patches(runtime, patches, force=changed == 'forced')
    assert applies


def test_responsibility_folders_preserve_order_ledger_and_rollback(fixture):
    _, runtime, patches = fixture
    first = BASE.replace('alpha=0', 'alpha=1')
    final = first.replace('beta=0', 'beta=1')
    patch(patches, 'next.patch', first, final)
    installer.apply_patches(runtime, patches)
    ledger = installer.state_path(runtime).read_bytes()
    # Folder order deliberately opposes patch order; recurse more than once.
    for name, folder in [('base.patch', 'z/rendering'), ('next.patch', 'a/input')]:
        target = patches / folder
        target.mkdir(parents=True)
        (patches / name).rename(target / name)
    legacy = patches / 'legacy'
    legacy.mkdir()
    patch(legacy, 'unused.patch', BASE, BASE.replace('alpha=0', 'alpha=9'))
    assert installer.apply_patches(runtime, patches) == []
    assert installer.state_path(runtime).read_bytes() == ledger
    assert (runtime / 'src/sample.c').read_text() == final
    for path in installer.active_patch_paths(patches):
        path.rename(patches / path.name)
    assert installer.apply_patches(runtime, patches) == []
    assert installer.state_path(runtime).read_bytes() == ledger


def test_duplicate_basename_across_folders_fails_before_writes(fixture):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    ledger = installer.state_path(runtime).read_bytes()
    before = (runtime / 'src/sample.c').read_bytes()
    nested = patches / 'input'
    nested.mkdir()
    patch(nested, 'base.patch', BASE, BASE.replace('alpha=0', 'alpha=9'))
    with pytest.raises(ValueError, match='Duplicate active patch basename'):
        installer.apply_patches(runtime, patches)
    assert installer.state_path(runtime).read_bytes() == ledger
    assert (runtime / 'src/sample.c').read_bytes() == before


def test_record_keeps_exact_bytes_order_and_stays_out_of_source(fixture):
    project, runtime, patches = fixture
    payload = (patches / 'base.patch').read_bytes().replace(b'\n', b'\r\n')
    (patches / 'base.patch').write_bytes(payload)
    installer.apply_patches(runtime, patches)
    record = installer.state_path(runtime)
    state = json.loads(record.read_bytes())
    assert state['patches'] == [dict(name='base.patch', content=payload.decode(),
                                     sha256=hashlib.sha256(payload).hexdigest(), options=[])]
    assert 'tsfp-patches' not in git(runtime, 'status', '--porcelain')
    old_stat = record.stat().st_mtime_ns
    assert installer.apply_patches(runtime, patches) == []
    assert record.stat().st_mtime_ns == old_stat


@pytest.mark.parametrize('crlf', [False, True])
def test_real_branch_switch_removes_and_updates_unarchived_patches(fixture, crlf):
    project, runtime, patches = fixture
    first = BASE.replace('alpha=0', 'alpha=1')
    feature = first.replace('beta=0', 'beta=1')
    patch(patches, 'feature.patch', first, feature)
    commit(project, 'Feature branch')
    git(project, 'branch', '-M', 'texture')
    installer.apply_patches(runtime, patches)
    assert (runtime / 'src/sample.c').read_text() == feature
    # An unrelated edit must survive both directions and repeated builds.
    extra = '\n/* unrelated edit */\n'
    content = feature + extra
    (runtime / 'src/sample.c').write_bytes(content.replace('\n', '\r\n' if crlf else '\n').encode())
    git(project, 'switch', '-qc', 'main')
    (patches / 'feature.patch').unlink()
    second = BASE.replace('alpha=0', 'alpha=2')
    patch(patches, 'base.patch', BASE, second)
    commit(project, 'Main changes patch contents and removes feature')
    installer.apply_patches(runtime, patches)
    assert (runtime / 'src/sample.c').read_text() == second + extra
    record = json.loads(installer.state_path(runtime).read_bytes())
    assert [entry['name'] for entry in record['patches']] == ['base.patch']
    assert installer.apply_patches(runtime, patches) == []
    git(project, 'switch', '-q', 'texture')
    installer.apply_patches(runtime, patches)
    assert (runtime / 'src/sample.c').read_text() == feature + extra
    assert installer.apply_patches(runtime, patches) == []
    git(project, 'switch', '-q', 'main')
    installer.apply_patches(runtime, patches)
    assert (runtime / 'src/sample.c').read_text() == second + extra


def test_renamed_patch_and_empty_stack(fixture):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    (patches / 'base.patch').rename(patches / 'renamed.patch')
    assert installer.apply_patches(runtime, patches) == []
    state = json.loads(installer.state_path(runtime).read_bytes())
    assert [entry['name'] for entry in state['patches']] == ['renamed.patch']
    (patches / 'renamed.patch').unlink()
    installer.apply_patches(runtime, patches)
    assert (runtime / 'src/sample.c').read_text() == BASE
    assert json.loads(installer.state_path(runtime).read_bytes())['patches'] == []
    assert installer.apply_patches(runtime, patches) == []


def test_failed_legacy_unwind_leaves_all_staged_files_unchanged(fixture):
    _, runtime, patches = fixture
    (patches / 'base.patch').unlink()
    legacy = patches / 'legacy'
    legacy.mkdir()
    first = BASE.replace('alpha=0', 'alpha=1')
    patch(legacy, 'old.patch', BASE, first)
    other = ''.join(difflib.unified_diff(BASE.splitlines(True), first.splitlines(True),
                                       fromfile='a/src/other.c', tofile='b/src/other.c'))
    target = legacy / 'old.patch'
    target.write_text(target.read_text() + other)
    # The first file can unwind, but the second cannot. Skipping this legacy
    # patch must not leave the successful hunk applied in the validation tree.
    (runtime / 'src/sample.c').write_text(first)
    before = {name: (repo_file(runtime, 'src', name)).read_bytes() for name in ('sample.c', 'other.c')}
    assert installer.apply_patches(runtime, patches) == []
    assert {name: (repo_file(runtime, 'src', name)).read_bytes() for name in before} == before


def test_modified_old_patch_preserves_sources_and_record(fixture):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    source = runtime / 'src/sample.c'
    source.write_text(source.read_text().replace('alpha=1', 'alpha=99'))
    (patches / 'base.patch').unlink()
    record = installer.state_path(runtime)
    before = source.read_bytes(), record.read_bytes()
    with pytest.raises(RuntimeError, match='base.patch conflicts.*unwind'):
        installer.apply_patches(runtime, patches)
    assert (source.read_bytes(), record.read_bytes()) == before


@pytest.mark.parametrize('corrupt', ['json', 'hash', 'version', 'options'])
def test_invalid_record_fails_without_source_writes(fixture, corrupt):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    record = installer.state_path(runtime)
    state = json.loads(record.read_bytes())
    if corrupt == 'hash':
        state['patches'][0]['content'] += '\n'
    elif corrupt == 'version':
        state['version'] = 999
    elif corrupt == 'options':
        state['patches'][0]['options'] = ['--unsafe-paths']
    record.write_text('{' if corrupt == 'json' else json.dumps(state))
    before = (runtime / 'src/sample.c').read_bytes(), record.read_bytes()
    with pytest.raises(RuntimeError, match='Invalid runtime patch record'):
        installer.apply_patches(runtime, patches)
    assert ((runtime / 'src/sample.c').read_bytes(), record.read_bytes()) == before


@pytest.mark.parametrize('failure', ['source', 'record'])
def test_failed_install_restores_sources_and_previous_record(fixture, monkeypatch, failure):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    patch(patches, 'base.patch', BASE, BASE.replace('alpha=0', 'alpha=2'))
    patch(patches, 'other.patch', BASE, BASE.replace('beta=0', 'beta=2'), 'src/other.c')
    before = {name: (runtime / name).read_bytes() for name in ('src/sample.c', 'src/other.c')}
    record = installer.state_path(runtime)
    record_before = record.read_bytes()
    copy = installer.shutil.copyfile
    replace = installer.os.replace

    def fail_copy(source, target):
        if Path(target) == runtime / 'src/sample.c':
            Path(target).write_bytes(b'partial write')
            raise OSError('simulated source write failure')
        return copy(source, target)

    def fail_replace(source, target):
        if Path(target) == record:
            raise OSError('simulated record publication failure')
        return replace(source, target)

    monkeypatch.setattr(installer.shutil, 'copyfile', fail_copy if failure == 'source' else copy)
    monkeypatch.setattr(installer.os, 'replace', fail_replace if failure == 'record' else replace)
    with pytest.raises(RuntimeError, match='restored runtime sources'):
        installer.apply_patches(runtime, patches)
    assert {name: (runtime / name).read_bytes() for name in before} == before
    assert record.read_bytes() == record_before
    assert not list(record.parent.glob('tsfp-patches-*.tmp'))


@pytest.mark.parametrize('concurrent_edit', ['source', 'record'])
def test_edit_during_validation_is_preserved(fixture, monkeypatch, concurrent_edit):
    _, runtime, patches = fixture
    installer.apply_patches(runtime, patches)
    patch(patches, 'base.patch', BASE, BASE.replace('alpha=0', 'alpha=2'))
    record = installer.state_path(runtime)
    source = runtime / 'src/sample.c'
    edited = source if concurrent_edit == 'source' else record
    original_git = installer.git
    expected = edited.read_bytes() + b'\n'

    def edit_after_staged_apply(tree, *args):
        result = original_git(tree, *args)
        if args[0] == 'apply' and '--check' not in args and '--reverse' not in args:
            edited.write_bytes(expected)
        return result

    monkeypatch.setattr(installer, 'git', edit_after_staged_apply)
    with pytest.raises(RuntimeError, match='changed during .*validation'):
        installer.apply_patches(runtime, patches)
    assert edited.read_bytes() == expected
    assert 'alpha=2' not in source.read_text()
