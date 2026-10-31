"""Compile the production transport and run it against real host UDP sockets."""
import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('fixture', ['native_network_transport.c', 'pc_network.c'])
def test_transport(tmp_path, fixture):
    if fixture == 'pc_network.c' and sys.platform != 'win32':
        pytest.skip('Windows extended fixture uses multiple loopback aliases')
    if sys.platform not in ('win32', 'darwin'):
        pytest.skip('Supported desktop guest-memory adapters')
    (tmp_path/'pc_network_build.h').write_text(
        '#define TSFP_NETWORK_COMPATIBILITY {1}\n'
        '#define TSFP_NETWORK_COMPATIBILITY_HEX "01' + '00'*31 + '"\n')
    compiler, environment = native_compiler(tmp_path)
    executable = tmp_path/('transport.exe' if os.name == 'nt' else 'transport')
    sources = [ROOT/'src/network/pc_network.c', ROOT/'tests/network'/fixture]
    if os.name == 'nt':
        objects = []
        for i, source in enumerate(sources):
            obj = tmp_path/f'part{i}.obj'
            subprocess.run([compiler, '/nologo', '/c', '/W4', '/D_CRT_SECURE_NO_WARNINGS',
                            '/I'+str(tmp_path), '/Fo:'+str(obj), str(source)],
                           env=environment, check=True, capture_output=True, text=True)
            objects.append(str(obj))
        command = [compiler, '/nologo', *objects, '/Fe:'+str(executable), 'ws2_32.lib', 'bcrypt.lib']
    else:
        command = [compiler, '-std=c11', '-Wall', '-Wextra', '-I'+str(tmp_path),
                   *map(str, sources), '-o', str(executable)]
    subprocess.run(command, env=environment, cwd=tmp_path, check=True, capture_output=True, text=True)
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout+result.stderr
    assert 'PASS:' in result.stdout
