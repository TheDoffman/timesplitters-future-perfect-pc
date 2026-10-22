"""Run the actual shared framebuffer cache against a fake screen-copy device."""
import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'),
                    reason='HLE renderer is compiled on Windows and macOS')
@pytest.mark.parametrize('break_refresh', [False, True], ids=['fixed', 'detect-stale-copy'])
def test_framebuffer_sampling_passes(tmp_path, break_refresh):
    compiler, environment = native_compiler(tmp_path)
    source = ROOT / 'external/ts3FPrecomp/src'
    includes = [source, ROOT / 'src', *(source / name for name in ('d3d', 'hle', 'kernel', 'config', 'platform'))]
    if break_refresh:
        # Prove the behavioral test detects losing the production binding hook.
        original = (source / 'hle/hle_d3d8_texture.c').read_text()
        hook = '        framebuffer_binding_changed(stage, texture);'
        assert original.count(hook) == 1
        mutant = tmp_path / 'mutant/hle/hle_d3d8_texture.c'
        mutant.parent.mkdir(parents=True)
        mutant.write_text(original.replace(hook, '        /* mutation: stale copy */'))
        includes.insert(0, mutant.parent.parent)
    exe = tmp_path / ('framebuffer.exe' if os.name == 'nt' else 'framebuffer')
    fixture = ROOT / 'tests/framebuffer_sampling.c'
    if os.name == 'nt':
        # MSVC otherwise accepts mock/declaration mismatches as warnings,
        # while Clang rejects them. Enforce the same contract on Windows.
        command = [compiler, '/nologo', '/std:c11', '/O2', '/Gy', '/Gw',
                   '/we4028', '/we4029']
        command += ['/I' + str(path) for path in includes]
        command += [str(fixture), '/Fe:' + str(exe), '/Fo:' + str(tmp_path / 'test.obj'),
                    '/link', '/OPT:REF']
    else:
        command = [compiler, '-std=gnu11', '-O2', '-Wl,-dead_strip']
        command += ['-I' + str(path) for path in includes]
        command += [str(fixture), '-o', str(exe)]
    result = subprocess.run(command, cwd=tmp_path, env=environment,
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    if break_refresh:
        assert result.returncode != 0, 'Regression did not detect the stale framebuffer copy'
        assert 'copies == 2 && sampled == 20' in result.stderr
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS: framebuffer sampling passes' in result.stdout
