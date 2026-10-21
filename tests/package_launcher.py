"""Check launcher inclusion and package checksums using tiny synthetic runtime files."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import subprocess
import struct
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
spec = importlib.util.spec_from_file_location('package_release', ROOT / 'tools/package-release.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class LauncherPackaging(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Windows launcher build prerequisite')
    def test_release_build_refuses_missing_artwork(self):
        powershell = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
        with tempfile.TemporaryDirectory(prefix='launcher-artwork-') as directory:
            root = Path(directory)
            banner = root / 'banner.png'
            banner.write_bytes(b'synthetic banner; compilation must not start')
            for missing, banner_path in [('banner', root / 'absent.png'), ('logo', banner)]:
                with self.subTest(missing=missing):
                    output = root / missing
                    result = subprocess.run([str(powershell), '-NoProfile', '-ExecutionPolicy', 'Bypass',
                        '-File', str(ROOT / 'tools/build-launcher.ps1'), '-Output', str(output),
                        '-RequireArtwork', '-BannerPath', str(banner_path), '-LogoPath', str(root / 'absent-logo.png')],
                        capture_output=True, text=True, timeout=30)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('Release launcher artwork is required: ' + missing + ' missing', result.stderr)
                    self.assertFalse((output / 'TimeSplitters Launcher.exe').exists())

    def test_manifest_and_user_data_exclusion(self):
        with tempfile.TemporaryDirectory(prefix='launcher-package-') as directory:
            root = Path(directory)
            (root/'VERSION').write_text('0.2.0-alpha.4\n')
            upstream = root / 'external/ts3FPrecomp'
            # Exercise the default bundled path without network or real artwork.
            textures = root/'assets/textures'
            textures.mkdir(parents=True)
            words = [0]*32
            for index, value in {0: 0x20534444, 1: 124, 3: 1, 4: 1, 7: 1,
                                 19: 32, 20: 4, 21: 0x31545844}.items():
                words[index] = value
            dds = struct.pack('<32I', *words)+b'\0'*8
            key = '0123456789abcdef'
            (textures/(key+'.dds')).write_bytes(dds)
            (textures/(key+'.json')).write_text(json.dumps({
                'key': key, 'sha256': hashlib.sha256(dds).hexdigest(),
                'prompt': 'private fixture prompt', 'source': '/private/art.png'}))

            def put(path):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'synthetic test fixture')

            for path in [root / 'build/windows/Release/futureperfect_recomp.exe',
                         root / 'build/windows/Release/futureperfect_recomp.pdb',
                         root / 'build/windows/Release/tsfp_crash_helper.exe',
                         root / 'build/launcher/TimeSplitters Launcher.exe',
                         root / 'build/launcher/Bin/TimeSplitters.App.exe',
                         root / 'config/pc-controls.default.json', root / 'tools/playable-launch.ps1',
                         root / 'config/pc-discord.default.ini', root / 'docs/discord-presence.md',
                         root / 'tools/start-game.ps1', root / 'tools/release/ImportGame.cs',
                         root / 'tools/launcher/GameMonogram.NOTICE.txt',
                         root / 'tools/release/Import-game.ps1', root / 'tools/release/Play.cmd',
                         root / 'Data/default.xbe', root / 'Saves/profile',
                         root / 'Settings/pc-controls.json']:
                put(path)
            discord_default = (ROOT / 'config/pc-discord.default.ini').read_bytes()
            (root / 'config/pc-discord.default.ini').write_bytes(discord_default)
            (root / 'config/pc-discord.ini').write_text('TSFP_DISCORD_V1 1 999999999999999999\n')
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
                    mock.patch.object(package.subprocess, 'check_output', side_effect=lambda args,**kw: b'' if 'ls-files' in args or 'diff' in args else 'test-source\n'), \
                    mock.patch.object(package, 'bundle', return_value={'test':True}), \
                    contextlib.redirect_stdout(io.StringIO()):
                package.main()
                self.assertEqual(build.call_count, 2)
                self.assertIn(str(root / 'tools/build-launcher.ps1'), build.call_args.args[0])
                self.assertIn('-RequireArtwork', build.call_args.args[0])
                self.assertIn('-Test', build.call_args.args[0])
            archive_path = root / 'build/releases/TimeSplitters-Future-Perfect-PC-v1.2.3-windows-x64.zip'
            prefix = archive_path.stem + '/'
            with zipfile.ZipFile(archive_path) as archive:
                self.assertIn(prefix + 'TimeSplitters Launcher.exe', archive.namelist())
                self.assertIn(prefix + 'Bin/TimeSplitters.App.exe', archive.namelist())
                self.assertIn(prefix + 'Bin/tsfp_crash_helper.exe', archive.namelist())
                self.assertIn(prefix + 'Support/Discord/README.txt', archive.namelist())
                self.assertEqual(archive.read(prefix + 'Support/Discord/pc-discord.default.ini'), discord_default)
                self.assertNotIn(prefix + 'Discord-setup.txt', archive.namelist())
                self.assertNotIn(prefix + 'Settings/pc-discord.ini', archive.namelist())
                self.assertIn('Support/Discord/README.txt', archive.read(prefix + 'README.txt').decode())
                self.assertTrue(discord_default.startswith(b'TSFP_DISCORD_V1 0 '))
                self.assertFalse(any('/config/pc-discord.ini' in name for name in archive.namelist()))
                self.assertFalse(any(name.endswith('.pdb') for name in archive.namelist()))
                self.assertIn(prefix + 'Support/Launcher/manifest.json', archive.namelist())
                self.assertIn(prefix + 'Licenses/Game-icon-lettering.txt', archive.namelist())
                self.assertFalse(any('/Data/' in name or '/Saves/' in name for name in archive.namelist()))
                manifest = json.loads(archive.read(prefix + 'BUILD.json'))
                self.assertEqual(manifest['version'], 'v1.2.3')
                self.assertEqual(manifest['textures']['source'], 'repository')
                self.assertEqual(manifest['textures']['texture_count'], 1)
                self.assertEqual(archive.read(prefix+'Textures/'+key+'.dds'), dds)
                public = json.loads(archive.read(prefix+'Textures/'+key+'.json'))
                self.assertNotIn('prompt', public)
                self.assertNotIn('source', public)
                self.assertEqual(manifest['launcher_version'], '1.1.0')
                self.assertIn('TimeSplitters Launcher.exe', manifest['files'])
                for name, digest in manifest['files'].items():
                    self.assertEqual(hashlib.sha256(archive.read(prefix + name)).hexdigest(), digest)
            digest = archive_path.with_suffix('.zip.sha256').read_text().split()[0]
            self.assertEqual(hashlib.sha256(archive_path.read_bytes()).hexdigest(), digest)
            with zipfile.ZipFile(root/'build/releases/TimeSplitters-Future-Perfect-PC-v1.2.3-windows-x64-symbols.zip') as symbols:
                self.assertIn('futureperfect_recomp.pdb', symbols.namelist())
                info = json.loads(symbols.read('BUILD.json'))
                self.assertEqual(info['files']['futureperfect_recomp.exe'], hashlib.sha256(symbols.read('futureperfect_recomp.exe')).hexdigest())

    def test_invalid_release_versions_are_rejected(self):
        for tag in ['main', '../escape', 'v01.2.3', 'v1.2.3-alpha.01']:
            with mock.patch.object(sys, 'argv', ['package-release.py', '--version', tag]), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                package.main()


if __name__ == '__main__':
    unittest.main()
