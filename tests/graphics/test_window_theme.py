"""Exercise Win32 UI adapters with isolated windows and cursor API doubles."""
from project_paths import source_file as repo_file
from pathlib import Path
import subprocess
import sys
import os
import xml.etree.ElementTree as ET

import pytest
from native_compiler import native_compiler


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows DWM adapter')
@pytest.mark.parametrize('source', ['window_theme.c', 'game_ui_theme.c', 'pc_mouse_capture.c'])
def test_window_theme(tmp_path, source):
    root = Path(__file__).resolve().parents[2]
    compiler, environment = native_compiler(tmp_path)
    executable = tmp_path / 'theme.exe'
    result = subprocess.run([compiler, '/nologo', '/std:c11', '/W3', '/I'+str(root/'src'), '/I'+str(root/'src/input'),
                             str(repo_file(root, 'tests', source)), '/Fe:'+str(executable),
                             '/Fo:'+str(tmp_path/'theme.obj'), 'user32.lib', 'gdi32.lib', 'comctl32.lib'],
                            env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout+result.stderr
    if result.stdout.startswith('SKIP:'):
        pytest.skip(result.stdout.strip())
    assert 'PASS:' in result.stdout


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows executable manifest')
@pytest.mark.parametrize('aware', [False, True])
def test_game_manifest_sets_process_dpi_without_compatibility_overrides(tmp_path, aware):
    import cmake
    import ninja
    import pefile

    root = Path(__file__).resolve().parents[2]
    _, environment = native_compiler(tmp_path)
    environment = dict(environment)
    environment.pop('__COMPAT_LAYER', None)
    source = tmp_path/'game with spaces'
    source.mkdir()
    (source/'main.c').write_text('''#include <windows.h>
#include <shellscalingapi.h>
#include <stdio.h>
#pragma comment(linker, "/manifestdependency:\\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\\"")
int main(void) {
    PROCESS_DPI_AWARENESS awareness;
    if (FAILED(GetProcessDpiAwareness(NULL, &awareness))) return 2;
    printf("dpi=%d\\n", (int)awareness);
    return 0;
}
''')
    patch = (root/'patches/platform/2130-game-dpi-awareness.patch').read_text()
    snippet = '\n'.join(line[1:] for line in patch.splitlines()
                        if line.startswith('+') and not line.startswith('+++'))
    (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\n'
        'project(game_dpi_probe C)\nadd_executable(${PROJECT_NAME} main.c)\n'
        'target_link_libraries(${PROJECT_NAME} PRIVATE shcore)\n'+snippet+'\n')
    binary = source/'build'
    cmake_exe = str(Path(cmake.CMAKE_BIN_DIR)/'cmake.exe')
    command = [cmake_exe, '-S', str(source), '-B', str(binary), '-G', 'Ninja',
               '-DCMAKE_MAKE_PROGRAM='+str(Path(ninja.BIN_DIR)/'ninja.exe'),
               '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_TRY_COMPILE_CONFIGURATION=Release']
    if aware:
        command.append('-DTSFP_GAME_MANIFEST='+str(root/'config/build/windows-game.manifest'))
    for args in (command, [cmake_exe, '--build', str(binary)]):
        result = subprocess.run(args, env=environment, capture_output=True, text=True,
                                errors='replace', timeout=60)
        assert result.returncode == 0, result.stdout+result.stderr
    executable = binary/'game_dpi_probe.exe'
    with pefile.PE(str(executable)) as pe:
        resource = next(entry for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries if entry.id == 24)
        assert len(resource.directory.entries) == 1 and resource.directory.entries[0].id == 1
        data = resource.directory.entries[0].directory.entries[0].data.struct
        manifest = ET.fromstring(pe.get_data(data.OffsetToData, data.Size))
    ns = {'a': 'urn:schemas-microsoft-com:asm.v1', 'v3': 'urn:schemas-microsoft-com:asm.v3',
          'old': 'http://schemas.microsoft.com/SMI/2005/WindowsSettings',
          'dpi': 'http://schemas.microsoft.com/SMI/2016/WindowsSettings'}
    level = manifest.find('.//v3:requestedExecutionLevel', ns)
    assert level.attrib['level'] == 'asInvoker'
    dependency = manifest.find('.//a:dependentAssembly/a:assemblyIdentity', ns)
    assert dependency.attrib['name'] == 'Microsoft.Windows.Common-Controls'
    if aware:
        assert manifest.find('.//old:dpiAware', ns).text == 'true'
        assert manifest.find('.//dpi:dpiAwareness', ns).text == 'System'
    result = subprocess.run([str(executable)], env=environment, capture_output=True,
                            text=True, timeout=10)
    assert result.returncode == 0, result.stdout+result.stderr
    assert result.stdout.strip() == 'dpi='+str(int(aware))
