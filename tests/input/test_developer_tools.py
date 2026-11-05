"""Compile the shared overlay with deterministic input and isolated settings boundaries."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('name', ['input/developer_tools.c', 'gameplay/developer_tools_guest.c'])
def test_developer_tools_input(tmp_path, name):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('developer-tools.exe' if os.name == 'nt' else 'developer-tools')
    source = ROOT / 'tests' / name
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/W3', '/WX', str(source), str(ROOT / 'src/input/pc_input.c'), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                '-I' + str(ROOT / 'external/ts3FPrecomp/src'), str(source), str(ROOT / 'src/input/pc_input.c'), '-lm', '-lpthread', '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


def test_original_checkpoint_destination(tmp_path):
    import hashlib
    from test_recomp_float_joins import compile_and_run, config, FunctionTranslator
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required for original checkpoint lookup')
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(path))
        info = dict(name='sub_0025EC30', end=0x25EC66)
        code = FunctionTranslator(image, {0x25EC30: info}).translate_function(0x25EC30, info)
        source = (ROOT / 'tests/gameplay/developer_checkpoint_original.c').read_text()
        print(compile_and_run(tmp_path, source.replace('/* ORIGINAL_CHECKPOINT_LOOKUP */', code)))
    finally:
        vars(config).update(previous)
