"""Exercise optional DWM attributes on a real isolated native window."""

from project_paths import source_file as repo_file

from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows DWM adapter')
@pytest.mark.parametrize('source', ['window_theme.c', 'game_ui_theme.c'])
def test_window_theme(tmp_path, source):
    root = Path(__file__).resolve().parents[2]
    compiler, environment = native_compiler(tmp_path)
    executable = tmp_path / 'theme.exe'
    result = subprocess.run([compiler, '/nologo', '/std:c11', '/W3', '/I'+str(root/'src'),
                             str(repo_file(root, 'tests', source)), '/Fe:'+str(executable),
                             '/Fo:'+str(tmp_path/'theme.obj'), 'user32.lib', 'gdi32.lib', 'comctl32.lib'],
                            env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout+result.stderr
    if result.stdout.startswith('SKIP:'):
        pytest.skip(result.stdout.strip())
    assert 'PASS:' in result.stdout
