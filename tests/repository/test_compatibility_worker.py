"""Exercise the publication boundary without credentials or model calls."""


import copy
from contextlib import nullcontext
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('compat_worker', ROOT / 'tools/ci/compatibility-worker.py')
W = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(W)
HEAD = 'a' * 40


def pull(head=HEAD):
    return {'number': 42, 'state': 'open', 'draft': False, 'user': {'login': W.OWNER},
            'base': {'ref': 'main'}, 'head': {'sha': head, 'ref': 'feature',
                                          'repo': {'full_name': W.REPO}}}


def runs(head=HEAD):
    return [{'id': i + 1, 'path': path, 'name': path, 'head_sha': head,
             'event': 'pull_request', 'run_number': 5, 'run_attempt': 1,
             'status': 'completed', 'conclusion': 'failure' if i == 0 else 'success',
             'html_url': 'https://github.com/test/actions/runs/' + str(i + 1)}
            for i, path in enumerate(sorted(W.WORKFLOWS))]


class PolicyTests(unittest.TestCase):
    def test_filters_untrusted_or_ineligible_pull_requests(self):
        self.assertTrue(W.eligible(pull()))
        variants = []
        for field, value in [('state', 'closed'), ('draft', True), ('user', {'login': 'someone'})]:
            pr = pull()
            pr[field] = value
            variants.append(pr)
        pr = pull(); pr['head']['repo']['full_name'] = 'someone/fork'; variants.append(pr)
        pr = pull(); pr['head']['repo'] = None; variants.append(pr)
        pr = pull(); pr['base']['ref'] = 'another-branch'; variants.append(pr)
        pr = pull(); pr['head']['ref'] = 'codex/compat-repair-42-abcdef'; variants.append(pr)
        for pr in variants:
            with self.subTest(pr=pr):
                self.assertFalse(W.eligible(pr))

    def test_waits_for_complete_current_head_ci_and_latest_attempt(self):
        self.assertEqual(len(W.failed_runs(runs(), HEAD)), 1)
        self.assertIsNone(W.failed_runs(runs()[:1], HEAD))
        self.assertIsNone(W.failed_runs(runs('b' * 40), HEAD))
        for status in ['queued', 'in_progress']:
            r = runs(); r[1]['status'] = status
            self.assertIsNone(W.failed_runs(r, HEAD))
        for conclusion in ['cancelled', 'skipped', 'neutral', None]:
            r = runs(); r[1]['conclusion'] = conclusion
            self.assertIsNone(W.failed_runs(r, HEAD))
        r = runs(); rerun = {**r[0], 'run_attempt': 2, 'status': 'in_progress'}
        self.assertIsNone(W.failed_runs(r + [rerun], HEAD))
        rerun.update(status='completed', conclusion='success')
        self.assertEqual(W.failed_runs([rerun] + r, HEAD), [])

    def test_no_api_environment_or_agent_escalation(self):
        with mock.patch.dict(os.environ, {'OPENAI_API_KEY': 'unused', 'GH_TOKEN': 'unused',
                                         'CODEX_API_KEY': 'unused', 'GIT_SSH_COMMAND': 'bad'}):
            env = W.clean_env()
        for name in ['OPENAI_API_KEY', 'CODEX_API_KEY', 'GH_TOKEN', 'GIT_SSH_COMMAND']:
            self.assertNotIn(name, env)
        args = W.codex_args('codex', Path('/source'), Path('/source/result'))
        self.assertIn('forced_login_method="chatgpt"', args)
        self.assertIn('approval_policy="never"', args)
        self.assertIn('sandbox_workspace_write.network_access=false', args)
        self.assertIn('features.apps=false', args)
        self.assertIn('features.plugins=false', args)
        self.assertNotIn('--dangerously-bypass-approvals-and-sandbox', args)

    def test_regular_source_only_publication(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'src').mkdir()
            source = root / 'src/test.c'
            source.write_text('old', encoding='utf-8')
            original = {'src/test.c': (hashlib.sha256(b'old').hexdigest(), '100644')}
            self.assertEqual(W.read_changes(root, original), {})
            source.write_text('new', encoding='utf-8')
            self.assertEqual(W.read_changes(root, original)['src/test.c'][0], b'new')
            (root / 'AGENTS.md').write_text('override', encoding='utf-8')
            with self.assertRaises(W.WorkerError):
                W.read_changes(root, original)
            (root / 'AGENTS.md').unlink()
            source.unlink()
            self.assertIsNone(W.read_changes(root, original)['src/test.c'])
        for path in ['.github/workflows/test.yml', 'AGENTS.md', 'private/saves/foo.c',
                     'tools/ci/compatibility-worker.py', 'tools/../secret.py', 'tools/a\nb.py',
                     'config/pc-controls.json', '/tmp/file.py']:
            self.assertFalse(W.allowed_change(path), path)

    @unittest.skipIf(os.name == 'nt', 'Windows symlink privileges are optional')
    def test_symlink_cannot_publish_outside_snapshot(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'tools').mkdir()
            (root / 'tools/bad.py').symlink_to(Path(__file__))
            with self.assertRaisesRegex(W.WorkerError, 'symlink'):
                W.read_changes(root, {})

    def test_command_output_and_duration_are_bounded(self):
        with self.assertRaisesRegex(W.WorkerError, 'output limit'):
            W.command([sys.executable, '-c', 'print("x"*100000)'], limit=100)
        with self.assertRaisesRegex(W.WorkerError, 'timed out'):
            W.command([sys.executable, '-c', 'import time; time.sleep(10)'], timeout=0.1)
        self.assertEqual(W.command([sys.executable, '-c', 'import sys; print(sys.stdin.read())'],
                                   data=b'bounded input').strip(), b'bounded input')

    @unittest.skipIf(os.name == 'nt', 'LaunchAgent lock is macOS/POSIX only')
    def test_overlapping_runs_do_not_acquire_the_lock(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'lock'
            with W.exclusive_lock(path):
                with self.assertRaisesRegex(W.WorkerError, 'active'):
                    with W.exclusive_lock(path):
                        pass
            with W.exclusive_lock(path):
                pass

    def test_plist_is_portable_and_uses_only_requested_mac_paths(self):
        root = Path('/Users/another-user/Library/Application Support/TSFP Compatibility Worker')
        plist = W.launch_agent(root, '/usr/bin/python3')
        self.assertEqual(plist['StartInterval'], 900)
        self.assertEqual(plist['ProgramArguments'][1], str(root / 'worker.py'))
        self.assertNotIn('KeepAlive', plist)
        self.assertNotIn('OPENAI_API_KEY', json.dumps(plist))


class UpgradeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plist = self.root / 'worker.plist'
        self.plist.write_bytes(b'existing dashboard wrapper')
        (self.root / 'worker.py').write_bytes(b'old controller')
        W.save_json(self.root / 'config.json', {'gh': 'gh', 'codex': 'codex'})
        W.save_json(self.root / 'state.json', {'paused': 'operator', 'attempts': {'old': 'retained'}})
        self.config = {'gh': 'gh', 'codex': 'codex', 'auto_merge': True}

    def test_fresh_install_requires_new_auto_merge_opt_in(self):
        for opted_in in (False, True):
            with self.subTest(opted_in=opted_in), tempfile.TemporaryDirectory() as tmp:
                home = Path(tmp)
                root = home / 'worker'; root.mkdir()
                W.save_json(root / 'config.json', self.config)
                argv = ['worker', '--state-dir', str(root)]
                if opted_in:
                    argv.append('--auto-merge')
                argv.append('install')
                with mock.patch.object(W.sys, 'argv', argv), \
                     mock.patch.object(W.sys, 'platform', 'darwin'), \
                     mock.patch.object(W.Path, 'home', return_value=home), \
                     mock.patch.object(W.os, 'getuid', return_value=501, create=True), \
                     mock.patch.object(W, 'exclusive_lock', return_value=nullcontext()), \
                     mock.patch.object(W, 'find_program', side_effect=lambda name, supplied: supplied), \
                     mock.patch.object(W, 'command'), \
                     mock.patch.object(W.Worker, 'preflight'), \
                     mock.patch.object(W.Worker, 'protected_main') as protection:
                    W.main()
                self.assertEqual(W.load_json(root / 'config.json', {})['auto_merge'], opted_in)
                self.assertEqual(protection.call_count, int(opted_in))

    def test_upgrade_retains_wrapper_state_and_rollback_files(self):
        state = (self.root / 'state.json').read_bytes()
        settings = (self.root / 'config.json').read_bytes()
        W.upgrade_installation(self.root, self.plist, self.config)
        self.assertEqual((self.root / 'worker.py').read_bytes(), Path(W.__file__).read_bytes())
        self.assertEqual(W.load_json(self.root / 'config.json', {}), self.config)
        self.assertEqual((self.root / 'worker.py.previous').read_bytes(), b'old controller')
        self.assertEqual((self.root / 'config.json.previous').read_bytes(), settings)
        self.assertEqual((self.root / 'state.json').read_bytes(), state)
        self.assertEqual(self.plist.read_bytes(), b'existing dashboard wrapper')
        self.assertEqual(list(self.root.glob('worker-upgrade-*')), [])

    def test_failed_config_write_restores_installed_controller(self):
        settings = (self.root / 'config.json').read_bytes()
        with mock.patch.object(W, 'save_json', side_effect=OSError('disk write failed')):
            with self.assertRaisesRegex(OSError, 'disk write failed'):
                W.upgrade_installation(self.root, self.plist, self.config)
        self.assertEqual((self.root / 'worker.py').read_bytes(), b'old controller')
        self.assertEqual((self.root / 'config.json').read_bytes(), settings)
        self.assertEqual(list(self.root.glob('worker-upgrade-*')), [])

    def test_missing_installation_is_not_created(self):
        self.plist.unlink()
        with self.assertRaisesRegex(W.WorkerError, 'not installed'):
            W.upgrade_installation(self.root, self.plist, self.config)
        self.assertEqual((self.root / 'worker.py').read_bytes(), b'old controller')

    @unittest.skipIf(os.name == 'nt', 'Windows symlink privileges are optional')
    def test_symlink_backup_is_rejected_before_writes(self):
        (self.root / 'worker.py.previous').symlink_to(self.plist)
        with self.assertRaisesRegex(W.WorkerError, 'symlink'):
            W.upgrade_installation(self.root, self.plist, self.config)
        self.assertEqual((self.root / 'worker.py').read_bytes(), b'old controller')
        self.assertEqual(self.plist.read_bytes(), b'existing dashboard wrapper')


class RepairTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.work = self.root / 'worker'; self.work.mkdir()
        self.repo = self.root / 'source'; self.repo.mkdir()
        self.git('init', '-q', str(self.repo))
        self.git('-C', str(self.repo), 'config', 'user.name', 'Test')
        self.git('-C', str(self.repo), 'config', 'user.email', 'test@example.invalid')
        (self.repo / 'src').mkdir()
        (self.repo / 'src/fix.c').write_text('old\n', encoding='utf-8')
        (self.repo / 'AGENTS.md').write_text('Keep shared logic shared.\n', encoding='utf-8')
        self.git('-C', str(self.repo), 'add', '.')
        self.git('-C', str(self.repo), 'commit', '-qm', 'fixture')
        self.head = self.git('-C', str(self.repo), 'rev-parse', 'HEAD').strip()
        self.pr = pull(self.head)
        self.worker = W.Worker(self.work, {'gh': 'fake-gh', 'codex': 'fake-codex'})
        self.worker.preflight = mock.Mock()
        self.worker.pages = mock.Mock(side_effect=lambda route, key=None:
                                     runs(self.head) if key else [copy.deepcopy(self.pr)])
        self.published = []
        self.live = True

        def api(route, payload=None):
            if payload:
                self.published.append(payload)
                return {'number': 43, 'html_url': 'https://github.com/test/pull/43'}
            if '/pulls?state=all' in route:
                return []
            return copy.deepcopy(self.pr) if self.live else pull('b' * 40)
        self.worker.api = mock.Mock(side_effect=api)
        real = W.command

        def run(argv, **kwargs):
            if argv[0] == 'fake-gh':
                return b'Windows/Mac test failed in src/fix.c'
            if argv[0] == 'fake-codex':
                snapshot = Path(argv[argv.index('-C') + 1])
                # Model output is a byte-preserving edit, not the test host's
                # implicit text newline translation (CRLF on Windows).
                (snapshot / 'src/fix.c').write_bytes(b'fixed\n')
                Path(argv[argv.index('-o') + 1]).write_text(
                    'Shared adapter corrected. Portable fixture passed; Windows CI pending.', encoding='utf-8')
                return b'model result'
            return real(argv, **kwargs)
        self.runner = mock.Mock(side_effect=run)
        self.patch_remote = mock.patch.object(W, 'REMOTE', str(self.repo))
        self.patch_remote.start(); self.addCleanup(self.patch_remote.stop)
        self.patch_command = mock.patch.object(W, 'command', self.runner)
        self.patch_command.start(); self.addCleanup(self.patch_command.stop)

    @staticmethod
    def git(*args):
        return subprocess.check_output(['git', *args], stderr=subprocess.STDOUT).decode()

    def test_end_to_end_repair_preserves_original_and_deduplicates(self):
        result = self.worker.poll()
        self.assertEqual(result['status'], 'published_awaiting_ci')
        self.assertEqual(self.git('-C', str(self.repo), 'rev-parse', 'HEAD').strip(), self.head)
        branch = 'codex/compat-repair-42-' + self.head[:12]
        self.assertEqual(self.git('-C', str(self.repo), 'show', branch + ':src/fix.c'), 'fixed\n')
        self.assertEqual(self.published[0]['base'], 'feature')
        self.assertNotEqual(self.published[0]['base'], 'main')
        self.assertEqual(self.worker.poll()['status'], 'idle')
        self.assertEqual(len([c for c in self.runner.call_args_list if c.args[0][0] == 'fake-codex']), 1)
        self.assertFalse(list(self.work.glob('repair-*')))

    def test_dry_run_never_invokes_model_or_writes_state(self):
        self.assertEqual(self.worker.poll(dry_run=True)['actionable_prs'], [42])
        self.runner.assert_not_called()
        self.assertFalse(self.worker.state_path.exists())

    def test_concurrent_source_change_prevents_publication(self):
        self.live = False
        self.assertEqual(self.worker.poll()['status'], 'superseded')
        self.assertEqual(self.published, [])
        self.assertFalse(self.git('-C', str(self.repo), 'branch', '--list', 'codex/compat-repair-*').strip())

    def test_crash_consumes_attempt_and_pauses(self):
        self.worker.repair = mock.Mock(side_effect=W.WorkerError('quota or other interruption'))
        with self.assertRaises(W.WorkerError):
            self.worker.poll()
        state = json.loads(self.worker.state_path.read_text())
        self.assertTrue(state['paused'])
        self.assertEqual(len(state['attempts']), 1)
        self.assertEqual(self.worker.poll()['status'], 'paused')
        self.worker.repair.assert_called_once()

    def test_existing_repair_pr_prevents_second_machine_from_repairing(self):
        self.worker.api.side_effect = lambda route, payload=None: [{'html_url': 'existing'}]
        self.assertEqual(self.worker.poll()['status'], 'already_published')
        self.runner.assert_not_called()

    def test_wrong_github_account_or_api_login_cannot_run(self):
        worker = W.Worker(self.work, {'gh': 'fake-gh', 'codex': 'fake-codex'})
        worker.api = mock.Mock(return_value={'login': 'somebody'})
        with self.assertRaisesRegex(W.WorkerError, 'GitHub CLI'):
            worker.preflight()
        worker.api.return_value = {'login': W.OWNER}
        with mock.patch.object(W, 'command', return_value=b'Logged in using an API key'):
            with self.assertRaisesRegex(W.WorkerError, 'ChatGPT'):
                worker.preflight()


class MergeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.worker = W.Worker(Path(self.temp.name), {'gh': 'fake-gh', 'auto_merge': True})
        self.parent = pull()
        self.repair = pull('b' * 40)
        self.repair.update(number=43, merged=False)
        self.repair['base']['ref'] = 'feature'
        self.repair['head']['ref'] = 'codex/compat-repair-42-' + HEAD[:12]
        self.record = dict(status='published_awaiting_ci', auto_merge=True, parent_number=42,
                           parent_branch='feature', repair_number=43, head=HEAD,
                           commit=self.repair['head']['sha'], branch=self.repair['head']['ref'])
        self.worker.state['attempts']['42:' + HEAD] = self.record
        self.merges = []
        self.rules = [{'type': 'pull_request'}, {'type': 'required_status_checks', 'parameters': {
            'strict_required_status_checks_policy': True,
            'required_status_checks': [{'context': n, 'integration_id': 15368} for n in W.REQUIRED_CHECKS]}}]
        self.worker.preflight = mock.Mock()
        self.worker.pages = mock.Mock(side_effect=self.pages)
        self.worker.merge_view = mock.Mock(side_effect=self.view)
        self.worker.api = mock.Mock(side_effect=self.api)

    def api(self, route, payload=None, method=None):
        if route.endswith('/rules/branches/main'):
            return copy.deepcopy(self.rules)
        if route.endswith('/merge'):
            self.assertEqual(method, 'PUT')
            pr = self.repair if '/43/' in route else self.parent
            self.assertEqual(payload['sha'], pr['head']['sha'])
            self.merges.append(pr['number'])
            pr.update(merged=True, state='closed', merge_commit_sha='c' * 40 if pr['number'] == 43 else 'd' * 40)
            if pr['number'] == 43:
                self.parent['head']['sha'] = pr['merge_commit_sha']
            return {'merged': True, 'sha': pr['merge_commit_sha']}
        if route.endswith('/43'):
            return copy.deepcopy(self.repair)
        if route.endswith('/42'):
            return copy.deepcopy(self.parent)
        raise AssertionError(route)

    def pages(self, route, key=None):
        if key == 'workflow_runs':
            pr = self.repair if self.repair['head']['sha'] in route else self.parent
            return [{**r, 'conclusion': 'success', 'pull_requests': [{'number': pr['number']}]}
                    for r in runs(pr['head']['sha'])]
        return [copy.deepcopy(self.parent)]

    def view(self, number):
        pr = self.repair if number == 43 else self.parent
        return dict(headRefOid=pr['head']['sha'], baseRefName=pr['base']['ref'],
                    mergeStateStatus='CLEAN', mergeable='MERGEABLE', reviewDecision='',
                    statusCheckRollup=[dict(__typename='CheckRun', name=n, status='COMPLETED',
                                           conclusion='SUCCESS') for n in W.REQUIRED_CHECKS])

    def test_repair_then_fresh_parent_ci_then_main_across_restart(self):
        self.worker.poll()
        self.assertEqual(self.merges, [43])
        self.assertEqual(self.record['status'], 'parent_awaiting_ci')
        loaded = W.Worker(self.worker.root, self.worker.config)
        self.assertEqual(loaded.state['attempts']['42:' + HEAD]['parent_head'], 'c' * 40)
        self.worker.state = loaded.state
        self.worker.poll()
        self.assertEqual(self.merges, [43, 42])
        self.assertEqual(self.worker.state['attempts']['42:' + HEAD]['status'], 'merged_main')
        self.worker.poll()
        self.assertEqual(self.merges, [43, 42])

    def test_wait_for_missing_pending_wrong_pr_and_stale_workflows(self):
        original = self.pages('', 'workflow_runs')
        original = [{**r, 'head_sha': self.repair['head']['sha'], 'pull_requests': [{'number': 43}]}
                    for r in original]
        variants = [[], original[:1], [{**r, 'status': 'in_progress'} for r in original],
                    [{**r, 'head_sha': 'f' * 40} for r in original],
                    [{**r, 'pull_requests': [{'number': 99}]} for r in original],
                    [{**r, 'conclusion': 'cancelled'} for r in original]]
        for value in variants:
            with self.subTest(value=value):
                self.worker.pages = mock.Mock(return_value=value)
                self.worker.followup(self.record)
                self.assertEqual(self.merges, [])

    def test_all_checks_reviews_and_mergeability_are_required(self):
        good = self.view(43)
        variants = [{**good, 'mergeStateStatus': state} for state in ['BEHIND', 'BLOCKED', 'UNKNOWN', 'DIRTY']]
        variants += [{**good, 'reviewDecision': state} for state in ['REVIEW_REQUIRED', 'CHANGES_REQUESTED']]
        variants += [{**good, 'statusCheckRollup': []}, {**good, 'headRefOid': 'e' * 40}]
        for result in ['FAILURE', 'CANCELLED', 'SKIPPED', 'NEUTRAL']:
            value = copy.deepcopy(good)
            value['statusCheckRollup'][0]['conclusion'] = result
            variants.append(value)
        value = copy.deepcopy(good)
        value['statusCheckRollup'].append(dict(__typename='StatusContext', context='extra', state='PENDING'))
        variants.append(value)
        for value in variants:
            with self.subTest(value=value):
                self.worker.merge_view = mock.Mock(return_value=value)
                self.worker.followup(self.record)
                self.assertEqual(self.merges, [])

    def test_failed_followup_pauses_without_new_model_attempt(self):
        failed = [{**r, 'pull_requests': [{'number': 43}]} for r in runs(self.repair['head']['sha'])]
        self.worker.pages = mock.Mock(return_value=failed)
        with self.assertRaisesRegex(W.WorkerError, 'CI failed'):
            self.worker.poll()
        self.assertEqual(self.record['status'], 'needs_attention')
        self.assertTrue(self.worker.state['paused'])
        self.assertEqual(self.merges, [])

    def test_changed_source_or_repair_is_never_merged(self):
        for target in [self.parent, self.repair]:
            old = copy.deepcopy(target)
            target['head']['sha'] = 'e' * 40
            self.worker.followup(self.record)
            self.assertEqual(self.record['status'], 'superseded')
            self.assertEqual(self.merges, [])
            target.update(old)

    def test_main_requires_live_protection(self):
        self.rules = []
        with self.assertRaisesRegex(W.WorkerError, 'Main must require'):
            self.worker.followup(self.record)
        self.assertEqual(self.merges, [])

    def test_recover_merge_response_lost_after_server_success(self):
        self.worker.followup(self.record)
        self.record['status'] = 'repair_merging'
        self.record.pop('parent_head')
        self.worker.followup(self.record)
        self.assertEqual(self.merges, [43, 42])
        self.record['status'] = 'parent_merging'
        self.worker.followup(self.record)
        self.assertEqual(self.record['status'], 'merged_main')
        self.assertEqual(self.merges, [43, 42])

    def test_dry_run_pause_disabled_and_legacy_records_never_merge(self):
        self.worker.poll(dry_run=True)
        self.assertEqual(self.merges, [])
        self.worker.state['paused'] = 'operator'
        self.worker.poll()
        self.assertEqual(self.merges, [])
        self.worker.state.pop('paused')
        self.worker.config['auto_merge'] = False
        self.worker.poll()
        self.assertEqual(self.merges, [])
        self.worker.config['auto_merge'] = True
        self.record.pop('auto_merge')
        self.worker.poll()
        self.assertEqual(self.merges, [])

    def test_stale_source_refresh_uses_expected_head_and_skips_model(self):
        def api(route, payload=None, method=None):
            if '/compare/' in route:
                return {'behind_by': 2}
            if route.endswith('/update-branch'):
                self.assertEqual(method, 'PUT')
                self.assertEqual(payload, {'expected_head_sha': HEAD})
                return {'message': 'Updating'}
            return self.parent
        self.worker.api.side_effect = api
        self.assertTrue(self.worker.sync_base_before_repair(self.parent, self.record))
        self.assertEqual(self.record['status'], 'base_updated_awaiting_ci')


if __name__ == '__main__':
    unittest.main()
