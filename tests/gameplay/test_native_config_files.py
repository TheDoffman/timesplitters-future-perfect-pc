"""Exercise native atomic writes on disposable files, including refusal paths."""


import ctypes
from pathlib import Path
import subprocess
import sys
import pytest

ROOT = Path(__file__).resolve().parents[2]
pytestmark = pytest.mark.skipif(sys.platform != 'darwin', reason='macOS settings adapter')

@pytest.fixture
def write_config(tmp_path):
    library = tmp_path / 'config.dylib'
    subprocess.run(['clang', '-dynamiclib', str(ROOT / 'src/native/config_files.c'), '-o', str(library)], check=True)
    fn = ctypes.CDLL(str(library)).tsfp_config_write
    fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    fn.restype = ctypes.c_int
    return lambda path, text, backup=1: bool(fn(str(path).encode(), text.encode(), backup))

def test_atomic_replace_and_exact_backup(tmp_path, write_config):
    path = tmp_path / 'controls.json'
    path.write_bytes(b'original\r\n')
    path.chmod(0o640)
    assert write_config(path, 'replacement\n')
    assert path.read_bytes() == b'replacement\n'
    assert path.with_suffix('.json.bak').read_bytes() == b'original\r\n'
    assert path.stat().st_mode & 0o777 == 0o640
    assert not list(tmp_path.glob('*.tmp'))

def test_refuse_symlink_and_read_only_without_changing_original(tmp_path, write_config):
    path = tmp_path / 'controls.json'
    path.write_text('keep')
    link = tmp_path / 'link.json'
    link.symlink_to(path)
    assert not write_config(link, 'bad')
    path.chmod(0o400)
    assert not write_config(path, 'bad')
    assert path.read_text() == 'keep'
    assert not path.with_suffix('.json.bak').exists()

def test_missing_controls_refused_but_new_display_supported(tmp_path, write_config):
    path = tmp_path / 'display.ini'
    assert not write_config(path, 'bad')
    assert write_config(path, 'TSFP_DISPLAY_V1 1280 720\n', backup=0)
    assert path.read_text() == 'TSFP_DISPLAY_V1 1280 720\n'
