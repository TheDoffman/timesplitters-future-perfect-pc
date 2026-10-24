"""Disabled trace filters must not scan labels at every lifted function entry."""
import os
from pathlib import Path
import re
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('setting,limit,mode,mutation', [
    (None, None, 'disabled', False),
    ('', None, 'disabled', False),
    ('alpha,beta', '2', 'limited', False),
    ('alpha,beta', '0', 'zero', False),
    ('alpha,beta', '-1', 'zero', False),
    (None, None, 'disabled', True),
])
def test_trace_filter(tmp_path, setting, limit, mode, mutation):
    text = (ROOT/'external/ts3FPrecomp/src/kernel/recomp_trace.c').read_text(encoding='utf-8')
    match = re.search(r'static int trace_only_match\(const char \*name\)\n\{.*?\n\}', text, re.S)
    assert match, 'Apply the full patch stack before testing'
    body = match.group()
    if mutation:
        assert 'size_t n;' in body and '    n = strlen(name);' in body
        body = body.replace('    n = strlen(name);', '').replace('size_t n;', 'size_t n = strlen(name);')
    source = tmp_path/'filter.c'
    source.write_text((ROOT/'tests/trace_filter.c').read_text().replace('/* PRODUCTION_TRACE_FILTER */', body), encoding='utf-8')
    compiler, env = native_compiler(tmp_path)
    output = tmp_path/('filter.exe' if os.name=='nt' else 'filter')
    args = ([compiler, '/nologo', '/std:c11', '/W4', str(source), '/Fe:'+str(output)]
            if os.name=='nt' else [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(output)])
    subprocess.run(args, cwd=tmp_path, env=env, check=True, capture_output=True, text=True, timeout=60)
    run_env = {k:v for k,v in os.environ.items() if k not in ('RECOMP_TRACE_ONLY','RECOMP_TRACE_ONLY_MAX')}
    if setting is not None:run_env['RECOMP_TRACE_ONLY']=setting
    if limit is not None:run_env['RECOMP_TRACE_ONLY_MAX']=limit
    result = subprocess.run([str(output), mode], env=run_env, capture_output=True, text=True, timeout=10)
    if mutation:
        assert result.returncode != 0 and 'scans==0' in result.stderr
    else:
        assert result.returncode == 0, result.stdout+result.stderr
