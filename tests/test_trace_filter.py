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


@pytest.fixture(scope="module")
def trace_quiet_binaries(tmp_path_factory):
    folder = tmp_path_factory.mktemp("trace-quiet")
    text = (ROOT/'external/ts3FPrecomp/src/kernel/recomp_trace.c').read_text()
    assert text.count('if (trace_quiet()) return;') == 2
    text = text.replace('#include "xbox_memory_layout.h"', '')
    # Count entry to original diagnostic helpers, not the new guard itself.
    for name in ('watch_check', 'dump_va_once', 'trace_only_match', 'prof_enabled', 'trace_budget'):
        pattern = rf'(static [^\n]+\b{name}\([^\n]*\)\n\{{)'
        text, count = re.subn(pattern, r'\1\n    ++diagnostic_calls;', text)
        assert count == 1, name
    compiler, env = native_compiler(folder)
    binaries = []
    for mutation in ('none','dispatch','shared-cache'):
        production = text.replace('if (trace_quiet()) return;', '(void)trace_quiet();') if mutation=='dispatch' else text
        if mutation=='shared-cache':
            production,count=re.subn(r'static RECOMP_TLS int (?=(?:s_trace_quiet|quiet)\b)','static int ',production)
            assert count==1
        source = folder/f'trace-{mutation}.c'
        source.write_text((ROOT/'tests/trace_quiet.c').read_text().replace('/* PRODUCTION_TRACE */', production))
        output = folder/(f'trace-{mutation}.exe' if os.name == 'nt' else f'trace-{mutation}')
        args = ([compiler, '/nologo', '/std:c11', '/O2', str(source), '/Fe:'+str(output)]
                if os.name == 'nt' else [compiler, '-std=c11', '-O2', str(source), '-o', str(output)])
        result = subprocess.run(args, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stdout+result.stderr
        binaries.append(output)
    return binaries


@pytest.mark.parametrize('settings,mode,marker', [
    ({'RECOMP_TRACE_BUDGET': '0'}, 'quiet', None),
    ({'RECOMP_TRACE_BUDGET': '-5'}, 'quiet', None),
    ({'RECOMP_TRACE_BUDGET': 'invalid'}, 'quiet', None),
    ({'RECOMP_TRACE_BUDGET': '2'}, 'enabled', '[TRACE]'),
    ({}, 'enabled', '[TRACE]'),
    ({'RECOMP_TRACE_BUDGET': '0', 'RECOMP_TRACE_ONLY': 'alpha', 'RECOMP_TRACE_ONLY_MAX': '2'}, 'enabled', '[ONLY]'),
    ({'RECOMP_TRACE_BUDGET': '0', 'RECOMP_WATCH_VA': '0x100'}, 'enabled', '[WATCH]'),
    ({'RECOMP_TRACE_BUDGET': '0', 'RECOMP_DUMP_VA': '0x100:1'}, 'enabled', '[DUMP]'),
    ({'RECOMP_TRACE_BUDGET': '0', 'RECOMP_TRACE_PROFILE': '1'}, 'profile', '[PROFILE]'),
])
def test_quiet_trace_dispatch(trace_quiet_binaries, settings, mode, marker):
    env = {k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_TRACE_', 'RECOMP_DUMP_', 'RECOMP_WATCH_', 'RECOMP_PROFILE_'))}
    env.update(settings)
    result = subprocess.run([str(trace_quiet_binaries[0]), mode], env=env, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout+result.stderr
    if marker:
        assert marker in result.stderr
        if marker == '[ONLY]': assert result.stderr.count('[ONLY] ->') == 2
    else:
        assert result.stderr == ''


def test_quiet_trace_dispatch_mutation(trace_quiet_binaries):
    env = {k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_TRACE_', 'RECOMP_DUMP_', 'RECOMP_WATCH_', 'RECOMP_PROFILE_'))}
    env['RECOMP_TRACE_BUDGET'] = '0'
    result = subprocess.run([str(trace_quiet_binaries[1]), 'quiet'], env=env, capture_output=True, text=True, timeout=10)
    assert result.returncode != 0 and 'diagnostic_calls==0' in result.stderr


def test_quiet_trace_startup_threads(trace_quiet_binaries):
    env = {k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_TRACE_', 'RECOMP_DUMP_', 'RECOMP_WATCH_', 'RECOMP_PROFILE_'))}
    env['RECOMP_TRACE_BUDGET']='0'
    current=subprocess.run([str(trace_quiet_binaries[0]),'startup-cache'],env=env,capture_output=True,text=True,timeout=10)
    assert current.returncode==0,current.stdout+current.stderr
    assert 'PASS: per-thread startup settings' in current.stdout and '[TRACE]' in current.stderr
    broken=subprocess.run([str(trace_quiet_binaries[2]),'startup-cache'],env=env,capture_output=True,text=True,timeout=10)
    assert broken.returncode!=0 and 'diagnostic_calls>before' in broken.stderr
