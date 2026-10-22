"""Compile the production Quit to Desktop guest adapter on either host."""
import os
import hashlib
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("native", [False, True])
@pytest.mark.parametrize("fixture", ['quit_menu', 'story_menu', 'story_lobby'])
def test_quit_menu(tmp_path, native, fixture):
    compiler, env = native_compiler(tmp_path)
    source = ROOT / 'tests' / (fixture+'.c')
    exe = tmp_path / ('quit.exe' if os.name == 'nt' else 'quit')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-lm', '-o', str(exe)]
    if native:
        args.insert(1, "/DTSFP_NATIVE_DISPLAY" if os.name == "nt" else "-DTSFP_NATIVE_DISPLAY")
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout


def test_story_original_us_entry_contract():
    """Check the observed original route and callback padding without disclosing code."""
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported local US executable required')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    base = struct.unpack_from('<I', image, 0x104)[0]
    count, table = struct.unpack_from('<II', image, 0x11c)
    sections = [struct.unpack_from('<IIII', image, table-base+i*0x38+4) for i in range(count)]
    def read(address, size):
        for start, _, offset, length in sections:
            if start <= address and address+size <= start+length:
                return image[offset+address-start:offset+address-start+size]
        raise AssertionError('Unmapped bounded US contract read')
    assert read(0x1201c, 4) == b'\xcc'*4
    call = read(0x2d0c89, 5)
    assert call[0] == 0xe8
    assert 0x2d0c8e+struct.unpack_from('<i', call, 1)[0] == 0x79d60
    assert b'\x68'+struct.pack('<I', 0x5238e0) in read(0x2d0aa0, 11)
    assert struct.unpack('<I', read(0x5238e0, 4))[0] == 0x2cb090
