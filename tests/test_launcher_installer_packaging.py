"""Reject invalid installer payloads before compiling or touching an installation."""
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'build/tools/innosetup-6.7.3/ISCC.exe'


@unittest.skipUnless(os.name == 'nt' and COMPILER.exists(), 'Pinned Windows installer compiler required')
class InstallerPayload(unittest.TestCase):
    def test_invalid_payloads_are_rejected_before_setup_is_created(self):
        version = re.search(r'Value = "([^"]+)"', (ROOT/'tools/launcher/LauncherVersion.cs').read_text()).group(1)
        powershell = Path(os.environ['WINDIR'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
        for problem in ('schema', 'version', 'checksum', 'extra', 'executable_version'):
            with self.subTest(problem=problem), tempfile.TemporaryDirectory(prefix='tsfp-setup-payload-') as directory:
                root = Path(directory)
                payload = root/'payload with spaces'
                files = {}
                for name in ('TimeSplitters Launcher.exe', 'Bin/TimeSplitters.App.exe'):
                    path = payload/name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b'synthetic payload, never execute')
                    files[name] = hashlib.sha256(path.read_bytes()).hexdigest()
                record = {'schema': 1, 'version': version, 'files': files}
                expected = {'schema': 'version differs', 'version': 'version differs', 'checksum': 'checksum mismatch',
                            'extra': 'Unexpected launcher payload', 'executable_version': 'Executable version differs'}[problem]
                if problem == 'schema': record['schema'] = 99
                if problem == 'version': record['version'] = '999.0.0'
                if problem == 'checksum': files['TimeSplitters Launcher.exe'] = '0'*64
                if problem == 'extra': files['Data/default.xbe'] = '0'*64
                manifest = payload/'Support/Launcher/manifest.json'
                manifest.parent.mkdir(parents=True)
                manifest.write_text(json.dumps(record))
                output = root/'output'
                env = dict(os.environ)
                env['PSModulePath'] = str(Path(os.environ['WINDIR'])/'System32/WindowsPowerShell/v1.0/Modules')
                result = subprocess.run([str(powershell), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    str(ROOT/'tools/build-launcher-installer.ps1'), '-Output', str(output), '-LauncherDirectory', str(payload)],
                    capture_output=True, text=True, timeout=30, env=env)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(expected, result.stdout+result.stderr)
                self.assertFalse(list(output.glob('*.exe')))


if __name__ == '__main__':
    unittest.main()
