"""One fail-closed PR result for the selected Windows, Mac and launcher work."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
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

import importlib.util
import os
from pathlib import Path


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


runtime = load('runtime_scope', 'ci-runtime-scope.py')
launcher = load('launcher_scope', 'launcher-compatibility.py')


def gate(plan, transitions, native, release, run_launcher,
         patches_result, native_result, launcher_result):
    runtime.gate(plan, transitions, native, release,
                 patches_result, native_result)
    launcher.gate(plan, run_launcher, launcher_result)
    return 'All selected PR checks passed on their required hosts.'


if __name__ == '__main__':
    names = ('PLAN_RESULT', 'RUN_TRANSITIONS', 'RUN_NATIVE', 'RUN_RELEASE',
             'RUN_LAUNCHER', 'PATCHES_RESULT', 'NATIVE_RESULT', 'LAUNCHER_RESULT')
    print(gate(*(os.environ.get(name, '') for name in names)))
