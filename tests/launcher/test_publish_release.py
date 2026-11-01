"""Release orchestration checks use tiny fixtures and a simulated GitHub CLI.

No real tag, release, game data or personal saves are touched.
"""


import importlib.util
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zipfile

import pytest

spec = importlib.util.spec_from_file_location('publish_release', Path(__file__).resolve().parents[2]/'tools/release/publish-release.py')
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
        (root/'VERSION').write_text('0.2.0-beta.3')
        pack = root/'assets/textures'
        pack.mkdir(parents=True)
        words = [0]*32
        for index, value in {0: 0x20534444, 1: 124, 3: 1, 4: 1, 7: 1,
                             19: 32, 20: 4, 21: 0x31545844}.items():
            words[index] = value
        texture = pack/'0123456789abcdef.dds'
        texture.write_bytes(struct.pack('<32I', *words)+b'\0'*8)
        texture.with_suffix('.json').write_text(json.dumps({'key': texture.stem, 'sha256': module.digest(texture)}))
        _, records, identity = module.TEXTURES.ensure_pack(root)
        texture.with_suffix('.json').write_bytes(records[0][1])
        (root/'config/textures').mkdir(parents=True)
        (root/'config/textures/texture-pack.json').write_text(json.dumps({
            'schema': 2, 'delivery': 'bundled', 'path': 'assets/textures',
            'texture_count': identity['texture_count'], 'installed_bytes': identity['installed_bytes']}))
        self.calls = []
        self.branch, self.dirty, self.remote = 'main', False, HEAD
        self.releases = [dict(tag_name='v0.2.0-beta.2', draft=False, prerelease=True, published_at='2026-10-15T01:00:00Z')]
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
            return tag if len(args) == 3 else self.remote+'\trefs/heads/'+self.source_branch+ ('\n'+tag if tag else '')
        if args[0] == 'merge-base':
            return ''
        if args[0] == 'log':
            return HEAD+'\tAdd Online hosting and <diagnostics>'
        if args[0] == 'rev-list':
            return HEAD
        raise AssertionError(args)

    def gh(self, *args, payload=None):
        self.calls.append(('gh', args, payload))
        if args[:2] == ('api', 'repos/owner/repo/releases/latest'):
            return json.dumps({'tag_name': 'launcher-v1.3.0', 'draft': False, 'prerelease': False,
                               'published_at': '2026-10-20', 'assets': [
                                   {'name': 'TimeSplitters-Launcher-v1.3.0-'+platform+'.zip', 'state': 'uploaded', 'size': 1}
                                   for platform in ('windows-x64', 'macos-arm64')]})
        if args[:2] == ('auth', 'status'):
            return ''
        if args[:2] == ('api', '--paginate') and '/pulls/' in args[-1]:
            return json.dumps([[dict(filename='src/network/pc_network.c')]])
        if args[:2] == ('api', '--paginate'):
            return json.dumps([self.releases])
        if 'generate-notes' in str(args):
            assert payload.get('previous_tag_name') == (self.previous['tag_name'] if self.previous else None)
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
        build = dict(source_commit=HEAD, version=self.tag, release_channel=module.CHANNELS.identity(self.tag), source_changes=[], textures=self.texture_identity,
                     files={'Bin/game.exe':module.digest(binary), **self.texture_files})
        (self.stage/'BUILD.json').write_text(json.dumps(build))
        with zipfile.ZipFile(self.archive, 'x') as archive:
            for path in (binary, self.stage/'BUILD.json'):
                archive.write(path, self.name+'/'+path.relative_to(self.stage).as_posix())
            _, records, _ = module.TEXTURES.ensure_pack(self.root)
            module.TEXTURES.write_zip(archive, self.name+'/', records)
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
    assert 'Changes since v0.2.0-beta.2' in notes and '- Online multiplayer ([#12]' in notes
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
    if problem == 'published': publisher.releases[0]['tag_name'] = 'v0.2.0-beta.3'
    if problem == 'version': (publisher.root/'VERSION').write_text('0.2.0-beta.1')
    with pytest.raises(ValueError):
        publisher.preflight()
    assert not publication_calls(publisher) and publisher.builds == 0


@pytest.mark.parametrize('problem', ['missing', 'pointer', 'corrupt', 'private_metadata', 'count', 'size', 'delivery', 'path'])
def test_preflight_requires_complete_hydrated_repository_textures(publisher, problem):
    pack = publisher.root/'assets/textures'
    path = pack/'0123456789abcdef.dds'
    manifest_path = publisher.root/'config/textures/texture-pack.json'
    manifest = json.loads(manifest_path.read_text())
    if problem == 'missing':
        pack.rename(pack.with_name('preserved-textures'))
    elif problem == 'pointer':
        path.write_bytes(b'version https://git-lfs.github.com/spec/v1\noid sha256:abc\nsize 136\n')
    elif problem == 'corrupt':
        path.write_bytes(b'broken DDS')
    elif problem == 'private_metadata':
        metadata = json.loads(path.with_suffix('.json').read_text())
        metadata['prompt'] = 'private art instruction'
        path.with_suffix('.json').write_text(json.dumps(metadata))
    else:
        key = {'count': 'texture_count', 'size': 'installed_bytes'}.get(problem, problem)
        manifest[key] = manifest[key]+1 if problem in ('count', 'size') else 'legacy'
        manifest_path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError):
        publisher.preflight()
    assert publisher.builds == 0 and not publisher.tag_created and not publication_calls(publisher)


@pytest.mark.parametrize('action', ['prepare', 'publish'])
@pytest.mark.parametrize('problem', ['omitted', 'missing_dds', 'missing_json', 'hash', 'count', 'size', 'local'])
def test_resumed_release_cannot_bypass_required_texture_pack(publisher, action, problem):
    publisher.preflight(); publisher.prepare()
    assert not publisher.stage.exists()
    build = json.loads(publisher.metadata.read_text())
    if problem == 'omitted':
        build.pop('textures')
        build['files'] = {name: sha for name, sha in build['files'].items() if not name.startswith('Textures/')}
    elif problem.startswith('missing_'):
        build['files'].pop('Textures/0123456789abcdef.'+problem.removeprefix('missing_'))
    elif problem == 'hash':
        build['files']['Textures/0123456789abcdef.dds'] = '0'*64
    elif problem in ('count', 'size'):
        build['textures'][{'count': 'texture_count', 'size': 'installed_bytes'}[problem]] += 1
    else:
        build['textures']['source'] = 'local'
    with zipfile.ZipFile(publisher.archive) as archive:
        contents = {publisher.name+'/'+name: archive.read(publisher.name+'/'+name) for name in build['files']}
    with zipfile.ZipFile(publisher.archive, 'w') as archive:
        for name, data in contents.items():
            archive.writestr(name, data)
        archive.writestr(publisher.name+'/BUILD.json', json.dumps(build))
    publisher.metadata.write_text(json.dumps(build))
    publisher.checksum.write_text(module.digest(publisher.archive)+'  '+publisher.archive.name+'\n')
    # Simulate a previously prepared package whose own receipts all agree.
    plan = json.loads(publisher.plan.read_text())
    plan['assets'] = {path.name: module.digest(path) for path in (publisher.archive, publisher.metadata, publisher.checksum)}
    publisher.plan.write_text(json.dumps(plan))
    with pytest.raises(ValueError, match='every repository texture'):
        getattr(publisher, action)()
    assert not publisher.tag_created and not publication_calls(publisher)


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


@pytest.mark.parametrize('version', ['0.2.0-beta.3', '0.2.0'])
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


def test_package_reuses_only_successful_decompression(publisher, monkeypatch):
    publisher.preflight(); publisher.output.mkdir(parents=True); publisher.build_package()
    def unexpected_read(*args, **kwargs):
        raise AssertionError('Unchanged archive must not be decompressed again')
    monkeypatch.setattr(module.zipfile, 'ZipFile', unexpected_read)
    publisher.verify_package()
    # Same size and timestamp do not authorize reuse of different archive bytes.
    previous = publisher.archive.stat()
    data = bytearray(publisher.archive.read_bytes()); data[-1] ^= 1
    publisher.archive.write_bytes(data)
    import os
    os.utime(publisher.archive, ns=(previous.st_atime_ns, previous.st_mtime_ns))
    with pytest.raises(ValueError, match='checksum mismatch'):
        publisher.verify_package()


def test_changed_metadata_does_not_reuse_decompression(publisher):
    publisher.preflight(); publisher.output.mkdir(parents=True); publisher.build_package()
    path = publisher.stage/'BUILD.json'
    build = json.loads(path.read_text()); build['extra'] = 'changed'
    path.write_text(json.dumps(build))
    with pytest.raises(ValueError, match='Archived build metadata differs'):
        publisher.verify_package()


def test_crc_corruption_with_updated_outer_hash_still_fails(publisher):
    publisher.preflight(); publisher.output.mkdir(parents=True); publisher.build_package()
    # FakePublisher writes ZIP_STORED. Change actual payload, retaining the
    # original CRC and updating the outer checksum to force member validation.
    with zipfile.ZipFile(publisher.archive) as archive:
        info = archive.getinfo(publisher.name+'/Bin/game.exe')
    data = bytearray(publisher.archive.read_bytes())
    filename_length, extra_length = struct.unpack_from('<HH', data, info.header_offset+26)
    data[info.header_offset+30+filename_length+extra_length] ^= 1
    publisher.archive.write_bytes(data)
    publisher.checksum.write_text(module.digest(publisher.archive)+'  '+publisher.archive.name+'\n')
    for _ in range(2):
        with pytest.raises(zipfile.BadZipFile, match='CRC'):
            publisher.verify_package()


def test_fresh_publisher_deep_checks_instead_of_trusting_memory(publisher, monkeypatch):
    publisher.preflight(); publisher.output.mkdir(parents=True); publisher.build_package()
    publisher._verified_archive = None  # A new process has no successful inspection.
    with monkeypatch.context() as patch:
        patch.setattr(module.zipfile.ZipFile, 'testzip', lambda *_: pytest.fail('Duplicate CRC scan'))
        publisher.verify_package()


def test_timing_records_failed_checks_without_swallowing_errors(publisher):
    with pytest.raises(RuntimeError, match='failed check'):
        with publisher.timed('fixture'):
            raise RuntimeError('failed check')
    assert publisher.timings[-1]['outcome'] == 'failed'
    assert publisher.timings[-1]['seconds'] >= 0
    publisher.output.mkdir(parents=True)
    publisher.save_timings()
    saved = json.loads((publisher.output/'last-run-timings.json').read_text())
    assert saved['stages'] == publisher.timings
    assert not (publisher.output/'last-run-timings.json.tmp').exists()


@pytest.mark.parametrize('corrupt', [False, True])
def test_streamed_textures_validate_from_zip_without_stage_copy(publisher, corrupt):
    publisher.preflight(); publisher.output.mkdir(parents=True); publisher.build_package()
    name = 'Textures/0123456789abcdef.dds'
    with zipfile.ZipFile(publisher.archive) as archive:
        contents = {entry: archive.read(entry) for entry in archive.namelist()}
    if corrupt:
        contents[publisher.name+'/'+name] = b'corrupt'
    with zipfile.ZipFile(publisher.archive, 'w') as archive:
        for entry, data in contents.items():
            archive.writestr(entry, data)
    publisher.checksum.write_text(module.digest(publisher.archive)+'  '+publisher.archive.name+'\n')
    assert not (publisher.stage/'Textures').exists()
    if corrupt:
        with pytest.raises(ValueError, match='Archived file changed'):
            publisher.verify_package()
    else:
        publisher.verify_package()


def test_published_baseline_includes_prereleases_and_ignores_drafts():
    stable = dict(tag_name='v0.1.0', draft=False, published_at='2026-01-01T00:00:00Z')
    preview = dict(tag_name='v0.2.0-beta.2', draft=False, published_at='2026-10-01T00:00:00Z')
    draft = dict(tag_name='v9.0.0', draft=True, published_at=None)
    assert module.last_published([[stable], [preview, draft]])[1] == preview


@pytest.mark.parametrize('tag', ['launcher-v1.2.7', 'other-release', 'vbad', 'v0.2.0-alpha.01', 'vv1.0.0'])
def test_game_baseline_ignores_unrelated_or_invalid_tags(tag):
    game = dict(tag_name='v0.2.0-beta.2', draft=False, published_at='2026-10-01T00:00:00Z')
    other = dict(tag_name=tag, draft=False, published_at='2026-10-16T00:00:00Z')
    releases, previous = module.last_published([[game], [other]])
    assert releases == [game, other]  # Keep every tag for collision checks.
    assert previous == game
    assert module.last_published([[other]])[1] is None


def test_newer_launcher_does_not_change_game_notes_baseline(publisher):
    publisher.releases.append(dict(tag_name='launcher-v1.2.7', draft=False,
                                   published_at='2026-10-16T00:00:00Z'))
    publisher.preflight(); publisher.prepare()
    assert publisher.previous['tag_name'] == 'v0.2.0-beta.2'
    assert 'Changes since v0.2.0-beta.2' in publisher.notes.read_text(encoding='utf-8')
    # FakePublisher also asserts generate-notes receives the previous game tag.


@pytest.mark.parametrize('files, merged, sha, included', [
    (['src/network/pc_network.c'], True, HEAD, True),
    (['tools/build/build-game-icon.ps1'], True, HEAD, True),
    (['tools/launcher/IconBuilder.cs'], True, HEAD, True),
    (['tools/launcher/GameMonogram.cs'], True, HEAD, True),
    (['tools/launcher/Core.cs', 'docs/project/STATUS.md', 'VERSION'], True, HEAD, False),
    (['tools/build/build-launcher.ps1', 'docs/players/windows-launcher.md'], True, HEAD, False),
    (['tools/launcher/Core.cs', 'tools/release/package-release.py'], True, HEAD, False),
    (['tools/build/apply-patches.py', '.github/workflows/runtime-patches.yml'], True, HEAD, False),
    (['tools/launcher/GameSettings.cs', 'src/interface/pc_display.c'], True, HEAD, True),
    (['config/pc-controls.default.json'], True, HEAD, True),
    (['tools/release/publish-release.py', '.github/workflows/runtime-patches.yml'], True, HEAD, False),
    (['src/network/pc_network.c'], False, HEAD, False),
    (['src/network/pc_network.c'], True, PREVIOUS, False),
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
    assert 'compare/v0.2.0-beta.2...v0.2.0-beta.3' in notes
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
    assert len(commands) == 2 and not publication_calls(publisher)
    assert not any('apply-patches.py' in str(command) for command in commands)


def test_launcher_install_rejection_stops_before_portable_checks_or_publication(publisher, monkeypatch):
    publisher.preflight()
    publisher.output.mkdir(parents=True)
    commands = []
    def run(arguments, **kwargs):
        commands.append(arguments)
        if publisher.root/'tools/release/package-release.py' in arguments:
            FakePublisher.build_package(publisher)
        if '--verify-package' in arguments:
            assert arguments[2:] == [publisher.archive, publisher.tag]
            raise RuntimeError('launcher rejected package')
        return ''
    monkeypatch.setattr(publisher, 'run', run)
    with pytest.raises(RuntimeError, match='launcher rejected package'):
        module.Publisher.build_package(publisher)
    assert not any(publisher.root/'tests/network/portable_online.py' in command for command in commands)
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
    'build/releases/TimeSplitters-Future-Perfect-PC-v0.2.0-beta.2-windows-x64',
    'private/saves',
])
def test_release_cleanup_targets_only_disposable_work(tmp_path, relative):
    script = tmp_path / 'tools/release/recycle-release-work.ps1'
    script.parent.mkdir(parents=True)
    shutil.copyfile(module.ROOT / 'tools/release/recycle-release-work.ps1', script)
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


def test_relocated_standard_library_preserves_ssl_trust_resources(tmp_path):
    import ssl
    spec = importlib.util.spec_from_file_location('bundle_online', module.ROOT/'tools/network/bundle_online.py')
    bundler = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bundler)
    base, runtime = tmp_path/'base', tmp_path/'relocated'
    library = base/'Lib'
    library.mkdir(parents=True); runtime.mkdir()
    original = Path(ssl.__file__)
    shutil.copy2(original, library/'ssl.py')
    resources = list(original.parent.glob('*.pem'))
    for path in resources:
        shutil.copy2(path, library/path.name)
    (library/'portable_fixture.py').write_text('VALUE = 42\n')
    bundler.bundle_standard_library(base, runtime, 'test')
    with zipfile.ZipFile(runtime/'pythontest.zip') as archive:
        assert archive.namelist() == ['portable_fixture.py']
    assert (runtime/'ssl.py').read_bytes() == original.read_bytes()
    for path in resources:
        assert (runtime/path.name).read_bytes() == path.read_bytes()
    # Actual SSLContext construction failed in alpha.2 preparation when its
    # module-relative trust policy was absent or addressed inside the ZIP.
    script = '''import pathlib, sys
root = pathlib.Path(sys.argv[1])
sys.path[:0] = [str(root/'pythontest.zip'), str(root)]
import ssl, portable_fixture
assert pathlib.Path(ssl.__file__) == root/'ssl.py'
assert portable_fixture.VALUE == 42
ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
print('PASS: relocated SSL context and unchanged trust policy')
'''
    result = subprocess.run([sys.executable, '-I', '-S', '-c', script, str(runtime)],
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout+result.stderr


def test_main_moving_after_preparation_prevents_upload(publisher):
    publisher.preflight(); publisher.prepare()
    publisher.remote = PREVIOUS
    with pytest.raises(ValueError, match='source branch changed'):
        publisher.publish()
    assert not publication_calls(publisher)


def test_changed_asset_between_prepare_and_publish_is_blocked(publisher):
    publisher.preflight(); publisher.prepare()
    publisher.archive.write_bytes(b'changed immediately before upload')
    with pytest.raises(ValueError, match='changed before upload'):
        publisher.publish()
    assert not publication_calls(publisher)


def test_large_texture_pr_uses_verified_local_merge_diff(publisher, monkeypatch):
    publisher.preflight()
    old_git = publisher.git
    def git(*args):
        if args[0] == 'diff':
            assert args == ('diff', '--no-renames', '--name-only', '-z', HEAD+'^1', HEAD)
            return 'assets/textures/0123456789abcdef.dds\0'
        return old_git(*args)
    def gh(*args, **kwargs):
        if '--paginate' in args:
            return json.dumps([[{'filename': 'docs/'+str(i)} for i in range(3000)]])
        return json.dumps(dict(merged_at='2026-10-20', merge_commit_sha=HEAD,
                               title='Texture pack', changed_files=3040))
    monkeypatch.setattr(publisher, 'git', git)
    monkeypatch.setattr(publisher, 'gh', gh)
    assert '[#12]' in publisher.game_pr_notes('https://github.com/owner/repo/pull/12')


@pytest.mark.parametrize('version,draft,missing,passes', [
    ('1.2.21', False, False, False), ('1.2.22', True, False, False),
    ('1.2.22', False, True, False), ('1.3.0', False, False, True)])
def test_bundled_game_requires_published_compatible_launcher(publisher, monkeypatch, version, draft, missing, passes):
    publisher.preflight()
    publisher.output.mkdir(parents=True, exist_ok=True)
    publisher.metadata.write_text(json.dumps({'textures': publisher.texture_identity, 'files': publisher.texture_files}))
    release = {'tag_name': 'launcher-v'+version, 'draft': draft, 'prerelease': False,
               'published_at': '2026-10-20', 'assets': [
                   {'name': f'TimeSplitters-Launcher-v{version}-{platform}.zip', 'state': 'uploaded', 'size': 1}
                   for platform in (['windows-x64'] if missing else ['windows-x64', 'macos-arm64'])]}
    monkeypatch.setattr(publisher, 'gh', lambda *args, **kwargs: json.dumps(release))
    if passes:
        publisher.require_launcher()
    else:
        with pytest.raises(ValueError, match='launcher'):
            publisher.require_launcher()


@pytest.mark.parametrize('version,passes', [('1.2.22', False), ('1.2.23', False), ('1.3.0', True)])
def test_retired_textures_require_launcher_with_transactional_migration(publisher, monkeypatch, version, passes):
    publisher.preflight()
    publisher.output.mkdir(parents=True, exist_ok=True)
    publisher.texture_identity['retired'] = {'fedcba9876543210': {'dds': 'a'*64, 'json': 'b'*64}}
    publisher.metadata.write_text(json.dumps({'textures': publisher.texture_identity, 'files': publisher.texture_files}))
    latest = {'tag_name': 'launcher-v'+version, 'draft': False, 'prerelease': False,
              'published_at': '2026-10-22', 'assets': [
                  {'name': f'TimeSplitters-Launcher-v{version}-{platform}.zip', 'state': 'uploaded', 'size': 1}
                  for platform in ('windows-x64', 'macos-arm64')]}
    monkeypatch.setattr(publisher, 'gh', lambda *args, **kwargs: json.dumps(latest))
    if passes:
        publisher.require_launcher()
    else:
        with pytest.raises(ValueError, match='launcher 1.3.0'):
            publisher.require_launcher()


def test_optional_texture_policy_needs_no_lfs_and_rejects_bundled_payload(publisher):
    manifest_path = publisher.root/'config/textures/texture-pack.json'
    manifest = json.loads(manifest_path.read_text())
    manifest['delivery'] = 'optional'
    manifest_path.write_text(json.dumps(manifest))
    (publisher.root/'assets/textures').rename(publisher.root/'assets/preserved-textures')
    publisher.require_repository_textures()
    publisher.require_package_textures({'textures': None, 'files': {'Bin/game.exe': 'a'*64}})
    with pytest.raises(ValueError):
        publisher.require_package_textures({'textures': None, 'files': {'Textures/0123456789abcdef.dds': 'a'*64}})


@pytest.mark.parametrize('version,channel', [
    ('0.4.1', 'stable'), ('0.5.0-story-coop.1', 'story-coop'),
    ('0.5.0-story-coop-beta.1', 'story-coop-beta'),
    ('0.5.0-beta.2', 'beta'), ('0.3.1-alpha.2', 'legacy'), ('0.4.0-preview.1', 'legacy')])
def test_game_channel_identity(version, channel):
    assert module.CHANNELS.identity(version)['channel'] == channel


@pytest.mark.parametrize('version', ['0.4.0-stable.1', '0.4.0-story-coop.0',
    '0.4.0-story-coop.01', '0.4.0-Story-Coop.1', '0.4.0-coop', '0.4.0+local'])
def test_invalid_channel_versions_are_rejected(version):
    with pytest.raises(ValueError): module.CHANNELS.identity(version)


def test_preview_branch_publishes_exact_pushed_source_without_main_merge(publisher):
    publisher.branch = publisher.source_branch = 'story-coop'
    (publisher.root/'VERSION').write_text('0.5.0-story-coop.1')
    publisher.releases.append(dict(tag_name='v0.4.0', draft=False, prerelease=False, published_at='2026-10-16'))
    publisher.preflight()
    assert publisher.channel['channel'] == 'story-coop'
    assert publisher.previous['tag_name'] == 'v0.4.0'
    assert publisher.channel['stable_base'] == 'v0.4.0'
    publisher.prepare()
    publisher.publish()
    assert '--prerelease' in publication_calls(publisher)[0][1]
    assert 'Story Coop Preview' in publisher.notes.read_text()
    assert '<!-- tsfp-release ' in publisher.notes.read_text()


def test_stable_hotfix_is_not_blocked_by_newer_experiment(publisher):
    publisher.branch = publisher.source_branch = 'hotfix/0.4.1'
    (publisher.root/'VERSION').write_text('0.4.1')
    publisher.releases = [dict(tag_name=tag, draft=False, prerelease='-' in tag, published_at=date)
                          for tag,date in [('v0.4.0','2026-10-15'), ('v0.5.0-story-coop.8','2026-10-22')]]
    publisher.preflight()
    assert publisher.previous['tag_name'] == 'v0.4.0'
    publisher.prepare()
    publisher.publish()
    assert '--prerelease' not in publication_calls(publisher)[0][1]
    assert '--latest=false' in publication_calls(publisher)[0][1]


def test_first_stable_notes_compare_against_latest_published_alpha(publisher):
    (publisher.root/'VERSION').write_text('0.3.2')
    publisher.releases = [dict(tag_name=tag, draft=False, prerelease=True, published_at=date)
                          for tag, date in [('v0.3.1-alpha.1', '2026-10-01'),
                                            ('v0.3.1-alpha.2', '2026-10-20'),
                                            ('v0.4.0-beta.1', '2026-10-21'),
                                            ('launcher-v1.3.0', '2026-10-23')]]
    publisher.releases.append(dict(tag_name='v0.3.1-alpha.3', draft=True, prerelease=True,
                                   published_at='2026-10-22'))
    publisher.preflight()
    assert publisher.previous['tag_name'] == 'v0.3.1-alpha.2'
    assert publisher.baseline == PREVIOUS
    assert publisher.stable_base is None
    publisher.output.mkdir(parents=True)
    publisher.write_notes()  # Fake GitHub checks the explicit previous_tag_name.
    notes = publisher.notes.read_text()
    assert '## Changes since v0.3.1-alpha.2' in notes
    assert '/compare/v0.3.1-alpha.2...v0.3.2' in notes
    assert ('git', ('rev-list', f'{PREVIOUS}..{HEAD}')) in publisher.calls


@pytest.mark.parametrize('version', ['0.3.3', '0.4.0'])
def test_later_stable_notes_prefer_stable_over_newer_previews(publisher, version):
    (publisher.root/'VERSION').write_text(version)
    publisher.releases = [dict(tag_name=tag, draft=False, prerelease='-' in tag,
                               published_at=f'2026-10-{day}') for tag, day in [
        ('v0.2.9', '10'), ('v0.3.2', '20'), ('v0.3.3-beta.1', '21')]]
    publisher.preflight()
    assert publisher.previous['tag_name'] == 'v0.3.2'


def test_first_stable_uses_latest_publication_but_skips_nonancestor(publisher, monkeypatch):
    (publisher.root/'VERSION').write_text('0.3.2')
    publisher.releases = [dict(tag_name=tag, draft=False, prerelease=True, published_at=date)
                          for tag, date in [('v0.3.1-alpha.3', '2026-10-18'),
                                            ('v0.3.1-alpha.2', '2026-10-20'),
                                            ('v0.3.1-story-coop.1', '2026-10-22')]]
    original = publisher.git
    def git(*args):
        if args[0] == 'rev-parse' and args[-1] == 'refs/tags/v0.3.1-story-coop.1^{commit}':
            return 'c'*40
        if args[0] == 'merge-base' and args[2] == 'c'*40:
            raise RuntimeError('not an ancestor')
        return original(*args)
    monkeypatch.setattr(publisher, 'git', git)
    publisher.preflight()
    assert publisher.previous['tag_name'] == 'v0.3.1-alpha.2'


def test_first_stable_preview_baseline_does_not_allow_maintenance_branch(publisher):
    publisher.branch = publisher.source_branch = 'hotfix/0.3.2'
    (publisher.root/'VERSION').write_text('0.3.2')
    with pytest.raises(ValueError, match='descend from a published Stable'):
        publisher.preflight()


def test_unrelated_channel_and_nonancestor_releases_are_not_note_baselines(publisher, monkeypatch):
    (publisher.root/'VERSION').write_text('0.5.0-story-coop.2')
    publisher.releases = [dict(tag_name=tag, draft=False, prerelease='-' in tag, published_at='2026-10-22')
                          for tag in ['v0.4.0','v0.4.1','v0.5.0-story-coop.1','v0.6.0-other.9']]
    original = publisher.git
    def git(*args):
        if args[0]=='rev-parse' and args[-1] in ('refs/tags/v0.4.1^{commit}', 'refs/tags/v0.5.0-story-coop.1^{commit}'):
            return 'c'*40
        if args[0]=='merge-base' and args[2]=='c'*40:
            raise RuntimeError('not an ancestor')
        return original(*args)
    monkeypatch.setattr(publisher,'git',git)
    publisher.preflight()
    assert publisher.previous['tag_name']=='v0.4.0'
    assert publisher.channel['latest_stable']=='v0.4.1'
    assert not publisher.channel['includes_latest_stable']
    publisher.output.mkdir(parents=True)
    publisher.write_notes()
    assert 'does not include the latest Stable release, v0.4.1' in publisher.notes.read_text()


def test_channel_display_identity_cannot_change_on_prepared_retry(publisher):
    publisher.preflight();publisher.prepare()
    publisher.channel['name']='Changed name'
    with pytest.raises(ValueError,match='Prepared output'):
        publisher.prepare()


def test_stable_branch_without_stable_ancestor_is_rejected(publisher):
    publisher.branch = publisher.source_branch = 'hotfix/0.4.1'
    (publisher.root/'VERSION').write_text('0.4.1')
    with pytest.raises(ValueError,match='descend from a published Stable'):
        publisher.preflight()


@pytest.mark.parametrize('version', ['0.3.2', '0.4.0-beta.1', '0.4.0-story-coop.1'])
def test_version_preparation_writes_only_explicit_version(tmp_path, monkeypatch, version):
    import sys
    monkeypatch.setitem(sys.modules,'release_channels',module.CHANNELS)
    spec = importlib.util.spec_from_file_location('release_version',module.ROOT/'tools/release/release-version.py')
    prepare = importlib.util.module_from_spec(spec);spec.loader.exec_module(prepare)
    (tmp_path/'VERSION').write_text('0.3.1-alpha.2')
    (tmp_path/'keep').write_text('keep')
    prepare.set_version(tmp_path,version)
    assert (tmp_path/'VERSION').read_text()==version+'\n'
    assert (tmp_path/'keep').read_text()=='keep'
    with pytest.raises(ValueError): prepare.set_version(tmp_path,'0.4.0-rc.1')
    assert (tmp_path/'VERSION').read_text()==version+'\n'



def test_preview_maintainer_notes_and_display_name_are_reviewable(publisher):
    publisher.channel_name='Story Co-op Preview'
    (publisher.root/'VERSION').write_text('0.5.0-story-coop.1')
    publisher.notes_file=publisher.root/'preview-notes.md'
    publisher.notes_file.write_text('## Known issues\nTest peers must use the same game build.')
    publisher.preflight();publisher.output.mkdir(parents=True);publisher.write_notes()
    notes=publisher.notes.read_text()
    assert 'Story Co-op Preview' in notes and 'Test peers must use the same game build.' in notes
    assert '**Update channel** to **Preview**' in notes
    assert '**Story Co-op Preview** under **Testing track**' in notes
    publisher.notes_file.write_text('<!-- tsfp-release {} -->')
    with pytest.raises(ValueError,match='override channel metadata'):
        publisher.write_notes()


@pytest.mark.parametrize('version', ['0.4.0-alpha.1','0.4.0-preview.1','0.4.0-rc.1','0.4.0-legacy.1'])
def test_new_publication_rejects_retired_general_suffixes_but_history_is_readable(publisher, version):
    assert module.CHANNELS.identity(version)['channel']=='legacy'
    with pytest.raises(ValueError,match='Use beta.N'):
        module.CHANNELS.identity(version,publishing=True)
    (publisher.root/'VERSION').write_text(version)
    with pytest.raises(ValueError,match='Use beta.N'):
        publisher.preflight()
    assert not publication_calls(publisher)


def test_beta_has_its_own_canonical_identity_and_fixed_display_name(publisher):
    (publisher.root/'VERSION').write_text('0.4.0-beta.1')
    publisher.preflight()
    assert publisher.channel['channel']=='beta' and publisher.channel['name']=='Beta'
    publisher.channel_name='General Preview'
    with pytest.raises(ValueError,match='track is named Beta'):
        publisher.preflight()


def test_beta_baseline_excludes_legacy_preview_and_alpha_builds(publisher):
    (publisher.root/'VERSION').write_text('0.4.0-beta.2')
    publisher.releases=[dict(tag_name=tag,draft=False,prerelease=True,published_at='2026-10-22')
                        for tag in ['v0.4.0-beta.1','v0.4.0-preview.99','v0.4.0-alpha.99']]
    publisher.preflight()
    assert publisher.previous['tag_name']=='v0.4.0-beta.1'


def test_worker_alias_cleanup_keeps_targets(tmp_path):
    tests = tmp_path/'build/release-tests'
    links = []
    for name in ('popen-gw0', 'popen-gw12'):
        target = tests/name/'compiler0'
        target.mkdir(parents=True)
        (target/'keep.txt').write_text('keep')
        link = target.with_name('compilercurrent')
        link_fixture(link, target)
        links.append(link)
    module.clean_pytest_current_links(tmp_path, tests)
    assert all(not link.is_symlink() for link in links)
    assert len(list(tests.rglob('keep.txt'))) == 2
    module.check_directory(tmp_path, tests)


@pytest.mark.parametrize('kind', ['cross-worker', 'nested', 'redirected-worker', 'unrecognized-worker'])
def test_worker_cleanup_rejects_redirects_without_mutation(tmp_path, kind):
    tests = tmp_path/'build/release-tests'
    target = tests/'popen-gw0/compiler0'
    target.mkdir(parents=True)
    good = target.with_name('compilercurrent')
    link_fixture(good, target)
    if kind == 'cross-worker':
        other = tests/'popen-gw1'; other.mkdir()
        bad = other/'compilercurrent'
    elif kind == 'nested':
        bad = target/'compilercurrent'
    elif kind == 'redirected-worker':
        bad = tests/'popen-gw1'
    else:
        other = tests/'unexpected-worker'; other.mkdir()
        bad = other/'compilercurrent'
    link_fixture(bad, target)
    with pytest.raises(ValueError, match='Unexpected link|Redirected release work path'):
        module.clean_pytest_current_links(tmp_path, tests)
    assert good.is_symlink() and bad.is_symlink() and target.is_dir()


@pytest.mark.parametrize('cpus,expected', [(None, 1), (1, 1), (2, 2), (24, 4)])
def test_default_suite_workers_are_bounded(tmp_path, monkeypatch, cpus, expected):
    monkeypatch.setattr(module.os, 'cpu_count', lambda: cpus)
    assert module.Publisher(tmp_path).test_workers == expected


@pytest.mark.parametrize('workers', [0, -1, 65, '4'])
def test_invalid_suite_worker_count_is_rejected(tmp_path, workers):
    with pytest.raises(ValueError, match='Test workers'):
        module.Publisher(tmp_path, test_workers=workers)


@pytest.mark.parametrize('workers', [1, 4])
def test_suite_runs_every_test_and_failure_stops_packaging(publisher, monkeypatch, workers):
    publisher.preflight()
    publisher.test_workers = workers
    commands = []
    def run(arguments, **kwargs):
        commands.append(arguments)
        if 'pytest' in arguments:
            raise RuntimeError('regression rejected candidate')
        return ''
    monkeypatch.setattr(publisher, 'run', run)
    with pytest.raises(RuntimeError, match='regression rejected candidate'):
        module.Publisher.build_package(publisher)
    command = commands[-1]
    assert command[:5] == [publisher.python, '-m', 'pytest', 'tests', '-q']
    assert '-k' not in command and '--lf' not in command
    assert '--suite-profile=release-full' in command
    if workers > 1:
        assert command[-4:] == ['-n', '4', '--dist=loadgroup', '--max-worker-restart=0']
    else:
        assert '-n' not in command
    assert not any('package-release.py' in str(arg) for args in commands for arg in args)


def test_parallel_pytest_session_fixtures_cleanup(tmp_path):
    tests = tmp_path/'build/release-tests'
    tests.parent.mkdir()
    fixture = tmp_path/'test_workers.py'
    fixture.write_text('''import pytest
@pytest.fixture(scope="session")
def compiled(tmp_path_factory):
    path = tmp_path_factory.mktemp("compiler")
    (path/"keep.txt").write_text("keep")
    return path
@pytest.mark.parametrize("case", range(4))
def test_compiled(compiled, case):
    assert (compiled/"keep.txt").read_text() == "keep"
''')
    result = subprocess.run([sys.executable, '-m', 'pytest', str(fixture), '-q',
                             '-n', '2', '--max-worker-restart=0',
                             '-o', 'tmp_path_retention_policy=failed', '--basetemp', str(tests)],
                            cwd=tmp_path, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    module.clean_pytest_current_links(tmp_path, tests)
    module.check_directory(tmp_path, tests)
    assert len(list(tests.glob('popen-gw*/compiler0/keep.txt'))) == 2


def setup_preview_repair(publisher, monkeypatch):
    import copy
    name='TimeSplitters-Future-Perfect-PC-v0.2.0-beta.3-windows-x64'
    old=dict(id=77,tag_name='v0.2.0-beta.3',target_commitish=PREVIOUS,
             draft=False,prerelease=True,immutable=False,published_at='2026-10-20',
             updated_at='2026-10-20',body='Original preview notes',
             html_url='https://github.com/owner/repo/releases/tag/v0.2.0-beta.3',
             assets=[dict(id=i,name=name+suffix,size=1,state='uploaded',digest='sha256:'+'c'*64)
                     for i,suffix in enumerate(('.zip','.zip.sha256','.BUILD.json'))])
    publisher.releases.append(old);publisher.replace_existing=True
    state=dict(release=old,tag=PREVIOUS,mutations=[])
    original_gh,original_git=publisher.gh,publisher.git
    def git(*args):
        if args[0]=='ls-remote':
            row=state['tag']+'\trefs/tags/v0.2.0-beta.3'
            return row if len(args)==3 else HEAD+'\trefs/heads/main\n'+row
        return original_git(*args)
    def gh(*args,payload=None):
        endpoint='repos/owner/repo/releases/77'
        if args==('api',endpoint):return json.dumps(state['release'])
        if args[:3]==('api','--method','PATCH'):
            state['mutations'].append(('patch',args[3]))
            if '/git/refs/' in args[3]:
                assert payload==dict(sha=HEAD,force=True)
                state['tag']=HEAD;return json.dumps(dict(object=dict(sha=HEAD)))
            assert args[3]==endpoint
            state['release'].update(payload);return json.dumps(state['release'])
        if args[:2]==('release','edit'):
            state['mutations'].append(('edit',args))
            assert '--latest=false' in args
            state['release']['draft']='--draft=true' in args
            return ''
        if args[:2]==('release','upload'):
            state['mutations'].append(('upload',args))
            assert '--clobber' in args
            state['release']['assets']=[dict(name=p.name,size=p.stat().st_size+(1 if publisher.bad_upload else 0),state='uploaded',digest='sha256:'+module.digest(p)) for p in (publisher.archive,publisher.checksum,publisher.metadata)]
            return ''
        return original_gh(*args,payload=payload)
    monkeypatch.setattr(publisher,'git',git);monkeypatch.setattr(publisher,'gh',gh)
    return state


def test_explicit_preview_repair_retags_and_verifies_assets_before_republication(publisher,monkeypatch):
    state=setup_preview_repair(publisher,monkeypatch)
    publisher.preflight();publisher.prepare();publisher.publish()
    assert state['tag']==HEAD and state['release']['target_commitish']==HEAD
    assert not state['release']['draft'] and state['release']['id']==77
    assert [kind for kind,_ in state['mutations']]==['edit','patch','patch','upload','edit']
    assert '--draft=true' in state['mutations'][0][1]
    assert '--draft=false' in state['mutations'][-1][1]
    assert not any(call[0]=='gh' and call[1][:2]==('release','create') for call in publisher.calls)


@pytest.mark.parametrize('blocked',['immutable','draft','stable','missing'])
def test_preview_repair_refuses_other_release_states_without_mutation(publisher,monkeypatch,blocked):
    state=setup_preview_repair(publisher,monkeypatch)
    if blocked=='missing':publisher.releases.pop()
    elif blocked=='stable':
        (publisher.root/'VERSION').write_text('0.2.0')
        state['release'].update(tag_name='v0.2.0',prerelease=False)
    else:state['release'][blocked]=True
    with pytest.raises(ValueError,match='Replacement requires'):publisher.preflight()
    assert not state['mutations'] and not publisher.builds


def test_preview_repair_detects_concurrent_edit_before_mutation(publisher,monkeypatch):
    state=setup_preview_repair(publisher,monkeypatch)
    publisher.preflight();publisher.prepare()
    state['release']['body']='Edited elsewhere'
    with pytest.raises(ValueError,match='changed during validation'):publisher.publish()
    assert not state['mutations'] and state['tag']==PREVIOUS


def test_preview_repair_keeps_bad_upload_hidden(publisher,monkeypatch):
    state=setup_preview_repair(publisher,monkeypatch)
    publisher.preflight();publisher.prepare();publisher.bad_upload=True
    with pytest.raises(ValueError,match='remains a draft'):publisher.publish()
    assert state['release']['draft']
    assert not any(kind=='edit' and '--draft=false' in args for kind,args in state['mutations'])


def test_prepared_repair_refuses_changed_release_on_resume(publisher,monkeypatch):
    state=setup_preview_repair(publisher,monkeypatch)
    publisher.preflight();publisher.prepare()
    state['release']['body']='Edited before resume'
    publisher.preflight()
    with pytest.raises(ValueError,match='changed since replacement preparation'):publisher.prepare()
    assert not state['mutations']


def test_preview_repair_refuses_ambiguous_source(publisher,monkeypatch):
    state=setup_preview_repair(publisher,monkeypatch)
    state['release']['target_commitish']='main'
    with pytest.raises(ValueError,match='source/tag'):publisher.preflight()
    assert not state['mutations']


def test_preview_repair_preserves_unexpected_assets(publisher,monkeypatch):
    state=setup_preview_repair(publisher,monkeypatch)
    state['release']['assets'].append(dict(id=99,name='keep-user-asset.txt',size=1,state='uploaded',digest='sha256:'+'d'*64))
    publisher.preflight();publisher.prepare()
    with pytest.raises(ValueError,match='unexpected assets'):publisher.publish()
    assert not state['mutations'] and state['tag']==PREVIOUS


def test_preview_repair_does_not_publish_if_tag_changes_during_upload(publisher,monkeypatch):
    state=setup_preview_repair(publisher,monkeypatch)
    gh=publisher.gh
    def changed(*args,payload=None):
        result=gh(*args,payload=payload)
        if args[:2]==('release','upload'):state['tag']=PREVIOUS
        return result
    monkeypatch.setattr(publisher,'gh',changed)
    publisher.preflight();publisher.prepare()
    with pytest.raises(ValueError,match='different commit'):publisher.publish()
    assert state['release']['draft']
