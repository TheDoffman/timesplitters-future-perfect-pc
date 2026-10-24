"""Retain the caller depth actually captured by the opt-in CPU sampler."""
import os
from pathlib import Path
import re
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('mutation', [None, 'six-frames', 'hidden-overflow'])
def test_sample_chains(tmp_path, mutation):
    text = (ROOT/'external/ts3FPrecomp/src/kernel/recomp_sample.c').read_text()
    depth = re.search(r'#define SAMPLE_MAX_DEPTH\s+32', text)
    table = re.search(r'#define SAMPLE_CHAIN\s+.*?\n\} SampleChain;', text, re.S)
    function = re.search(r'static int chain_add\(.*?\n\}', text, re.S)
    assert depth and table and function, 'Apply the complete patch stack first'
    source = '\n'.join((depth.group(), table.group(), function.group()))
    if mutation == 'six-frames':
        source = source.replace('#define SAMPLE_CHAIN        SAMPLE_MAX_DEPTH', '#define SAMPLE_CHAIN        6')
    elif mutation == 'hidden-overflow':
        assert 'return 0;' in source
        source = source.replace('return 0;', 'return 1;')
    fixture = tmp_path/'sample_chains.c'
    fixture.write_text((ROOT/'tests/sample_chains.c').read_text().replace('/* PRODUCTION_CHAIN_TABLE */', source))
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('sample.exe' if os.name == 'nt' else 'sample')
    args = ([compiler, '/nologo', '/std:c11', '/W4', str(fixture), '/Fe:'+str(exe)]
            if os.name == 'nt' else [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', str(fixture), '-o', str(exe)])
    subprocess.run(args, cwd=tmp_path, env=env, check=True, capture_output=True, text=True, timeout=60)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    if mutation:
        assert result.returncode != 0
        assert ('used==2' if mutation == 'six-frames' else '!chain_add') in result.stderr
    else:
        assert result.returncode == 0, result.stdout+result.stderr
        assert 'PASS: deep caller keys' in result.stdout
