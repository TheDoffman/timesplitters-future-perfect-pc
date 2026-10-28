"""Offline checks for release choices and automatic report labels; no live mutations."""


import importlib.util
import re
from pathlib import Path
import shutil
import tempfile
import unittest
import urllib.error
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('issue_forms', ROOT / 'tools/ci/issue-forms.py')
forms = importlib.util.module_from_spec(spec)
spec.loader.exec_module(forms)


def release(tag, **changes):
    return dict(tag_name=tag, draft=False, published_at='2026-10-16T00:00:00Z', **changes)


def report(platform='Windows 11', enhancement=False, legacy=False):
    heading = 'What would you like to add or change?' if enhancement else 'What happened?'
    if enhancement and not legacy:
        return f'### {heading}\n\nAn idea\n'
    return (f'### Game version\n\nv0.2.0-alpha.4\n\n'
            f'### Launcher version\n\nv1.2.1\n\n'
            f'### Platform\n\n{platform}\n\n### {heading}\n\nDescription\n'
            + ('\n### How would this help?\n\nA reason\n' if enhancement else ''))


def http_error(code):
    return urllib.error.HTTPError('https://api.github.com/', code, 'Fixture error', {}, None)


class VersionPullRequestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.file = self.root / forms.FORMS[0]
        self.file.parent.mkdir(parents=True)
        self.file.write_text('generated reference data\n')
        self.base = 'a' * 40
        self.previous = None
        self.extra_files = []
        self.existing = []
        self.client = forms.GitHub('owner/repo')
        self.client.request = Mock(side_effect=self.response)

    def response(self, method, path, payload=None):
        if method == 'GET' and path == 'git/ref/heads/main':
            return {'object': {'sha': self.base}}
        if method == 'GET' and path == 'git/ref/heads/automation/issue-form-versions':
            if self.previous is None:
                raise http_error(404)
            return {'object': {'sha': self.previous}}
        if path.startswith('compare/'):
            return {'files': self.extra_files}
        if path.startswith('git/commits/'):
            return {'tree': {'sha': 'base-tree'}}
        if method == 'POST' and path == 'git/trees':
            return {'sha': 'updated-tree'}
        if method == 'POST' and path == 'git/commits':
            return {'sha': 'new-commit'}
        if method == 'GET' and path.startswith('pulls?'):
            return self.existing
        if method == 'POST' and path == 'pulls':
            return {'html_url': 'https://github.com/owner/repo/pull/7'}
        if method in ('POST', 'PATCH') and path.startswith('git/ref'):
            return {}
        self.fail(f'Unexpected request: {method} {path}')

    def publish(self, changed=None):
        return forms.publish_version_pr(self.client, self.root,
                                        [self.file] if changed is None else changed,
                                        'main', 'a' * 40)

    def test_current_references_make_no_requests(self):
        self.assertIsNone(self.publish([]))
        self.client.request.assert_not_called()

    def test_new_pr_uses_main_tree_and_only_generated_file(self):
        self.assertEqual(self.publish(), 'https://github.com/owner/repo/pull/7')
        calls = self.client.request.call_args_list
        tree = next(call.args[2] for call in calls if call.args[:2] == ('POST', 'git/trees'))
        self.assertEqual(tree['base_tree'], 'base-tree')
        self.assertEqual(tree['tree'], [dict(path=forms.FORMS[0], mode='100644',
                                           type='blob', content='generated reference data\n')])
        self.client.request.assert_any_call('POST', 'git/refs',
            {'ref': 'refs/heads/automation/issue-form-versions', 'sha': 'new-commit'})
        self.assertFalse(any(call.args[1].startswith('git/refs/heads/main') for call in calls))
        proposal = next(call.args[2] for call in calls if call.args[:2] == ('POST', 'pulls'))
        self.assertIn('New releases made the version examples out of date.', proposal['body'])
        self.assertEqual(re.findall(r'^## (.+)$', proposal['body'], re.M),
                         ['Change', 'Cause and fix'])
        self.assertNotIn('improvement form', proposal['body'])

    def test_existing_pr_preserves_both_ancestors_without_force_push(self):
        self.previous = 'b' * 40
        self.existing = [{'html_url': 'https://github.com/owner/repo/pull/5'}]
        self.extra_files = [{'filename': forms.FORMS[0]}]
        self.assertEqual(self.publish(), self.existing[0]['html_url'])
        self.client.request.assert_any_call('POST', 'git/commits', {
            'message': 'Refresh issue form release references', 'tree': 'updated-tree',
            'parents': [self.base, self.previous]})
        self.client.request.assert_any_call('PATCH', 'git/refs/heads/automation/issue-form-versions',
                                            {'sha': 'new-commit', 'force': False})
        self.assertFalse(any(call.args[:2] == ('POST', 'pulls')
                             for call in self.client.request.call_args_list))

    def test_unrelated_bot_branch_edits_are_preserved(self):
        self.previous = 'b' * 40
        self.extra_files = [{'filename': 'README.md'}]
        with self.assertRaisesRegex(ValueError, 'other edits'):
            self.publish()
        self.assertTrue(all(call.args[0] == 'GET' for call in self.client.request.call_args_list))

    def test_stale_checkout_cannot_publish(self):
        self.base = 'c' * 40
        with self.assertRaisesRegex(ValueError, 'advanced'):
            self.publish()
        self.assertEqual(self.client.request.call_count, 1)

    def test_unlisted_file_cannot_be_uploaded(self):
        with self.assertRaisesRegex(ValueError, 'Only generated'):
            self.publish([self.root / 'README.md'])
        self.client.request.assert_not_called()


class VersionTests(unittest.TestCase):
    def test_release_kinds_drafts_and_semantic_order(self):
        records = [release(tag) for tag in (
            'v1.0.0-alpha.2', 'v1.0.0-alpha.10', 'v1.0.0',
            'v2.0.0-beta.1', 'launcher-v1.10.0', 'launcher-v1.2.0',
            'launcher-v1.10.0', 'unrelated', 'v1.0.0-alpha.01', 'v01.0.0',
            'v2.0.0\nInjected heading')]
        records.extend([
            dict(release('v9.0.0'), draft=True),
            dict(release('launcher-v9.0.0'), published_at=None)])
        versions = forms.release_versions(records)
        self.assertEqual(versions['GAME'], [
            'v2.0.0-beta.1', 'v1.0.0', 'v1.0.0-alpha.10', 'v1.0.0-alpha.2'])
        self.assertEqual(versions['LAUNCHER'], ['v1.10.0', 'v1.2.0'])

    def test_options_are_bounded_and_keep_recent_versions(self):
        versions = forms.release_versions([release(f'v1.0.{i}') for i in range(40)])
        self.assertEqual(len(versions['GAME']), 30)
        self.assertEqual(versions['GAME'][0], 'v1.0.39')
        self.assertEqual(versions['GAME'][-1], 'v1.0.10')

    def test_pagination_does_not_miss_older_release_pages(self):
        client = forms.GitHub('owner/repo')
        client.request = Mock(side_effect=[[release('v1.0.0')] * 100,
                                           [release('launcher-v1.0.0')]])
        records = client.releases()
        self.assertEqual(len(records), 101)
        self.assertEqual(client.request.call_args_list[1].args,
                         ('GET', 'releases?per_page=100&page=2'))

    def test_api_failures_are_not_treated_as_an_empty_release_list(self):
        client = forms.GitHub('owner/repo')
        client.request = Mock(side_effect=http_error(403))
        with self.assertRaises(urllib.error.HTTPError):
            client.releases()
        client.request = Mock(return_value={'message': 'rate limited'})
        with self.assertRaises(ValueError):
            client.releases()

    def test_update_is_idempotent_and_preserves_questions_and_fallbacks(self):
        with tempfile.TemporaryDirectory(prefix='tsfp-issue-forms-test-') as directory:
            root = Path(directory)
            for relative in forms.FORMS:
                (root / relative).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, root / relative)
            records = [release('v3.0.0'), release('launcher-v4.0.0')]
            originals = [(root / p).read_bytes() for p in forms.FORMS]
            self.assertEqual(len(forms.refresh_versions(root, records, check=True)), 1)
            self.assertEqual(originals, [(root / p).read_bytes() for p in forms.FORMS])
            self.assertEqual(len(forms.refresh_versions(root, records)), 1)
            self.assertEqual(forms.refresh_versions(root, records), [])
            for relative in forms.FORMS:
                text = (root / relative).read_text()
                self.assertIn('- `v3.0.0`', text)
                self.assertIn('- `v4.0.0`', text)
                self.assertIn("I don't know", text)
                self.assertIn('Anything else? (optional)', text)
                for field in ('game-version', 'launcher-version', 'platform'):
                    section = text.split('    id: ' + field + '\n', 1)[1].split('\n  - type:', 1)[0]
                    self.assertIn('  - type: input\n    id: ' + field + '\n', text)
                    self.assertNotIn('default:', section)
                    self.assertNotIn('value:', section)
                    self.assertNotIn('options:', section)
                    self.assertIn('      required: true', section)
            before = (root / forms.FORMS[0]).read_bytes()
            for records in ([], [release('v5.0.0')], [release('launcher-v5.0.0')],
                            [dict(release('v9.0.0'), draft=True)]):
                for check in (False, True):
                    with self.subTest(records=records, check=check):
                        with self.assertRaisesRegex(ValueError, 'Refusing to erase'):
                            forms.refresh_versions(root, records, check=check)
                        self.assertEqual((root / forms.FORMS[0]).read_bytes(), before)
            self.assertIn('Not installed yet', (root / forms.FORMS[0]).read_text())

    def test_initial_empty_repository_is_allowed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / forms.FORMS[0]
            path.parent.mkdir(parents=True)
            text = (ROOT / forms.FORMS[0]).read_text(encoding='utf-8')
            text = re.sub(r'(?m)^        - `v[^`]+`\n', '', text)
            path.write_text(text, encoding='utf-8')
            forms.refresh_versions(root, [])
            self.assertEqual(path.read_text(encoding='utf-8').count('No published releases.'), 2)
            self.assertEqual(forms.refresh_versions(root, []), [])

    def test_bad_launcher_block_does_not_partially_update_game_versions(self):
        with tempfile.TemporaryDirectory(prefix='tsfp-issue-forms-test-') as directory:
            root = Path(directory)
            for relative in forms.FORMS:
                (root / relative).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, root / relative)
            first = root / forms.FORMS[0]
            first.write_text(first.read_text().replace('END AUTO LAUNCHER VERSIONS', 'missing marker'))
            before = first.read_bytes()
            with self.assertRaises(ValueError):
                forms.refresh_versions(root, [release('v3.0.0')])
            self.assertEqual(first.read_bytes(), before)

    def test_version_refresh_preserves_ideas_routing_without_recreating_enhancement_form(self):
        relative = '.github/ISSUE_TEMPLATE/config.yml'
        with tempfile.TemporaryDirectory(prefix='tsfp-issue-forms-test-') as directory:
            root = Path(directory)
            for name in (*forms.FORMS, relative):
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, root / name)
            path = root / relative
            before = path.read_bytes()
            self.assertIn('url: https://github.com/TheDoffman/timesplitters-future-perfect-pc/discussions/new?category=ideas', path.read_text())
            self.assertFalse((ROOT / '.github/ISSUE_TEMPLATE/02-enhancement.yml').exists())
            forms.refresh_versions(root, [release('v3.0.0'), release('launcher-v4.0.0')])
            self.assertEqual(path.read_bytes(), before)
            self.assertFalse((root / '.github/ISSUE_TEMPLATE/02-enhancement.yml').exists())


class LabelTests(unittest.TestCase):
    def test_typed_platforms_accept_case_and_os_version_details(self):
        for platform, label in [('windows 11 24H2', 'platform: windows'),
                                ('WINDOWS 10', 'platform: windows'),
                                ('win11', 'platform: windows'),
                                ('macOS 15.5', 'platform: macos'),
                                ('Mac OS', 'platform: macos')]:
            with self.subTest(platform=platform):
                self.assertEqual(forms.report_labels(report(platform)), {'bug', label})

    def test_platform_and_type_come_from_form_answers(self):
        for platform, label in [('Windows 10', 'platform: windows'),
                                ('Windows 11', 'platform: windows'),
                                ('macOS', 'platform: macos')]:
            with self.subTest(platform=platform):
                self.assertEqual(forms.report_labels(report(platform)), {'bug', label})
                self.assertEqual(forms.report_labels(report(platform, True, legacy=True)), {'enhancement', label})
        self.assertEqual(forms.report_labels(report('Other / not sure')), {'bug'})
        self.assertEqual(forms.report_labels(report().replace('\n', '\r\n')),
                         {'bug', 'platform: windows'})

    def test_bom_prefixed_report_is_recognized(self):
        for body in ('\ufeff' + report(), '\ufeff' + report().replace('\n', '\r\n')):
            self.assertEqual(forms.report_labels(body), {'bug', 'platform: windows'})

    def test_short_enhancement_gets_type_label_without_versions_or_platform(self):
        self.assertEqual(forms.report_labels(report(enhancement=True)), {'enhancement'})

    def test_manual_reports_and_pull_requests_are_untouched(self):
        client = Mock()
        self.assertFalse(forms.label_report(client, {'number': 1, 'body': 'Plain report'}))
        self.assertFalse(forms.label_report(client, {
            'number': 2, 'body': report(), 'pull_request': {}}))
        client.request.assert_not_called()

    def test_edits_replace_old_platform_but_preserve_manual_labels(self):
        client = Mock()
        issue = dict(number=5, body=report('macOS', True), labels=[{'name': name} for name in (
            'bug', 'platform: windows', 'major', 'accessibility')])
        self.assertTrue(forms.label_report(client, issue))
        mutations = [call.args for call in client.request.call_args_list if call.args[0] != 'GET']
        self.assertIn(('POST', 'issues/5/labels',
                       {'labels': ['enhancement']}), mutations)
        self.assertIn(('DELETE', 'issues/5/labels/bug'), mutations)
        self.assertIn(('DELETE', 'issues/5/labels/platform%3A%20windows'), mutations)
        self.assertFalse(any('major' in str(call) or 'accessibility' in str(call) for call in mutations))

    def test_missing_platform_label_is_created(self):
        client = Mock()
        client.request.side_effect = [http_error(404), {}, {}]
        forms.label_report(client, dict(number=7, body=report(), labels=[{'name': 'bug'}]))
        calls = client.request.call_args_list
        self.assertEqual(calls[1].args[0:2], ('POST', 'labels'))
        self.assertEqual(calls[1].args[2]['name'], 'platform: windows')
        self.assertEqual(calls[2].args, ('POST', 'issues/7/labels',
                                        {'labels': ['platform: windows']}))

    def test_concurrent_label_creation_is_rechecked(self):
        client = Mock()
        client.request.side_effect = [http_error(404), http_error(422), {}, {}]
        forms.label_report(client, dict(number=7, body=report(), labels=[{'name': 'bug'}]))
        self.assertEqual(client.request.call_args_list[2].args,
                         ('GET', 'labels/platform%3A%20windows'))

    def test_label_permission_error_is_reported_without_removing_labels(self):
        client = Mock()
        client.request.side_effect = http_error(403)
        with self.assertRaises(urllib.error.HTTPError):
            forms.label_report(client, dict(number=7, body=report('macOS'),
                                            labels=[{'name': 'bug'}, {'name': 'platform: windows'}]))
        self.assertEqual(client.request.call_count, 1)


class LauncherDiagnosticsForms(unittest.TestCase):
    def test_launcher_diagnostics_field_is_optional_in_bug_form(self):
        source = (ROOT / 'tools/launcher/DiagnosticIssueDraft.cs').read_text(encoding='utf-8')
        field = re.search(r'const string DiagnosticsField\s*=\s*"([a-z-]+)"', source).group(1)
        text = (ROOT / '.github/ISSUE_TEMPLATE/01-bug-report.yml').read_text(encoding='utf-8')
        ids = re.findall(r'^    id: (.+)$', text, re.M)
        self.assertEqual(ids.count(field), 1)
        section = text.split('    id: ' + field + '\n', 1)[1].split('\n  - type:', 1)[0]
        self.assertIn('label: Launcher diagnostics (optional)', section)
        self.assertIn('render: text', section)
        self.assertNotIn('required: true', section)


if __name__ == '__main__':
    unittest.main()
