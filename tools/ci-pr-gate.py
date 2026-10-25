"""One fail-closed PR result for the selected Windows, Mac and launcher work."""
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
