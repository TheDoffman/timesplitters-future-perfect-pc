"""Compile the shared overlay with deterministic input and isolated settings boundaries."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('name', ['input/developer_tools.c', 'gameplay/developer_tools_guest.c', 'network/developer_lobby_guest.c'])
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


@pytest.mark.parametrize('fixture,address,end,marker', [
    ('developer_checkpoint_original.c',0x25EC30,0x25EC66,'/* ORIGINAL_CHECKPOINT_LOOKUP */'),
    ('developer_ammo_original.c',0x1B66E0,0x1B673F,'/* ORIGINAL_AMMO_CONSUMPTION */'),
    ('developer_cutscene_original.c',0x98E50,0x98F51,'/* ORIGINAL_CUTSCENE_CLOCK */'),
])
def test_original_developer_routine(tmp_path,fixture,address,end,marker):
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
        info = dict(name=f'sub_{address:08X}', end=end)
        code = FunctionTranslator(image, {address: info}).translate_function(address, info)
        source = (ROOT / 'tests/gameplay' / fixture).read_text()
        print(compile_and_run(tmp_path, source.replace(marker, code)))
    finally:
        vars(config).update(previous)


def test_original_developer_body_visibility(tmp_path):
    import hashlib
    from test_recomp_float_joins import compile_and_run, config, FunctionTranslator
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required')
    image = bytearray(path.read_bytes())
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(path))
        code = []
        for address, end in ((0xA02F9, 0xA0333), (0xA1BF1, 0xA1C31)):
            # Execute the original branch in isolation, returning at its join.
            image[config.va_to_file_offset(end)] = 0xC3
            info = dict(name=f'sub_{address:08X}', end=end + 1)
            code.append(FunctionTranslator(bytes(image), {address: info}).translate_function(address, info))
        source = (ROOT / 'tests/gameplay/developer_body_original.c').read_text()
        print(compile_and_run(tmp_path, source.replace('/* ORIGINAL_VISIBILITY_BRANCHES */', '\n'.join(code))))
    finally:
        vars(config).update(previous)
