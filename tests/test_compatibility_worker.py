"""Exercise the publication boundary without credentials or model calls."""
import copy
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

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('compat_worker', ROOT / 'tools/compatibility-worker.py')
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
                     'tools/compatibility-worker.py', 'tools/../secret.py', 'tools/a\nb.py',
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
                return {'html_url': 'https://github.com/test/pull/43'}
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


if __name__ == '__main__':
    unittest.main()
