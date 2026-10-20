"""Release orchestration checks use tiny fixtures and a simulated GitHub CLI.

No real tag, release, game data or personal saves are touched.
"""
import importlib.util
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

import pytest

spec = importlib.util.spec_from_file_location('publish_release', Path(__file__).resolve().parents[1]/'tools/publish-release.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
HEAD, PREVIOUS = 'a'*40, 'b'*40


def link_fixture(link, target):
    try:
        link.symlink_to(target, target_is_directory=True)
    except OSError as error:
        pytest.skip(f'Directory symlink prerequisite unavailable: {error}')


def test_pytest_alias_cleanup_keeps_target_files(tmp_path):
    tests=tmp_path/'build/release-tests'; target=tests/'audio-buffer0'
    target.mkdir(parents=True);(target/'fixture.txt').write_text('keep')
    link=tests/'audio-buffercurrent'; link_fixture(link,target)
    module.clean_pytest_current_links(tmp_path,tests)
    assert not link.is_symlink() and (target/'fixture.txt').read_text()=='keep'
    module.check_directory(tmp_path,tests)


def test_successful_pytest_link_fixtures_are_discarded_before_release_cleanup(tmp_path):
    tests=tmp_path/'build/release-tests'
    tests.parent.mkdir()
    fixture=tmp_path/'fixture_test.py'
    fixture.write_text('''import pytest
@pytest.fixture(scope="session")
def compiled(tmp_path_factory):
    cache=tmp_path_factory.mktemp("compiler")
    (cache/"fixture.txt").write_text("keep")
    return cache
def test_fixture(tmp_path,compiled):
    target=tmp_path/"target";target.mkdir()
    try: (tmp_path/"nested-link").symlink_to(target,target_is_directory=True)
    except OSError: pytest.skip("symlinks unavailable")
''')
    result=subprocess.run([sys.executable,'-m','pytest',str(fixture),'-q',
                           '-o','tmp_path_retention_policy=failed','--basetemp',str(tests)],
                          cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    module.clean_pytest_current_links(tmp_path,tests)
    module.check_directory(tmp_path,tests)
    assert (tests/'compiler0/fixture.txt').read_text()=='keep'


@pytest.mark.parametrize('kind',['external','unexpected','nested'])
def test_pytest_alias_cleanup_rejects_unrecognized_links_without_mutation(tmp_path,kind):
    tests=tmp_path/'build/release-tests'; target=tests/'fixture0'
    target.mkdir(parents=True)
    good=tests/'fixturecurrent'; link_fixture(good,target)
    if kind=='external':
        other=tmp_path/'outside0';other.mkdir();bad=tests/'outsidecurrent'
    elif kind=='nested':
        other=target;bad=target/'fixturecurrent'
    else:
        other=target;bad=tests/'unexpected'
    link_fixture(bad,other)
    with pytest.raises(ValueError,match='Unexpected link'):
        module.clean_pytest_current_links(tmp_path,tests)
    assert good.is_symlink() and bad.is_symlink() and target.is_dir()


class FakePublisher(module.Publisher):
    def __init__(self, root):
        super().__init__(root)
        (root/'VERSION').write_text('0.2.0-alpha.3')
        self.calls = []
        self.branch, self.dirty, self.remote = 'main', False, HEAD
        self.releases = [dict(tag_name='v0.2.0-alpha.2', draft=False, prerelease=True, published_at='2026-10-15T01:00:00Z')]
        self.bad_upload = False
        self.tag_created = False
        self.builds = 0

    def git(self, *args):
        self.calls.append(('git', args))
        if args[0] == 'status':
            return ' M source.c' if self.dirty else ''
        if args[:2] == ('branch', '--show-current'):
            return self.branch
        if args[:2] == ('remote', 'get-url'):
            return 'https://github.com/owner/repo.git'
        if args[0] == 'fetch':
            return ''
        if args[0] == 'rev-parse':
            return PREVIOUS if 'refs/tags/' in args[-1] else HEAD
        if args[0] == 'ls-remote':
            tag = HEAD+'\trefs/tags/'+self.tag if self.tag_created else ''
            return tag if len(args) == 3 else self.remote+'\trefs/heads/main'+ ('\n'+tag if tag else '')
        if args[0] == 'log':
            return HEAD+'\tAdd Online hosting and <diagnostics>'
        if args[0] == 'rev-list':
            return HEAD
        raise AssertionError(args)

    def gh(self, *args, payload=None):
        self.calls.append(('gh', args, payload))
        if args[:2] == ('auth', 'status'):
            return ''
        if args[:2] == ('api', '--paginate') and '/pulls/' in args[-1]:
            return json.dumps([[dict(filename='src/pc_network.c')]])
        if args[:2] == ('api', '--paginate'):
            return json.dumps([self.releases])
        if 'generate-notes' in str(args):
            assert payload['previous_tag_name'] == 'v0.2.0-alpha.2'
            assert payload['target_commitish'] == HEAD
            return json.dumps(dict(body='## What\u2019s Changed\n\n* Online multiplayer https://github.com/owner/repo/pull/12'))
        if args[:2] == ('api', 'repos/owner/repo/pulls/12'):
            return json.dumps(dict(merged_at='2026-10-17T00:00:00Z', merge_commit_sha=HEAD,
                                   title='Online multiplayer', changed_files=1))
        if 'git/refs' in str(args):
            assert payload == dict(ref='refs/tags/'+self.tag, sha=HEAD)
            self.tag_created = True
            return json.dumps(dict(ref=payload['ref'], object=dict(sha=HEAD)))
        if args[:2] in (('release', 'create'), ('release', 'edit')):
            return ''
        if args[:2] == ('release', 'view'):
            files = (self.archive, self.checksum, self.metadata)
            return json.dumps(dict(isDraft=True, targetCommitish=HEAD, url='https://github.com/owner/repo/releases/tag/'+self.tag,
                assets=[dict(name=p.name, size=p.stat().st_size+(1 if self.bad_upload else 0)) for p in files]))
        raise AssertionError(args)

    def recycle(self, path):
        # Only this test's own fixture stage, never a real release tree.
        assert path == self.stage and path.parent == self.output
        shutil.rmtree(path)

    def build_package(self):
        self.builds += 1
        self.stage.mkdir()
        binary = self.stage/'Bin/game.exe'
        binary.parent.mkdir()
        binary.write_bytes(b'fixture executable, no original game data')
        build = dict(source_commit=HEAD, version=self.tag, source_changes=[], files={'Bin/game.exe':module.digest(binary)})
        (self.stage/'BUILD.json').write_text(json.dumps(build))
        with zipfile.ZipFile(self.archive, 'x') as archive:
            for path in (binary, self.stage/'BUILD.json'):
                archive.write(path, self.name+'/'+path.relative_to(self.stage).as_posix())
        self.checksum.write_text(module.digest(self.archive)+'  '+self.archive.name+'\n')
        self.verify_package()
        shutil.copyfile(self.stage/'BUILD.json', self.metadata)


@pytest.fixture
def publisher(tmp_path, monkeypatch):
    monkeypatch.setattr(module.shutil, 'which', lambda name: name)
    return FakePublisher(tmp_path)


def publication_calls(publisher):
    return [call for call in publisher.calls if call[0]=='gh' and call[1][:2] in (('release','create'), ('release','edit'))]


def test_prepare_then_publish_reuses_exact_package(publisher):
    publisher.preflight()
    publisher.prepare()
    assert not publication_calls(publisher)
    assert not publisher.stage.exists()
    notes = publisher.notes.read_text(encoding='utf-8')
    assert 'Changes since v0.2.0-alpha.2' in notes and '- Online multiplayer ([#12]' in notes
    assert 'What\u2019s Changed' not in notes and "What's Changed" not in notes
    assert 'Source changes' not in notes and 'Build and validation' not in notes
    assert not any(call[0] == 'git' and call[1][0] == 'log' for call in publisher.calls)
    publisher.prepare()
    assert publisher.builds == 1
    publisher.publish()
    create, edit = publication_calls(publisher)
    assert '--draft' in create[1] and '--prerelease' in create[1] and '--latest=false' in create[1]
    assert create[1][create[1].index('--target')+1] == HEAD
    assert '--notes-file' in create[1] and '--draft=false' in edit[1]
    assert '--verify-tag' in create[1]


@pytest.mark.parametrize('problem', ['branch', 'dirty', 'remote', 'published', 'version'])
def test_preflight_blocks_wrong_or_already_published_source(publisher, problem):
    if problem == 'branch': publisher.branch = 'feature'
    if problem == 'dirty': publisher.dirty = True
    if problem == 'remote': publisher.remote = PREVIOUS
    if problem == 'published': publisher.releases[0]['tag_name'] = 'v0.2.0-alpha.3'
    if problem == 'version': (publisher.root/'VERSION').write_text('0.2.0-alpha.1')
    with pytest.raises(ValueError):
        publisher.preflight()
    assert not publication_calls(publisher) and publisher.builds == 0


def test_changed_prepared_asset_is_not_uploaded(publisher):
    publisher.preflight(); publisher.prepare()
    publisher.archive.write_bytes(b'changed after tests')
    with pytest.raises(ValueError, match='assets changed'):
        publisher.prepare()
    assert not publication_calls(publisher)


def test_upload_failure_leaves_draft_unpublished(publisher):
    publisher.preflight(); publisher.prepare()
    publisher.bad_upload = True
    with pytest.raises(ValueError, match='not published'):
        publisher.publish()
    assert len(publication_calls(publisher)) == 1


def test_draft_option_never_publishes(publisher):
    publisher.preflight(); publisher.prepare(); publisher.publish(draft=True)
    assert len(publication_calls(publisher)) == 1


@pytest.mark.parametrize('version', ['0.2.0-alpha.3', '0.2.0'])
@pytest.mark.parametrize('draft', [False, True])
def test_game_releases_preserve_latest_launcher(publisher, version, draft):
    (publisher.root/'VERSION').write_text(version)
    publisher.preflight(); publisher.prepare(); publisher.publish(draft=draft)
    calls = publication_calls(publisher)
    assert len(calls) == (1 if draft else 2)
    # Both draft creation and final publication must keep the launcher Latest.
    assert all('--latest=false' in call[1] for call in calls)
    assert ('--prerelease' in calls[0][1]) == ('-' in version)
    creation = calls[0][1]
    assert creation[creation.index('--title') + 1] == 'TSFP v' + version


def test_package_manifest_rejects_changed_stage(publisher):
    publisher.preflight(); publisher.output.mkdir(parents=True); publisher.build_package()
    (publisher.stage/'Bin/game.exe').write_bytes(b'changed after packaging')
    with pytest.raises(ValueError, match='Staged file changed'):
        publisher.verify_package()


@pytest.mark.parametrize('corrupt', [False, True])
def test_streamed_textures_validate_from_zip_without_stage_copy(publisher, corrupt):
    publisher.preflight(); publisher.output.mkdir(parents=True); publisher.build_package()
    build = json.loads((publisher.stage/'BUILD.json').read_text())
    name = 'Textures/0123456789abcdef.dds'
    content = b'synthetic texture payload'
    build['textures'] = {'source': 'local', 'texture_count': 1}
    build['files'][name] = hashlib.sha256(content).hexdigest()
    (publisher.stage/'BUILD.json').write_text(json.dumps(build))
    with zipfile.ZipFile(publisher.archive, 'w') as archive:
        for relative in build['files']:
            if relative != name:
                archive.write(publisher.stage/relative, publisher.name+'/'+relative)
        archive.write(publisher.stage/'BUILD.json', publisher.name+'/BUILD.json')
        archive.writestr(publisher.name+'/'+name, b'corrupt' if corrupt else content)
    publisher.checksum.write_text(module.digest(publisher.archive)+'  '+publisher.archive.name+'\n')
    assert not (publisher.stage/'Textures').exists()
    if corrupt:
        with pytest.raises(ValueError, match='Archived file changed'):
            publisher.verify_package()
    else:
        publisher.verify_package()


def test_published_baseline_includes_prereleases_and_ignores_drafts():
    stable = dict(tag_name='v0.1.0', draft=False, published_at='2026-01-01T00:00:00Z')
    preview = dict(tag_name='v0.2.0-alpha.2', draft=False, published_at='2026-10-01T00:00:00Z')
    draft = dict(tag_name='v9.0.0', draft=True, published_at=None)
    assert module.last_published([[stable], [preview, draft]])[1] == preview


@pytest.mark.parametrize('tag', ['launcher-v1.2.7', 'other-release', 'vbad', 'v0.2.0-alpha.01', 'vv1.0.0'])
def test_game_baseline_ignores_unrelated_or_invalid_tags(tag):
    game = dict(tag_name='v0.2.0-alpha.2', draft=False, published_at='2026-10-01T00:00:00Z')
    other = dict(tag_name=tag, draft=False, published_at='2026-10-16T00:00:00Z')
    releases, previous = module.last_published([[game], [other]])
    assert releases == [game, other]  # Keep every tag for collision checks.
    assert previous == game
    assert module.last_published([[other]])[1] is None


def test_newer_launcher_does_not_change_game_notes_baseline(publisher):
    publisher.releases.append(dict(tag_name='launcher-v1.2.7', draft=False,
                                   published_at='2026-10-16T00:00:00Z'))
    publisher.preflight(); publisher.prepare()
    assert publisher.previous['tag_name'] == 'v0.2.0-alpha.2'
    assert 'Changes since v0.2.0-alpha.2' in publisher.notes.read_text(encoding='utf-8')
    # FakePublisher also asserts generate-notes receives the previous game tag.


@pytest.mark.parametrize('files, merged, sha, included', [
    (['src/pc_network.c'], True, HEAD, True),
    (['tools/build-game-icon.ps1'], True, HEAD, True),
    (['tools/launcher/IconBuilder.cs'], True, HEAD, True),
    (['tools/launcher/GameMonogram.cs'], True, HEAD, True),
    (['tools/launcher/Core.cs', 'STATUS.md', 'VERSION'], True, HEAD, False),
    (['tools/build-launcher.ps1', 'docs/windows-launcher.md'], True, HEAD, False),
    (['tools/launcher/Core.cs', 'tools/package-release.py'], True, HEAD, False),
    (['tools/apply-patches.py', '.github/workflows/runtime-patches.yml'], True, HEAD, False),
    (['tools/launcher/GameSettings.cs', 'src/pc_display.c'], True, HEAD, True),
    (['config/pc-controls.default.json'], True, HEAD, True),
    (['tools/publish-release.py', '.github/workflows/runtime-patches.yml'], True, HEAD, False),
    (['src/pc_network.c'], False, HEAD, False),
    (['src/pc_network.c'], True, PREVIOUS, False),
])
def test_notes_select_merged_game_prs_by_files_and_commit_range(publisher, monkeypatch, files, merged, sha, included):
    publisher.preflight()
    def gh(*args, **kwargs):
        if '--paginate' in args:
            return json.dumps([[dict(filename=name)] for name in files])
        return json.dumps(dict(merged_at='2026-10-17' if merged else None,
                               merge_commit_sha=sha, title='Launcher and game <update>', changed_files=len(files)))
    monkeypatch.setattr(publisher, 'gh', gh)
    notes = publisher.game_pr_notes('https://github.com/owner/repo/pull/12\nhttps://github.com/owner/repo/pull/12')
    assert ('[#12]' in notes) == included
    assert notes.count('[#12]') == int(included)
    assert 'compare/v0.2.0-alpha.2...v0.2.0-alpha.3' in notes
    assert ('git', ('rev-list', PREVIOUS+'..'+HEAD)) in publisher.calls


def test_notes_classify_renames_and_refuse_truncated_file_lists(publisher, monkeypatch):
    publisher.preflight()
    count = 1
    def gh(*args, **kwargs):
        if '--paginate' in args:
            return json.dumps([[dict(filename='archive/old.c', previous_filename='src/old.c')]])
        return json.dumps(dict(merged_at='2026-10-17', merge_commit_sha=HEAD,
                               title='Move old code', changed_files=count))
    monkeypatch.setattr(publisher, 'gh', gh)
    assert '[#12]' in publisher.game_pr_notes('https://github.com/owner/repo/pull/12')
    count = 2
    with pytest.raises(ValueError, match='Incomplete changed-file list'):
        publisher.game_pr_notes('https://github.com/owner/repo/pull/12')


def test_no_merged_prs_has_explicit_empty_message(publisher):
    publisher.preflight()
    notes = publisher.game_pr_notes('## New Contributors\nNo PR links here.')
    assert 'No merged game-related pull requests' in notes


def test_semver_order_and_origin_validation():
    versions = ['0.2.0-alpha.2', '0.2.0-alpha.10', '0.2.0-beta.1', '0.2.0', '0.2.1']
    assert sorted(versions, key=module.version_key) == versions
    with pytest.raises(ValueError): module.version_key('0.2.0-alpha.01')
    assert module.github_repository('git@github.com:owner/repo.git') == 'owner/repo'
    with pytest.raises(ValueError): module.github_repository('https://token@github.com/owner/repo')


def test_failed_build_prevents_packaging_and_publication(publisher, monkeypatch):
    publisher.preflight()
    commands = []
    def fail_build(arguments, **kwargs):
        commands.append(arguments)
        if arguments[0] == 'powershell.exe':
            raise RuntimeError('build failed')
        return ''
    monkeypatch.setattr(publisher, 'run', fail_build)
    with pytest.raises(RuntimeError, match='build failed'):
        module.Publisher.build_package(publisher)
    assert commands[0] == [publisher.python, '-m', 'pip', 'install', '-r',
                           publisher.root/'requirements.txt']
    assert len(commands) == 3 and not publication_calls(publisher)


def test_launcher_install_rejection_stops_before_portable_checks_or_publication(publisher, monkeypatch):
    publisher.preflight()
    publisher.output.mkdir(parents=True)
    commands = []
    def run(arguments, **kwargs):
        commands.append(arguments)
        if publisher.root/'tools/package-release.py' in arguments:
            FakePublisher.build_package(publisher)
        if '--verify-package' in arguments:
            assert arguments[2:] == [publisher.archive, publisher.tag]
            raise RuntimeError('launcher rejected package')
        return ''
    monkeypatch.setattr(publisher, 'run', run)
    with pytest.raises(RuntimeError, match='launcher rejected package'):
        module.Publisher.build_package(publisher)
    assert not any(publisher.root/'tests/portable_online.py' in command for command in commands)
    assert not publisher.plan.exists() and not publication_calls(publisher)


def test_dependency_install_failure_stops_before_build_or_tests(publisher, monkeypatch):
    publisher.preflight()
    commands = []
    def fail_install(arguments, **kwargs):
        commands.append(arguments)
        raise RuntimeError('dependency installation failed')
    monkeypatch.setattr(publisher, 'run', fail_install)
    with pytest.raises(RuntimeError, match='dependency installation failed'):
        module.Publisher.build_package(publisher)
    assert commands == [[publisher.python, '-m', 'pip', 'install', '-r',
                         publisher.root/'requirements.txt']]
    assert not publication_calls(publisher)


def test_failed_cleanup_preserves_original_test_error(publisher, monkeypatch, capsys):
    publisher.preflight()
    def run(arguments, **kwargs):
        if '-m' in arguments and 'pytest' in arguments:
            (publisher.root / 'build/release-tests').mkdir(parents=True)
            raise RuntimeError('tests failed')
        return ''
    def fail_cleanup(path):
        raise RuntimeError('cleanup unavailable')
    monkeypatch.setattr(publisher, 'run', run)
    monkeypatch.setattr(publisher, 'recycle', fail_cleanup)
    with pytest.raises(RuntimeError, match='^tests failed$'):
        module.Publisher.build_package(publisher)
    assert 'cleanup unavailable' in capsys.readouterr().err


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows cleanup script')
@pytest.mark.parametrize('relative', [
    'build/release-tests',
    'build/releases/TimeSplitters-Future-Perfect-PC-v0.2.0-alpha.2-windows-x64',
    'private/saves',
])
def test_release_cleanup_targets_only_disposable_work(tmp_path, relative):
    script = tmp_path / 'tools/recycle-release-work.ps1'
    script.parent.mkdir()
    shutil.copyfile(module.ROOT / 'tools/recycle-release-work.ps1', script)
    target = tmp_path / relative
    target.mkdir(parents=True)
    (target / 'fixture.txt').write_text('disposable fixture')
    archive = tmp_path / 'build/releases/keep.zip'
    archive.parent.mkdir(parents=True, exist_ok=True)
    archive.write_bytes(b'keep')
    result = subprocess.run(['powershell.exe', '-NoProfile', '-File', str(script),
                             '-LiteralPath', str(target)], capture_output=True, text=True)
    if relative == 'private/saves':
        assert result.returncode != 0
        assert (target / 'fixture.txt').exists()
    else:
        assert result.returncode == 0, result.stderr
        assert not target.exists()
    assert archive.read_bytes() == b'keep'


def test_release_lock_prevents_overlapping_builds(tmp_path):
    with module.release_lock(tmp_path/'release.lock'):
        with pytest.raises(ValueError, match='already running'):
            with module.release_lock(tmp_path/'release.lock'):
                pytest.fail('Second release acquired the lock')


def test_main_moving_after_preparation_prevents_upload(publisher):
    publisher.preflight(); publisher.prepare()
    publisher.remote = PREVIOUS
    with pytest.raises(ValueError, match='main changed'):
        publisher.publish()
    assert not publication_calls(publisher)


def test_changed_asset_between_prepare_and_publish_is_blocked(publisher):
    publisher.preflight(); publisher.prepare()
    publisher.archive.write_bytes(b'changed immediately before upload')
    with pytest.raises(ValueError, match='changed before upload'):
        publisher.publish()
    assert not publication_calls(publisher)
