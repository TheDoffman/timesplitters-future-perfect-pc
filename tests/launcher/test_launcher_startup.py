"""Exercise the portable script with a synthetic child, never a real game/save."""


import json
import os
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(os.name != 'nt', reason='Windows portable launcher')
@pytest.mark.parametrize('skip', [None, False, True])
def test_script_startup_preference(tmp_path, skip):
    windows = Path(os.environ['WINDIR'])
    compiler = windows / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    shell = windows / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    (tmp_path / 'Bin').mkdir()
    (tmp_path / 'Settings').mkdir()
    controls = {'mouse': {'sensitivity': .1, 'enabled': True, 'invert_y': False}}
    if skip is not None:
        controls['launcher'] = {'skip_startup_splashes': skip}
    (tmp_path / 'Settings/pc-controls.json').write_text(json.dumps(controls))
    shutil.copyfile(ROOT / 'tools/playable-launch.ps1', tmp_path / 'Launch.ps1')
    source = tmp_path / 'child.cs'
    source.write_text('''using System;
public class Child {
    public static int Main() {
        Console.WriteLine("XMV_PLAY=" + Environment.GetEnvironmentVariable("RECOMP_XMV_PLAY"));
        Console.WriteLine("UNWANTED=" + Environment.GetEnvironmentVariable("TSFP_UNWANTED"));
        Console.WriteLine("DISCORD_CONFIG=" + Environment.GetEnvironmentVariable("TSFP_DISCORD_CONFIG"));
        return 0;
    }
}
''')
    subprocess.run([str(compiler), '/nologo', '/target:exe',
                    '/out:' + str(tmp_path / 'Bin/futureperfect_recomp.exe'),
                    str(source)], check=True, cwd=tmp_path, capture_output=True)
    env = dict(os.environ, RECOMP_XMV_PLAY='1' if skip else '0', TSFP_UNWANTED='bad')
    subprocess.run([str(shell), '-NoProfile', '-ExecutionPolicy', 'Bypass',
                    '-File', str(tmp_path / 'Launch.ps1')], check=True,
                   cwd=tmp_path, env=env, capture_output=True, timeout=20)
    output = next((tmp_path / 'Logs').glob('*.out')).read_text()
    assert 'XMV_PLAY=' + ('0' if skip else '1') in output
    assert 'UNWANTED=\n' in output.replace('\r\n', '\n')
    assert 'DISCORD_CONFIG=' + str(tmp_path / 'Settings/pc-discord.ini') in output
