"""Exercise dual-host publication, incomplete uploads and immutable release recovery offline."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.parse
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('launcher_release', ROOT/'tools/launcher-release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
SHA = 'a'*40


class FakeGitHub:
    def __init__(self):
        self.calls = []
        self.tag = None
        self.release = None
        self.assets = []
        self.fail_upload = None
        self.bad_digest = False
        self.latest = {'tag_name': 'launcher-v1.2.18'}
        self.history = [dict(self.latest, draft=False, prerelease=False)]

    def call(self, method, path, data=None, upload=None):
        self.calls.append((method, path, data))
        if '/git/ref/tags/' in path:
            return {'object': {'type': 'commit', 'sha': self.tag}} if self.tag else None
        if path.endswith('/git/refs'):
            self.tag = data['sha']
            return {}
        if '/releases/tags/' in path:
            return copy.deepcopy(self.release)
        if path.endswith('/releases/latest'):
            return self.latest
        if path.endswith('/releases/generate-notes'):
            return {'body': 'Changes'}
        if '/releases?per_page=100&page=' in path:
            page = int(path.rsplit('=', 1)[1])
            return self.history[(page-1)*100:page*100]
        if method == 'POST' and path.endswith('/releases'):
            assert data['draft'], 'The first visible release state must be a draft.'
            self.release = dict(data, id=1)
            return copy.deepcopy(self.release)
        if method == 'GET' and '/assets?' in path:
            return copy.deepcopy(self.assets)
        if upload:
            name = urllib.parse.parse_qs(urllib.parse.urlparse(path).query)['name'][0]
            if name == self.fail_upload:
                raise RuntimeError('Simulated interrupted upload')
            asset = {'name': name, 'id': len(self.calls), 'state': 'uploaded',
                     'size': upload.stat().st_size,
                     'digest': 'sha256:' + ('0'*64 if self.bad_digest else release.digest(upload))}
            self.assets.append(asset)
            return asset
        if method == 'DELETE':
            self.assets = [a for a in self.assets if a['id'] != int(path.rsplit('/', 1)[-1])]
            return None
        if method == 'PATCH':
            assert len(self.assets) == 8, 'Never publish a partial release.'
            self.release.update(data)
            return self.release
        if path.endswith('/dispatches'):
            assert not self.release['draft']
            return None
        raise AssertionError((method, path, data))


class LauncherReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.plan = dict(schema=1, version='1.2.19', tag='launcher-v1.2.19', source_revision=SHA,
                         repository=release.REPOSITORY, publish=True, prerelease=False, default_branch='main')
        self.api = FakeGitHub()
        self.git = patch.object(release, 'git', return_value=SHA).start()
        self.version = patch.object(release, 'source_version', return_value=self.plan['version']).start()
        self.addCleanup(patch.stopall)

    def test_plan_reads_unicode_github_event_under_windows_legacy_encoding(self):
        event_path = self.directory/'event.json'
        plan_path = self.directory/'plan.json'
        body = 'Fix \u201clinked paths\u201d for \u65e5\u672c\u8a9e users'
        event = {'pull_request': {'body': body}, 'repository': {'default_branch': 'main'}}
        event_path.write_bytes(json.dumps(event, ensure_ascii=False).encode('utf-8'))
        read_text = Path.read_text

        def legacy_read(path, *args, **kwargs):
            if path == event_path and not args and not kwargs.get('encoding'):
                kwargs['encoding'] = 'cp1252'
            return read_text(path, *args, **kwargs)

        env = dict(GITHUB_EVENT_PATH=str(event_path), GITHUB_EVENT_NAME='pull_request',
                   GITHUB_REF='refs/pull/214/merge', GITHUB_SHA=SHA,
                   GITHUB_REPOSITORY=release.REPOSITORY, LAUNCHER_VALIDATION_SHA=SHA)
        with patch.object(Path, 'read_text', legacy_read), \
                patch.dict(release.os.environ, env, clear=True), \
                patch('sys.argv', ['launcher-release.py', 'plan', '--plan', str(plan_path)]), \
                patch.object(release, 'plan_release', wraps=release.plan_release) as planner, \
                patch('builtins.print'):
            release.main()
        self.assertEqual(planner.call_args.args[0]['pull_request']['body'], body)
        plan = json.loads(plan_path.read_text(encoding='utf-8'))
        self.assertFalse(plan['publish'])
        self.assertEqual(plan['source_revision'], SHA)

    def package(self, platform, *, extra=None, executable=True, sealed=True):
        files = ({'TimeSplitters Launcher.exe': b'bootstrap', 'Bin/TimeSplitters.App.exe': b'launcher'}
                 if platform == 'windows-x64' else {
                     'TimeSplitters Launcher.app/Contents/Info.plist': b'plist',
                     'TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher': b'native',
                     'TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources': b'seal',
                     'TimeSplitters Launcher.app/Contents/Resources/Licenses/Avalonia.txt': b'license'})
        if not sealed:
            files.pop("TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources", None)
        if extra:
            files.update(extra)
        manifest = {'schema': 1, 'version': self.plan['version'],
                    'files': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}
        files['Support/Launcher/manifest.json'] = json.dumps(manifest).encode()
        archive = self.directory/release.package_names(self.plan['version'], platform)[0]
        with zipfile.ZipFile(archive, 'w') as package:
            for name, data in files.items():
                info = zipfile.ZipInfo(name)
                info.external_attr = (0o100755 if executable else 0o100644) << 16
                package.writestr(info, data)
        checksum = release.digest(archive)
        archive.with_suffix('.zip.sha256').write_text(checksum+'  '+archive.name+'\n')
        release.write_json(self.directory/release.manifest_name(platform),
                           dict(schema=1, version=self.plan['version'], asset_name=archive.name,
                                size=archive.stat().st_size, sha256=checksum))
        return archive

    def both(self):
        for platform in release.PLATFORMS:
            self.package(platform)
            release.stamp(self.directory, self.plan, platform)

    def test_complete_release_publishes_only_after_all_eight_uploads(self):
        self.both()
        url = release.publish(self.directory, self.plan, self.api)
        self.assertTrue(url.endswith('/launcher-v1.2.19'))
        self.assertFalse(self.api.release['draft'])
        self.assertEqual(self.api.release['make_latest'], 'true')
        self.assertEqual(self.api.calls[-1][1].split('/')[-2:], ['issue-form-versions.yml', 'dispatches'])
        self.assertEqual(self.api.tag, SHA)
        for platform in release.PLATFORMS:
            self.assertIn('/releases/download/launcher-v1.2.19/TimeSplitters-Launcher-v1.2.19-'+platform+'.zip)',
                          self.api.release['body'])
        self.assertEqual(sorted(a['name'] for a in self.api.assets if a['name'].endswith('.zip')),
                         ['TimeSplitters-Launcher-v1.2.19-macos-arm64.zip',
                          'TimeSplitters-Launcher-v1.2.19-windows-x64.zip'])
        for platform in release.PLATFORMS:
            metadata = json.loads((self.directory/release.manifest_name(platform)).read_text())
            self.assertEqual(metadata['asset_name'], release.package_names(self.plan['version'], platform)[0])

    def test_missing_platform_does_not_even_contact_github(self):
        self.package('windows-x64')
        release.stamp(self.directory, self.plan, 'windows-x64')
        with self.assertRaises(FileNotFoundError):
            release.publish(self.directory, self.plan, self.api)
        self.assertEqual(self.api.calls, [])

    def test_notes_use_previous_stable_launcher_across_paginated_mixed_history(self):
        self.api.history = [{'tag_name': 'v0.3.1-alpha.2'}]*100 + [
            {'tag_name': 'launcher-v1.2.17'},
            {'tag_name': 'launcher-v1.2.18'},
            {'tag_name': 'launcher-v1.2.19'},  # Current release is not its own baseline.
            {'tag_name': 'launcher-v1.2.20'},  # Nor is a newer version.
            {'tag_name': 'launcher-v1.2.19-beta.1', 'prerelease': True},
            {'tag_name': 'launcher-v1.2.19-beta.2'},
            {'tag_name': 'launcher-v1.2.18', 'draft': True}]
        self.both()
        release.publish(self.directory, self.plan, self.api)
        notes = [call[2] for call in self.api.calls if call[1].endswith('/generate-notes')]
        self.assertEqual(notes, [dict(tag_name=self.plan['tag'], target_commitish=SHA,
                                     previous_tag_name='launcher-v1.2.18')])

    def test_initial_launcher_notes_never_use_game_baseline(self):
        self.api.history = [{'tag_name': 'v0.3.1-alpha.2'},
                            {'tag_name': 'launcher-v1.2.18', 'draft': True}]
        self.assertEqual(release.launcher_notes(self.api, '/repos/test', self.plan['tag'], SHA),
                         'Initial launcher release.')
        self.assertFalse(any(call[1].endswith('/generate-notes') for call in self.api.calls))

    def test_launcher_notes_filter_game_docs_housekeeping_and_out_of_range_prs(self):
        base = '/repos/'+release.REPOSITORY
        paths = {1: ['src/game.c'], 2: ['tools/launcher/Core.cs'],
                 3: ['docs/launcher.md'], 4: ['tools/issue-forms.py'],
                 5: ['src/input.c', 'tools/launcher/GameSettings.cs'],
                 6: ['tools/build-mac-launcher.py'], 7: ['tools/launcher/Core.cs']}
        def call(method, path):
            number = int(path.split('/pulls/')[1].split('/')[0])
            if '/files?' in path:
                return [{'filename': name} for name in paths[number]]
            return dict(merged_at='2026-10-23', merge_commit_sha='b'*40 if number == 7 else SHA,
                        title=f'Change {number}', changed_files=len(paths[number]))
        self.api.call = call
        generated = '\n'.join(f'https://github.com/{release.REPOSITORY}/pull/{n}' for n in paths)
        notes = release.launcher_pr_notes(self.api, base, generated, 'launcher-v1.2.18',
                                          'launcher-v1.2.19', SHA)
        for number in (2, 6):
            self.assertIn(f'Change {number}', notes)
        self.assertIn(f'Launcher updates ([#5](https://github.com/{release.REPOSITORY}/pull/5))', notes)
        self.assertNotIn('Change 5', notes)
        for number in (1, 3, 4, 7):
            self.assertNotIn(f'Change {number}', notes)
        self.assertIn('Full source comparison', notes)

    def test_launcher_notes_check_renamed_paths_and_paginate_files(self):
        def call(method, path):
            if '/files?' not in path:
                return dict(merged_at='2026-10-23', merge_commit_sha=SHA,
                            title='Move launcher file', changed_files=101)
            if path.endswith('page=1'):
                return [{'filename': 'docs/usage.md'}]*100
            return [{'filename': 'archive/old.cs', 'previous_filename': 'tools/launcher/Old.cs'}]
        self.api.call = call
        notes = release.launcher_pr_notes(self.api, '/repos/'+release.REPOSITORY,
                    f'https://github.com/{release.REPOSITORY}/pull/1', 'launcher-v1.2.18', self.plan['tag'], SHA)
        self.assertIn('Move launcher file', notes)

    def test_launcher_notes_refuse_incomplete_pr_files(self):
        self.api.call = lambda method, path: ([] if '/files?' in path else dict(
            merged_at='2026-10-23', merge_commit_sha=SHA, title='Change', changed_files=1))
        with self.assertRaisesRegex(ValueError, 'Incomplete changed-file'):
            release.launcher_pr_notes(self.api, '/repos/'+release.REPOSITORY,
                f'https://github.com/{release.REPOSITORY}/pull/1', 'launcher-v1.2.18', self.plan['tag'], SHA)

    def test_incomplete_notes_history_is_rejected(self):
        self.api.history = [{'tag_name': 'v0.3.1-alpha.2'}]*2000
        with self.assertRaisesRegex(ValueError, 'history is incomplete'):
            release.launcher_notes(self.api, '/repos/test', self.plan['tag'], SHA)

    def test_stale_aliases_and_old_archives_are_not_published(self):
        self.both()
        for platform in release.PLATFORMS:
            alias = self.directory/f'TimeSplitters-Launcher-{platform}.zip'
            self.assertFalse(alias.exists(), 'Stamp must not create a second ZIP.')
            alias.write_bytes(b'stale alias from an earlier build')
            alias.with_suffix('.zip.sha256').write_text('old checksum')
            (self.directory/f'TimeSplitters-Launcher-v1.2.18-{platform}.zip').write_bytes(b'old package')
        release.publish(self.directory, self.plan, self.api)
        self.assertEqual(len(self.api.assets), 8)
        self.assertFalse(any('v1.2.18' in a['name'] for a in self.api.assets))
        self.assertFalse(any(a['name'].startswith('TimeSplitters-Launcher-'+platform)
                             for a in self.api.assets for platform in release.PLATFORMS))

    def test_mixed_revision_or_corrupt_checksum_prevents_remote_mutation(self):
        self.both()
        receipt = self.directory/'launcher-build-macos-arm64.json'
        value = json.loads(receipt.read_text())
        value['source_revision'] = 'b'*40
        release.write_json(receipt, value)
        with self.assertRaisesRegex(ValueError, 'same tested source'):
            release.publish(self.directory, self.plan, self.api)
        self.assertEqual(self.api.calls, [])
        release.stamp(self.directory, self.plan, 'macos-arm64')
        (self.directory/'TimeSplitters-Launcher-v1.2.19-windows-x64.zip.sha256').write_text('bad')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            release.publish(self.directory, self.plan, self.api)
        self.assertEqual(self.api.calls, [])

    def test_wrong_builder_revision_is_rejected(self):
        self.git.return_value = 'b'*40
        with self.assertRaisesRegex(ValueError, 'planned source'):
            release.stamp(self.directory, self.plan, 'macos-arm64')

    def test_corrupt_metadata_and_payload_are_rejected(self):
        archive = self.package('windows-x64')
        archive.write_bytes(archive.read_bytes()+b'corrupt')
        with self.assertRaisesRegex(ValueError, 'manifest mismatch'):
            release.verify_package(self.directory, self.plan, 'windows-x64')

    def test_unsafe_unexpected_and_case_collision_payloads_are_rejected(self):
        for path in ('../escape.exe', '/absolute.exe', 'Bin\\escape.exe', 'Bin/extra.exe', 'bin/timesplitters.app.exe'):
            with self.subTest(path=path):
                self.package('windows-x64', extra={path: b'bad'})
                with self.assertRaises(ValueError):
                    release.verify_package(self.directory, self.plan, 'windows-x64')

    def test_mac_bundle_seal_is_required_and_only_canonical_signature_path_is_allowed(self):
        self.package('macos-arm64', sealed=False)
        with self.assertRaisesRegex(ValueError, 'Missing launcher entry point'):
            release.verify_package(self.directory, self.plan, 'macos-arm64')
        self.package('macos-arm64', extra={
            'TimeSplitters Launcher.app/Contents/_CodeSignature/unexpected': b'bad'})
        with self.assertRaisesRegex(ValueError, 'Unexpected launcher payload'):
            release.verify_package(self.directory, self.plan, 'macos-arm64')

    def test_mac_signing_seals_nested_code_before_bundle_and_requires_strict_verification(self):
        spec = importlib.util.spec_from_file_location('mac_builder', ROOT/'tools/build-mac-launcher.py')
        builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(builder)
        app = self.directory/'TimeSplitters Launcher.app'
        libs = app/'Contents/MacOS'
        libs.mkdir(parents=True)
        (libs/'native.dylib').write_bytes(b'fixture')
        with patch.object(builder.subprocess, 'run') as run:
            builder.sign_bundle(app)
        calls = [c.args[0] for c in run.call_args_list]
        self.assertEqual(calls[0], ['/usr/bin/xattr', '-cr', str(app)])
        self.assertEqual(calls[1][-1], str(libs/'native.dylib'))
        self.assertEqual(calls[2][-1], str(app))
        self.assertIn('--sign', calls[2])
        self.assertEqual(calls[3], ['/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=2', str(app)])
        self.assertTrue(all(c.kwargs['check'] for c in run.call_args_list))

    def test_mac_executable_mode_is_required(self):
        self.package('macos-arm64', executable=False)
        with self.assertRaisesRegex(ValueError, 'permissions'):
            release.verify_package(self.directory, self.plan, 'macos-arm64')

    def test_failed_upload_keeps_draft_and_retry_completes_it(self):
        self.both()
        self.api.fail_upload = 'launcher-update-macos-arm64.json'
        with self.assertRaisesRegex(RuntimeError, 'interrupted'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(self.api.release['draft'])
        self.assertFalse(any(c[0] == 'PATCH' for c in self.api.calls))
        self.api.fail_upload = None
        release.publish(self.directory, self.plan, self.api)
        self.assertFalse(self.api.release['draft'])
        self.assertEqual(len(self.api.assets), 8)
        self.assertEqual(sum(c[0] == 'POST' and c[1].endswith('/releases') for c in self.api.calls), 1)

    def test_digest_mismatch_keeps_draft_then_retry_repairs_owned_assets(self):
        self.both()
        self.api.bad_digest = True
        with self.assertRaisesRegex(ValueError, 'stays a draft'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(self.api.release['draft'])
        self.api.bad_digest = False
        release.publish(self.directory, self.plan, self.api)
        self.assertFalse(self.api.release['draft'])

    def test_published_release_can_retry_refresh_but_cannot_replace_assets(self):
        self.both()
        release.publish(self.directory, self.plan, self.api)
        self.api.calls.clear()
        release.publish(self.directory, self.plan, self.api)
        self.assertEqual([c[0] for c in self.api.calls if c[0] != 'GET'], ['POST'])
        self.api.assets[0]['digest'] = 'sha256:'+'0'*64
        self.api.calls.clear()
        with self.assertRaisesRegex(ValueError, 'immutable'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(all(c[0] == 'GET' for c in self.api.calls))

    def test_retry_preserves_unrelated_published_assets(self):
        self.both()
        release.publish(self.directory, self.plan, self.api)
        self.api.assets.append({'name': 'unrelated.zip'})
        self.api.calls.clear()
        with self.assertRaisesRegex(ValueError, 'unrelated assets'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(all(c[0] == 'GET' for c in self.api.calls))
        self.assertEqual(len(self.api.assets), 9)

    def test_missing_published_asset_cannot_be_replaced(self):
        self.both()
        release.publish(self.directory, self.plan, self.api)
        self.api.assets.pop()
        self.api.calls.clear()
        with self.assertRaisesRegex(ValueError, 'immutable'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(all(c[0] == 'GET' for c in self.api.calls))

    def test_failed_mac_upload_keeps_draft_until_retry(self):
        self.both()
        self.api.fail_upload = 'TimeSplitters-Launcher-v1.2.19-macos-arm64.zip'
        with self.assertRaisesRegex(RuntimeError, 'interrupted'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(self.api.release['draft'])
        self.assertFalse(any(c[0] == 'PATCH' for c in self.api.calls))
        self.api.fail_upload = None
        release.publish(self.directory, self.plan, self.api)
        self.assertFalse(self.api.release['draft'])
        self.assertEqual(len(self.api.assets), 8)

    def test_existing_unowned_release_or_wrong_tag_is_not_modified(self):
        self.both()
        self.api.tag = 'b'*40
        with self.assertRaisesRegex(ValueError, 'different commit'):
            release.publish(self.directory, self.plan, self.api)
        self.api.tag = SHA
        self.api.release = {'body': 'Maintainer release', 'target_commitish': SHA}
        with self.assertRaisesRegex(ValueError, 'not be overwritten'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(all(c[0] == 'GET' for c in self.api.calls))

    def test_older_stable_cannot_replace_latest(self):
        self.both()
        self.api.latest = {'tag_name': 'launcher-v1.2.20'}
        with self.assertRaisesRegex(ValueError, 'newer'):
            release.publish(self.directory, self.plan, self.api)
        self.assertTrue(all(c[0] == 'GET' for c in self.api.calls))

    def test_preview_does_not_replace_latest(self):
        self.plan.update(version='1.2.19-rc.1', tag='launcher-v1.2.19-rc.1', prerelease=True)
        self.version.return_value = self.plan['version']
        self.both()
        release.publish(self.directory, self.plan, self.api)
        self.assertTrue(self.api.release['prerelease'])
        self.assertEqual(self.api.release['make_latest'], 'false')

    def test_validation_only_is_read_only(self):
        self.plan['publish'] = False
        with self.assertRaisesRegex(ValueError, 'validation-only'):
            release.publish(self.directory, self.plan, self.api)
        self.assertEqual(self.api.calls, [])

    def test_plan_reads_utf8_github_event_with_windows_default_encoding(self):
        event = self.directory/'event.json'
        plan_path = self.directory/'plan.json'
        event.write_text(json.dumps({
            'repository': {'default_branch': 'r\u00e9vision'},
            'pull_request': {'body': 'Fix the \u201cUPDATE CHANNEL\u201d overlap.'},
        }, ensure_ascii=False), encoding='utf-8')
        read_text = Path.read_text

        def windows_read_text(path, encoding=None, **kwargs):
            return read_text(path, encoding=encoding or 'cp1252', **kwargs)

        environment = dict(GITHUB_EVENT_PATH=str(event), GITHUB_EVENT_NAME='pull_request',
                           GITHUB_REF='refs/pull/215/merge', GITHUB_SHA=SHA,
                           GITHUB_REPOSITORY=release.REPOSITORY, LAUNCHER_VALIDATION_SHA=SHA)
        with patch.dict(release.os.environ, environment, clear=True), \
                patch('sys.argv', ['launcher-release.py', 'plan', '--plan', str(plan_path)]), \
                patch.object(Path, 'read_text', windows_read_text), patch('builtins.print'):
            release.main()
        plan = json.loads(plan_path.read_text(encoding='utf-8'))
        self.assertEqual(plan['default_branch'], 'r\u00e9vision')
        self.assertEqual(plan['source_revision'], SHA)
        self.assertFalse(plan['publish'])

    @patch.object(release.subprocess, 'run')
    def test_publish_event_policy_checks_main_ancestry_and_matching_tag(self, run):
        event = {'repository': {'default_branch': 'main'}, 'inputs': {'publish': 'true'}}
        for kind, ref in [('push', 'refs/tags/launcher-v1.2.19'), ('workflow_dispatch', 'refs/heads/main')]:
            with self.subTest(kind=kind):
                plan = release.plan_release(event, kind, ref, SHA, release.REPOSITORY)
                self.assertTrue(plan['publish'])
                self.assertEqual(run.call_args.args[0], ['git', 'merge-base', '--is-ancestor', SHA, 'FETCH_HEAD'])
        for kind, ref, repository in [('push', 'refs/tags/launcher-v9.9.9', release.REPOSITORY),
                                     ('workflow_dispatch', 'refs/heads/topic', release.REPOSITORY),
                                     ('push', 'refs/tags/launcher-v1.2.19', 'fork/repository')]:
            with self.subTest(ref=ref, repository=repository), self.assertRaises(ValueError):
                release.plan_release(event, kind, ref, SHA, repository)
        self.assertFalse(release.plan_release(event, 'pull_request', 'refs/pull/1/merge', SHA, release.REPOSITORY)['publish'])
        event['inputs']['publish'] = 'false'
        self.assertFalse(release.plan_release(event, 'workflow_dispatch', 'refs/heads/topic', SHA, release.REPOSITORY)['publish'])

    def test_workflow_keeps_write_token_after_both_host_builds_and_verification(self):
        workflow = (ROOT/'.github/workflows/launcher-release.yml').read_text()
        build, publish = workflow.split('\n  publish:\n')
        self.assertNotIn(': write', build)
        self.assertNotIn('secrets.GITHUB_TOKEN', build)
        self.assertIn('needs: build', publish)
        shared = (ROOT/'.github/workflows/launcher-build.yml').read_text()
        self.assertIn('needs: [windows, mac]', shared)
        self.assertNotIn(': write', shared)
        self.assertIn("github.event_name == 'push' || github.event_name == 'workflow_dispatch'", publish)
        self.assertIn('cancel-in-progress: false', workflow)
        self.assertIn('persist-credentials: false', publish)
        self.assertIn('--verify-package', shared)
        self.assertIn('-Test -RequireArtwork', shared)
        self.assertIn('--install-sdk --test --require-artwork', shared)
        self.assertIn("tags: ['launcher-v*']", workflow)
        self.assertIn('default: false', workflow)
        self.assertNotIn('TimeSplitters-Launcher-*.zip*', shared)
        self.assertIn('TimeSplitters-Launcher-v${{ steps.plan.outputs.version }}-windows-x64.zip', shared)
        self.assertIn('TimeSplitters-Launcher-v${{ needs.windows.outputs.version }}-macos-arm64.zip', shared)

    def test_publication_checkout_includes_changelog_baseline_and_commit_history(self):
        workflow = (ROOT/'.github/workflows/launcher-release.yml').read_text()
        publish = workflow.split('\n  publish:\n', 1)[1]
        checkout = publish.split('      - uses: actions/checkout@', 1)[1].split('      - uses:', 1)[0]
        self.assertIn('fetch-depth: 0', checkout,
                      'Launcher changelogs need prior release tags and the complete commit range.')

    def test_readme_download_links_do_not_depend_on_removed_aliases(self):
        readme = (ROOT/'README.md').read_text(encoding='utf-8')
        self.assertIn('href="https://github.com/'+release.REPOSITORY+'/releases/latest"', readme)
        self.assertNotIn('/releases/latest/download/TimeSplitters-Launcher-', readme)


if __name__ == '__main__':
    unittest.main()
