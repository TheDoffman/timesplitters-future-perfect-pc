"""Compile the actual shared DDS reader and test mod precedence/fallback."""
import os
from pathlib import Path
import struct
import subprocess

import pytest
from native_compiler import native_compiler
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
from mod_package import ModError, validate_dds, validate_payload
KEY = '0123456789abcdef.dds'


def dds(marker, width=4, height=4):
    # 4x4/2x2/1x1 BC1, complete mip chain, distinct synthetic payload.
    header = bytearray(128)
    for offset, value in {0: 0x20534444, 4: 124, 12: height, 16: width, 28: 3,
                          76: 32, 80: 4, 84: 0x31545844}.items():
        struct.pack_into('<I', header, offset, value)
    return bytes(header) + bytes([marker]) * 24


@pytest.fixture(scope='module')
def reader(tmp_path_factory):
    folder = tmp_path_factory.mktemp('texture-mod-reader')
    compiler, environment = native_compiler(folder)
    executable = folder / ('reader.exe' if os.name == 'nt' else 'reader')
    source = ROOT / 'tests/mods/texture_mod_mount.c'
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/W4', '/I' + str(ROOT / 'src/textures'),
                   str(source), '/Fe:' + str(executable)]
    else:
        command = [compiler, '-std=c11', '-Wall', '-Wextra', '-I' + str(ROOT / 'src/textures'),
                   str(source), '-o', str(executable)]
    result = subprocess.run(command, cwd=folder, env=environment, capture_output=True,
                            text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return executable, environment


@pytest.mark.parametrize('case,expected', [
    ('no-mods', 1), ('override', 2), ('missing', 1), ('malformed', 1),
    ('wrong-aspect', 1), ('mod-only', 2), ('both-missing', -1),
    ('non-key-name', 1),
])
def test_actual_shared_mod_texture_selection(reader, tmp_path, case, expected):
    executable, environment = reader
    official = tmp_path / 'official'
    mods = tmp_path / 'mods'
    official.mkdir()
    mods.mkdir()
    name = 'unknown.dds' if case == 'non-key-name' else KEY
    original = official / name
    if case not in ('mod-only', 'both-missing'):
        original.write_bytes(dds(1))
    if case in ('override', 'mod-only', 'non-key-name'):
        (mods / name).write_bytes(dds(2))
    elif case == 'malformed':
        (mods / name).write_bytes(b'bad DDS')
    elif case == 'wrong-aspect':
        (mods / name).write_bytes(dds(2, width=8))
    environment = dict(environment)
    environment.pop('TSFP_MOD_TEXTURE_DIR', None)
    if case != 'no-mods':
        environment['TSFP_MOD_TEXTURE_DIR'] = str(mods)
    result = subprocess.run([str(executable), str(original), '4', '4', str(expected)],
                            env=environment, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize('mutation', ['none', 'missing-mips', 'truncated', 'extra',
                                     'bad-format', 'volume', 'non-power-two'])
def test_creator_texture_validation_matches_runtime_contract(mutation):
    data = bytearray(dds(1))
    if mutation == 'missing-mips':
        struct.pack_into('<I', data, 28, 2)
    elif mutation == 'truncated':
        data = data[:-1]
    elif mutation == 'extra':
        data += b'\x00'
    elif mutation == 'bad-format':
        struct.pack_into('<I', data, 84, 0)
    elif mutation == 'volume':
        struct.pack_into('<I', data, 24, 1)
    elif mutation == 'non-power-two':
        struct.pack_into('<I', data, 16, 3)
    if mutation == 'none':
        validate_dds(data)
        validate_payload('textures/' + KEY, data)
        with pytest.raises(ModError):
            validate_payload('textures/arbitrary-name.dds', data)
    else:
        with pytest.raises(ModError):
            validate_dds(data)
