"""Keep water helpers aligned with rounded guest uploads, including negative controls."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('mutation', ['none', 'truncate', 'ties-away'])
def test_surface_coefficient_rounding(tmp_path, mutation):
    header = (ROOT/'src/graphics/pc_surface_bias.h').read_text(encoding='utf-8')
    needle = 'return (uint32_t)nearbyint((double)scaled);'
    assert header.count(needle) == 1
    if mutation != 'none':
        replacement = ('return (uint32_t)scaled;' if mutation == 'truncate'
                       else 'return (uint32_t)round((double)scaled);')
        header = header.replace(needle, replacement)
    (tmp_path/'pc_surface_bias.h').write_text(header, encoding='utf-8')
    source = tmp_path/'coefficients.c'
    source.write_text((ROOT/'tests/graphics/surface_coefficients.c').read_text(encoding='utf-8'), encoding='utf-8')
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path/('coefficients.exe' if os.name == 'nt' else 'coefficients')
    command = ([compiler, '/nologo', '/std:c11', '/O2', '/WX', str(source), '/Fe:'+str(exe)]
               if os.name == 'nt' else
               [compiler, '-std=c11', '-O2', '-Werror', '-frounding-math', str(source), '-lm', '-o', str(exe)])
    built = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout+built.stderr
    result = subprocess.run([str(exe)], cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=10)
    if mutation == 'none':
        assert result.returncode == 0 and 'PASS:' in result.stdout, result.stdout+result.stderr
    else:
        assert result.returncode != 0 and 'coefficient mismatch' in result.stderr, result.stdout+result.stderr
