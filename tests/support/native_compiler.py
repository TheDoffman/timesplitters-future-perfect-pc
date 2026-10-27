"""Find a native C compiler, including standalone MSVC Build Tools on Windows."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import os
from pathlib import Path
import shutil
import subprocess


def native_compiler(directory):
    if os.name != 'nt':
        environment = dict(os.environ)
        compiler = shutil.which(environment.get('CC', 'cc'))
        assert compiler, 'Native regressions require a C compiler'
        return compiler, _headers(environment)
    # Windows environment names are case-insensitive, Python dict keys are not.
    environment = {key.upper(): value for key, value in os.environ.items()}
    compiler = shutil.which('cl', path=environment.get('PATH'))
    if compiler and environment.get('VSCMD_ARG_TGT_ARCH') == 'x64' and environment.get('INCLUDE') and environment.get('LIB'):
        return compiler, _headers(environment)
    locator = Path(environment.get('PROGRAMFILES(X86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    assert locator.is_file(), 'Install Visual Studio or Build Tools with the MSVC x64 C++ toolchain'
    installation = subprocess.check_output([
        str(locator), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath',
    ], text=True).strip()
    setup = Path(installation) / 'VC/Auxiliary/Build/vcvars64.bat'
    assert installation and setup.is_file(), 'MSVC x64 is required (Visual Studio or standalone Build Tools)'
    script = Path(directory) / 'native-compiler-env.cmd'
    script.write_text(f'@echo off\ncall "{setup}" >nul\nif errorlevel 1 exit /b 1\nset\n', encoding='utf-8')
    output = subprocess.check_output(
        f'cmd.exe /d /u /s /c ""{script}""', encoding='utf-16-le', env=environment,
    )
    environment.update((key.upper(), value) for key, value in
                       (line.split('=', 1) for line in output.splitlines()
                        if '=' in line and not line.startswith('=')))
    compiler = shutil.which('cl', path=environment.get('PATH'))
    assert compiler and environment.get('VSCMD_ARG_TGT_ARCH') == 'x64', 'Could not initialize the MSVC x64 environment'
    return compiler, _headers(environment)


def _headers(environment):
    root = _layout_root
    folders = [root / 'src', root / 'tests']
    folders += sorted(p for area in ('src', 'tests') for p in (root / area).iterdir() if p.is_dir() and not p.name.startswith('_'))
    key = 'INCLUDE' if os.name == 'nt' else 'CPATH'
    environment[key] = os.pathsep.join(filter(None, [environment.get(key, ''), *(str(p) for p in folders)]))
    return environment
