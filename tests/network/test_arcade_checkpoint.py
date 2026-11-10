"""Compile production checkpoint capture/restore without native game assets."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


def compile_arcade_fixture(tmp_path, fixture='arcade_checkpoint.c'):
    compiler, environment = native_compiler(tmp_path)
    executable = tmp_path / ('arcade-checkpoint.exe' if os.name == 'nt' else 'arcade-checkpoint')
    source = ROOT / 'tests/network'/fixture
    control = fixture == 'arcade_control_transport.c'
    sources = [str(source)]
    if control:
        sources.insert(0, str(ROOT/'src/network/pc_network.c'))
        (tmp_path/'pc_network_build.h').write_text('#define TSFP_NETWORK_COMPATIBILITY {1}\n'
                                                 '#define TSFP_NETWORK_COMPATIBILITY_HEX "fixture"\n')
    if Path(compiler).name.lower() in ('cl', 'cl.exe'):
        command = [compiler, '/nologo', '/std:c11', '/W4', '/O2', '/I'+str(ROOT/'src'),
                   '/I'+str(tmp_path), '/Fe:'+str(executable), *sources]
        if control:
            command.extend(['/DTSFP_ARCADE_MIGRATION', '/D_CRT_SECURE_NO_WARNINGS', 'ws2_32.lib', 'bcrypt.lib'])
    else:
        command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                   '-I'+str(ROOT/'src'), '-I'+str(tmp_path), *sources, '-lm', '-o', str(executable)]
        if control:
            command.extend(['-DTSFP_ARCADE_MIGRATION', '-D_DEFAULT_SOURCE', '-pthread'])
    compiled = subprocess.run(command, cwd=tmp_path, env=environment, text=True,
                              capture_output=True, timeout=60)
    assert compiled.returncode == 0, compiled.stdout+compiled.stderr
    return executable


@pytest.mark.parametrize('fixture', ['arcade_checkpoint.c', 'arcade_control_transport.c'])
def test_native_arcade_checkpoint(tmp_path, fixture):
    executable = compile_arcade_fixture(tmp_path, fixture)
    result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=10)
    assert result.returncode == 0, result.stdout+result.stderr
    assert 'PASS: Arcade checkpoint' in result.stdout
