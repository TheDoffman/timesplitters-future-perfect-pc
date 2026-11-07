"""Compile the production transport and run it against real host UDP sockets."""
import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('fixture', ['native_network_transport.c', 'pc_network.c', 'network_eligibility.c'])
def test_transport(tmp_path, fixture):
    if fixture == 'pc_network.c' and sys.platform != 'win32':
        pytest.skip('Windows extended fixture uses multiple loopback aliases')
    if sys.platform not in ('win32', 'darwin', 'linux'):
        pytest.skip('Supported desktop guest-memory adapters')
    (tmp_path/'pc_network_build.h').write_text(
        '#define TSFP_NETWORK_COMPATIBILITY {1}\n'
        '#define TSFP_NETWORK_COMPATIBILITY_HEX "01' + '00'*31 + '"\n')
    compiler, environment = native_compiler(tmp_path)
    executable = tmp_path/('transport.exe' if os.name == 'nt' else 'transport')
    sources = [ROOT/'tests/network'/fixture] if fixture == 'network_eligibility.c' else [ROOT/'src/network/pc_network.c', ROOT/'tests/network'/fixture]
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
        command = [compiler, '-std=c11', '-D_DEFAULT_SOURCE', '-Wall', '-Wextra', '-pthread',
                   '-I'+str(tmp_path), '-I'+str(ROOT/'src/network'),
                   *map(str, sources), '-o', str(executable)]
    compiled = subprocess.run(command, env=environment, cwd=tmp_path, capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stdout+compiled.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout+result.stderr
    assert 'PASS:' in result.stdout
    if fixture == 'network_eligibility.c':
        for mode in ('lan', 'online', 'missing', 'disconnected'):
            result = subprocess.run([str(executable), mode], cwd=tmp_path,
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, result.stdout+result.stderr
            assert 'PASS:' in result.stdout
