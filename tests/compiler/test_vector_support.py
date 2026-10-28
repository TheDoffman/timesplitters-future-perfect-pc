"""Bit-level constructor contracts and removal of obsolete SIMD runtime helpers."""
import os
from pathlib import Path
import re
import subprocess

import pytest
from native_compiler import native_compiler
from test_guest_abi import between

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h'


def test_vector_operations_have_no_inherited_runtime_layer():
    text = HEADER.read_text(encoding='utf-8')
    assert set(re.findall(r'\b(?:XMM|MMX)_[A-Z0-9_]+\b', text)) == {'XMM_SCALAR'}
    assert not re.search(r'\brecomp_(?:xmm|sat)_\w+', text)


@pytest.mark.parametrize('variant', ['production', 'generated', 'isolated', 'lost_bits', 'dirty_upper'])
def test_scalar_constructor(tmp_path, variant):
    text = HEADER.read_text(encoding='utf-8')
    if variant == 'isolated':
        text = ('#include <stdint.h>\n#include <stddef.h>\n#include <string.h>\n' +
                between(text, '/* TSFP guest register interface begin.',
                        '/* TSFP guest register interface end. */') + '\n' +
                between(text, '/* TSFP scalar vector construction begin.',
                        '/* TSFP scalar vector construction end. */'))
    if variant == 'generated':
        text = '#define RECOMP_GENERATED_CODE\n' + text
    mutations = {
        'lost_bits': ('memcpy(&lanes.u[0], &value, sizeof(value));', 'lanes.u[0] = 0u;'),
        'dirty_upper': ('lanes.u[3] = 0u;', 'lanes.u[3] = 1u;'),
    }
    if variant in mutations:
        old, new = mutations[variant]
        assert old in text
        text = text.replace(old, new)
    (tmp_path/'under_test.h').write_text(text, encoding='utf-8')
    source = tmp_path/'vector.c'
    source.write_bytes((ROOT/'tests/compiler/vector_support.c').read_bytes())
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('vector.exe' if os.name == 'nt' else 'vector')
    command = ([compiler, '/nologo', '/std:c11', '/O2', '/WX', str(source), f'/Fe:{exe}']
               if os.name == 'nt' else
               [compiler, '-std=c11', '-O2', '-Werror', '-fstrict-aliasing',
                '-fsanitize=undefined', '-fno-sanitize-recover=all', str(source), '-lm', '-o', str(exe)])
    built = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=90)
    assert built.returncode == 0, built.stdout + built.stderr
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    if variant in mutations:
        assert run.returncode == 1 and 'vector mismatch' in run.stdout, run.stdout + run.stderr
    else:
        assert run.returncode == 0, run.stdout + run.stderr
        assert '131084 vector cases passed' in run.stdout
