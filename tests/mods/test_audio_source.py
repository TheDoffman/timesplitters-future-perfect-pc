"""Compile the production audio attribution and guest wrapper without game assets."""
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('platform', ['TSFP_PC_CONTROLS', 'TSFP_NATIVE_DISPLAY', None])
def test_source_identity_preserves_original_call_and_thread_boundaries(tmp_path, platform):
    compiler, environment = native_compiler(tmp_path)
    windows = Path(compiler).stem.lower() == 'cl'
    output = tmp_path/('audio-source.exe' if windows else 'audio-source')
    source = ROOT/'tests/mods/native_audio_source.c'
    if windows:
        command = [compiler, '/nologo', '/std:c11', '/W4', '/WX', '/O2',
            '/I'+str(ROOT/'src'), str(source), '/Fo:'+str(tmp_path/'source.obj'), '/Fe:'+str(output)]
        if platform: command.append('/D'+platform)
    else:
        command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2', '-pthread',
            '-I'+str(ROOT/'src'), str(source), '-o', str(output)]
        if platform: command.append('-D'+platform)
    built = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    checked = subprocess.run([str(output)], cwd=tmp_path, capture_output=True, text=True, timeout=20)
    assert checked.returncode == 0 and 'PASS:' in checked.stdout, checked.stdout+checked.stderr
