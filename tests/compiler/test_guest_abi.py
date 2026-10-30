"""Compile the project guest ABI in isolation and in its runtime consumers."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h'


def between(text, first, last):
    start = text.index(first)
    return text[start:text.index(last, start)+len(last)]


def own_header(text):
    return ('#ifndef TSFP_ABI_FIXTURE\n#define TSFP_ABI_FIXTURE\n'
            '#include <stdint.h>\n#include <stddef.h>\n'
            'extern ptrdiff_t g_xbox_mem_offset;\n' +
            between(text, '/* TSFP guest register interface begin.',
                    '/* TSFP guest register interface end. */') + '\n' +
            between(text, '/* TSFP guest primitive support begin.',
                    '/* TSFP partial-register and stack primitives end. */') + '\n#endif\n')


@pytest.mark.parametrize('variant', ['standalone', 'production', 'lost_wrap',
                                     'lost_preservation', 'old_pop', 'shared_registers'])
def test_guest_cells_memory_stack_and_thread_isolation(tmp_path, variant):
    text = HEADER.read_text(encoding='utf-8')
    if variant != 'production':
        text = own_header(text)
    if variant == 'lost_wrap':
        old = '(uintptr_t)(uint32_t)(address)'
        assert old in text
        text = text.replace(old, '(uintptr_t)(address)')
    elif variant == 'lost_preservation':
        old = '(prior & ~(low << shift))'
        assert old in text
        text = text.replace(old, '(prior & 0u)')
    elif variant == 'old_pop':
        old = '*tsfp_stack_cell_ = tsfp_old_stack_ + 4u; \\\n    (destination) = tsfp_popped_;'
        assert old in text
        text = text.replace(old, '(destination) = tsfp_popped_; \\\n    *tsfp_stack_cell_ += 4u;')
    elif variant == 'shared_registers':
        text = '#define RECOMP_TLS\n' + text
    (tmp_path/'under_test.h').write_text(text, encoding='utf-8')
    source = tmp_path/'cells.c'
    source.write_bytes((ROOT/'tests/compiler/guest_abi.c').read_bytes())
    callback = tmp_path/'callback.c'
    callback.write_text('#include "under_test.h"\n'
                        'void cross_unit_callback(void) { ++g_eax; ++g_xmm0.u[0]; ++g_mm7.q; }\n')
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('cells.exe' if os.name == 'nt' else 'cells')
    command = ([compiler, '/nologo', '/std:c11', '/O2', '/WX', str(source), str(callback), f'/Fe:{exe}']
               if os.name == 'nt' else
               [compiler, '-std=c11', '-O2', '-Werror', '-fstrict-aliasing',
                '-fsanitize=undefined', '-fno-sanitize-recover=all', '-pthread',
                str(source), str(callback), '-lm', '-o', str(exe)])
    build = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=90)
    assert build.returncode == 0, build.stdout + build.stderr
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    if variant in ('standalone', 'production'):
        assert run.returncode == 0, run.stdout + run.stderr
        assert 'guest cells, memory, stack and threads passed' in run.stdout
    else:
        assert run.returncode == 1, run.stdout + run.stderr
        assert 'line ' in run.stdout
