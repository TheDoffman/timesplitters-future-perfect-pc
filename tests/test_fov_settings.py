"""Exercise production FOV persistence and the guest settings callbacks."""
import subprocess
import sys
from pathlib import Path
import pytest
from native_compiler import native_compiler
ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'external/ts3FPrecomp'

@pytest.mark.skipif(sys.platform != 'darwin', reason='Native Mac menu compile contract')
def test_native_menu_callbacks_compile(tmp_path):
    # The portable pointer fixture supplies its own FOV callback. Compile the
    # real translation unit too: a Windows-only callback guard otherwise passes
    # policy tests but breaks the Mac link after a shared menu feature is added.
    compiler, environment = native_compiler(tmp_path)
    result = subprocess.run([compiler, '-std=c11', '-fsyntax-only',
        '-Werror=undefined-internal', '-DTSFP_NATIVE_DISPLAY',
        '-I' + str(ROOT / 'src'), '-I' + str(RUNTIME / 'templates/runtime'),
        '-I' + str(RUNTIME / 'src'),
        '-I' + str(RUNTIME / 'src/input'), '-I' + str(RUNTIME / 'src/config'),
        str(ROOT / 'src/pc_display_menu.c')], cwd=tmp_path, env=environment,
        capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr

@pytest.mark.skipif(sys.platform != 'win32', reason='Windows settings persistence adapter')
def test_fov_settings(tmp_path):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / 'fov.exe'
    result = subprocess.run([compiler, '/nologo', '/std:c11', '/O2', '/W3',
        '/I' + str(ROOT / 'src'), '/I' + str(RUNTIME / 'templates/runtime'),
        '/I' + str(RUNTIME / 'src/input'), '/I' + str(RUNTIME / 'src/config'),
        str(ROOT / 'tests/fov_settings.c'), str(ROOT / 'src/texture_mode.c'),
        '/Fe:' + str(exe), '/link', 'user32.lib'],
        cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe), str(tmp_path / 'display.ini')],
        capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout
