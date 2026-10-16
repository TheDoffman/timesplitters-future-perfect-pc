"""Check launcher inclusion and package checksums using tiny synthetic runtime files."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_release', ROOT / 'tools/package-release.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class LauncherPackaging(unittest.TestCase):
    def test_manifest_and_user_data_exclusion(self):
        with tempfile.TemporaryDirectory(prefix='launcher-package-') as directory:
            root = Path(directory)
            upstream = root / 'external/ts3FPrecomp'

            def put(path):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'synthetic test fixture')

            for path in [root / 'build/windows/Release/futureperfect_recomp.exe',
                         root / 'build/launcher/TimeSplitters Launcher.exe',
                         root / 'build/launcher/Bin/TimeSplitters.App.exe',
                         root / 'config/pc-controls.default.json', root / 'tools/playable-launch.ps1',
                         root / 'tools/start-game.ps1', root / 'tools/release/ImportGame.cs',
                         root / 'tools/release/Import-game.ps1', root / 'tools/release/Play.cmd',
                         root / 'Data/default.xbe', root / 'Saves/profile',
                         root / 'Settings/pc-controls.json']:
                put(path)
            manifest_path = root / 'build/launcher/Support/Launcher/manifest.json'
            manifest_path.parent.mkdir(parents=True, exist_ok=True)
            manifest_path.write_text(json.dumps({'schema': 1, 'version': '1.1.0', 'files': {}}))
            for relative in ['LICENSE', 'LICENSE.upstream-MIT', 'NOTICE', 'LICENSES/LGPL-2.1.txt',
                             'third_party/SDL/LICENSE.txt', 'third_party/ffmpeg/LICENSE.txt',
                             'third_party/ffmpeg/bin/avcodec-62.dll', 'third_party/ffmpeg/bin/avutil-60.dll',
                             'third_party/ffmpeg/bin/swresample-6.dll']:
                put(upstream / relative)
            put(root / 'ProgramFiles/Microsoft Visual Studio/2022/Test/VC/Redist/MSVC/1/x64/Microsoft.VC143.CRT/vcruntime140.dll')
            with mock.patch.object(package, 'ROOT', root), \
                    mock.patch.object(sys, 'argv', ['package-release.py', '--version', 'v1.2.3']), \
                    mock.patch.dict(os.environ, {'ProgramFiles': str(root / 'ProgramFiles')}), \
                    mock.patch.object(package.subprocess, 'run') as build, \
                    mock.patch.object(package.subprocess, 'check_output', return_value='test-source\n'), \
                    contextlib.redirect_stdout(io.StringIO()):
                package.main()
                self.assertEqual(build.call_count, 1)
                self.assertIn(str(root / 'tools/build-launcher.ps1'), build.call_args.args[0])
            archive_path = next((root / 'build/releases').glob('*.zip'))
            prefix = archive_path.stem + '/'
            with zipfile.ZipFile(archive_path) as archive:
                self.assertIn(prefix + 'TimeSplitters Launcher.exe', archive.namelist())
                self.assertIn(prefix + 'Bin/TimeSplitters.App.exe', archive.namelist())
                self.assertIn(prefix + 'Support/Launcher/manifest.json', archive.namelist())
                self.assertFalse(any('/Data/' in name or '/Saves/' in name for name in archive.namelist()))
                manifest = json.loads(archive.read(prefix + 'BUILD.json'))
                self.assertEqual(manifest['version'], 'v1.2.3')
                self.assertEqual(manifest['launcher_version'], '1.1.0')
                self.assertIn('TimeSplitters Launcher.exe', manifest['files'])
                for name, digest in manifest['files'].items():
                    self.assertEqual(hashlib.sha256(archive.read(prefix + name)).hexdigest(), digest)
            digest = archive_path.with_suffix('.zip.sha256').read_text().split()[0]
            self.assertEqual(hashlib.sha256(archive_path.read_bytes()).hexdigest(), digest)

    def test_invalid_release_versions_are_rejected(self):
        for tag in ['main', '../escape', 'v01.2.3', 'v1.2.3-alpha.01']:
            with mock.patch.object(sys, 'argv', ['package-release.py', '--version', tag]), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                package.main()


if __name__ == '__main__':
    unittest.main()
