"""Original synthetic VM contracts; no upstream fixtures or title assets."""
from pathlib import Path
import os
import subprocess
import sys
import pytest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
sys.path.insert(0, str(ROOT / 'tests/compiler'))
from test_translation_product import _compile


def build(folder, mutation=None):
    for name in ('guest_virtual_memory.c', 'guest_virtual_memory.h',
                 'guest_allocation.c', 'guest_allocation.h',
                 'guest_virtual_memory_contracts.c'):
        path = HERE / name
        if not path.exists(): path = ROOT / 'src/runtime' / name
        text = path.read_text(encoding='utf-8')
        if mutation and mutation[0] == name:
            _, old, new = mutation
            assert text.count(old) == 1
            text = text.replace(old, new)
        (folder / name).write_text(text, encoding='utf-8')
    warnings = ['/W4'] if os.name == 'nt' else ['-Wall', '-Wextra']
    return _compile(folder, folder / 'guest_virtual_memory_contracts.c',
                    extra=[folder / 'guest_virtual_memory.c',
                           folder / 'guest_allocation.c', *warnings])


@pytest.fixture(scope='module')
def executable(tmp_path_factory):
    return build(tmp_path_factory.mktemp('guest-virtual-memory'))


@pytest.mark.parametrize('mode', ['allocate', 'invalid', 'free', 'query', 'protect'])
def test_guest_virtual_contracts(executable, mode):
    run = subprocess.run([str(executable), mode], capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, run.stdout + run.stderr
    assert 'contracts passed' in run.stdout


@pytest.mark.parametrize('mode,mutation', [
    ('allocate', ('guest_virtual_memory.c', 'write_word(size, requested);', 'write_word(size, requested + 1);')),
    ('invalid', ('guest_virtual_memory.c', 'if (!base || !size || !v->allocations) return ARGUMENT;',
                 'if (!base || !size || !v->allocations) return OK;')),
    ('free', ('guest_allocation.c', 'if (!result) result = release(&s->heap, address, OWN_RESERVATION);',
              'if (!result) result = 0;')),
    ('query', ('guest_virtual_memory.c', 'span(v, output, 28, 1)', 'span(v, output, 4, 1)')),
    ('protect', ('guest_virtual_memory.c', 'if (!v->protect(v->host, address, bytes, protection, &previous)) return ACCESS;',
                 '(void)v->protect(v->host, address, bytes, protection, &previous);')),
])
def test_compiled_negative_controls(tmp_path, mode, mutation):
    exe = build(tmp_path, mutation)
    run = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=30)
    assert run.returncode != 0, 'Mutated provider incorrectly passed'
