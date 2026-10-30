"""Exercise production FOV persistence and the guest settings callbacks."""

from project_paths import source_file as repo_file

import subprocess
import sys
from pathlib import Path
import pytest
from native_compiler import native_compiler
ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / 'external/ts3FPrecomp'

@pytest.mark.parametrize('source', ['pc_display_menu.c', 'pc_prompts.c'])
@pytest.mark.skipif(sys.platform != 'darwin', reason='Native Mac menu compile contract')
def test_native_menu_callbacks_compile(tmp_path, source):
    # The portable pointer fixture supplies its own FOV callback. Compile the
    # real translation unit too: a Windows-only callback guard otherwise passes
    # policy tests but breaks the Mac link after a shared menu feature is added.
    compiler, environment = native_compiler(tmp_path)
    result = subprocess.run([compiler, '-std=c11', '-fsyntax-only',
        '-Werror=undefined-internal', '-DTSFP_NATIVE_DISPLAY',
        '-I' + str(ROOT / 'src'), '-I' + str(RUNTIME / 'templates/runtime'),
        '-I' + str(RUNTIME / 'src'),
        '-I' + str(RUNTIME / 'src/input'), '-I' + str(RUNTIME / 'src/config'),
        str(repo_file(ROOT, 'src', source))], cwd=tmp_path, env=environment,
        capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr

@pytest.mark.skipif(sys.platform != 'win32', reason='Windows settings persistence adapter')
@pytest.mark.parametrize("mutation", ["fixed", "drain-other-menu"])
def test_fov_settings(tmp_path, mutation):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / 'fov.exe'
    source = ROOT / 'tests/graphics/fov_settings.c'
    if mutation == 'drain-other-menu':
        adapter = (ROOT / 'src/storage/pc_storage_menu.inc').read_text(encoding='utf-8')
        guard = 'if(sd_picker_menu && sd_picker_menu==menu)'
        assert guard in adapter
        (tmp_path / 'pc_storage_menu.inc').write_text(adapter.replace(guard, 'if(sd_picker_menu)'), encoding='utf-8')
        menu = tmp_path / 'pc_display_menu.c'
        menu.write_text((ROOT / 'src/interface/pc_display_menu.c').read_text(encoding='utf-8'), encoding='utf-8')
        fixture = source.read_text(encoding='utf-8').replace('../src/interface/pc_display.c', (ROOT / 'src/interface/pc_display.c').as_posix()).replace('../src/interface/pc_display_menu.c', menu.as_posix())
        source = tmp_path / 'fov_settings.c'
        source.write_text(fixture, encoding='utf-8')
    result = subprocess.run([compiler, '/nologo', '/std:c11', '/O2', '/W3',
        '/I' + str(ROOT / 'src'), '/I' + str(RUNTIME / 'templates/runtime'),
        '/I' + str(RUNTIME / 'src/input'), '/I' + str(RUNTIME / 'src/config'),
        str(source), str(ROOT / 'src/textures/texture_mode.c'),
        '/Fe:' + str(exe), '/link', 'user32.lib'],
        cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe), str(tmp_path / 'display.ini')],
        capture_output=True, text=True, timeout=15)
    if mutation == 'drain-other-menu':
        assert result.returncode != 0
        assert 'picker_result==1' in result.stderr
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS:' in result.stdout
