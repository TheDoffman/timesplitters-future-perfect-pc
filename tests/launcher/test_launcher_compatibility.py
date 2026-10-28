"""Scope changes must not bypass either launcher host; this suite needs no API."""


import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('compatibility', ROOT/'tools/ci/launcher-compatibility.py')
compatibility = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compatibility)


class Compatibility(unittest.TestCase):
    def test_shared_pages_platform_adapters_and_build_inputs_select_both_hosts(self):
        for path in ('tools/launcher/SettingsPage.cs', 'tools/launcher/macos/NativeHost.cs',
                     'tools/launcher/LauncherPlatform.cs', 'tools/build/build-launcher.ps1',
                     'tools/build/build-mac-launcher.py', 'tests/launcher/launcher_shared_ui.cs',
                     'config/build/mac-launcher-sdk.json', 'src/interface/pc_display_modes.inc',
                     '.github/workflows/launcher-build.yml'):
            with self.subTest(path=path):
                self.assertTrue(compatibility.relevant(path))

    def test_unrelated_game_and_docs_changes_do_not_need_launcher_builds(self):
        for path in ('docs/players/macos.md', 'docs/project/STATUS.md', 'docs/project/MAC_PORT_BACKLOG.md', 'src/native/input_sdl.c',
                     'patches/texture-mode-ui.patch', 'tools/textures/texture-pack.py'):
            self.assertFalse(compatibility.relevant(path))

    def test_moved_packaging_and_icon_checks_still_select_their_owner(self):
        for path in ('tools/build/build-game-icon.ps1', 'tests/repository/test_game_icon.py',
                     'tools/build/launcher-cache.ps1', 'tests/launcher/test_launcher_cache.py',
                     'tests/launcher/package_launcher.py', 'tools/release/package-release.py',
                     'tools/release/ImportGame.cs', 'requirements.txt',
                     'patches/platform/1150-game-icon.patch',
                     'tests/support/native_compiler.py',
                     'tests/launcher/test_launcher_startup.py'):
            with self.subTest(path=path):
                self.assertTrue(compatibility.relevant(path))

    def test_main_push_keeps_launcher_validation_after_runtime_deduplication(self):
        workflow = (ROOT/'.github/workflows/launcher-compatibility.yml').read_text()
        self.assertIn('  push:\n    branches: [main]\n', workflow)
        self.assertIn('github.event.pull_request.base.sha || github.event.before', workflow)
        self.assertIn('0000000000000000000000000000000000000000', workflow)

    def test_failed_cancelled_and_skipped_builds_cannot_pass(self):
        for result in ('failure', 'cancelled', 'skipped', ''):
            with self.subTest(result=result), self.assertRaises(ValueError):
                compatibility.gate('success', 'true', result)
        self.assertIn('passed', compatibility.gate('success', 'true', 'success'))

    def test_scope_failure_or_missing_output_cannot_pass(self):
        for scope, run, build in (('failure','false','skipped'), ('cancelled','true','success'),
                                  ('success','','skipped'), ('success','false','failure')):
            with self.subTest(scope=scope,run=run), self.assertRaises(ValueError):
                compatibility.gate(scope,run,build)
        self.assertIn('No launcher', compatibility.gate('success', 'false', 'skipped'))

    def test_revision_input_rejects_options_and_abbreviations(self):
        for value in ('HEAD', '--output=/tmp/test', 'abc123', None):
            with self.assertRaises(ValueError):
                compatibility.exact_sha(value)

    def test_renamed_out_and_deleted_launcher_files_remain_in_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(['git', *args], cwd=root, stderr=subprocess.DEVNULL).decode().strip()
            git('init', '-q'); git('config','user.name','Test'); git('config','user.email','test@example.invalid')
            (root/'tools/launcher').mkdir(parents=True)
            (root/'tools/launcher/Old.cs').write_text('class Old {}\n',newline='\n')
            (root/'tools/launcher/Delete.cs').write_text('class Delete {}\n',newline='\n')
            git('add','.'); git('commit','-qm','base'); base=git('rev-parse','HEAD')
            git('mv','tools/launcher/Old.cs','Moved.cs'); git('rm','tools/launcher/Delete.cs')
            git('commit','-qm','move out and delete'); head=git('rev-parse','HEAD')
            paths=compatibility.changed_paths(root,base,head)
            self.assertIn('tools/launcher/Old.cs',paths)
            self.assertIn('tools/launcher/Delete.cs',paths)
            self.assertTrue(any(map(compatibility.relevant,paths)))

    def test_workflow_has_no_api_or_write_credentials(self):
        workflow=(ROOT/'.github/workflows/launcher-compatibility.yml').read_text()
        for forbidden in ('openai/', 'secrets.', 'write', 'pull_request_target', 'OPENAI_API_KEY'):
            self.assertNotIn(forbidden,workflow)
        self.assertIn('name: Launcher compatibility\n',workflow)
        self.assertIn('needs: [scope, build]',workflow)
        self.assertIn('source_ref: ${{ needs.scope.outputs.head }}',workflow)


if __name__ == '__main__':
    unittest.main()
