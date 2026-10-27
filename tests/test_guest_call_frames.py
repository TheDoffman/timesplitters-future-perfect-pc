"""Compile actual call checks and HLE frames with independent state oracles."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler
from test_guest_abi import between

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h'


def isolated(text):
    return '''#include <stdint.h>
#include <stddef.h>
#define RECOMP_TLS
extern uint32_t g_eax,g_ebx,g_esi,g_edi,g_esp;
extern ptrdiff_t g_xbox_mem_offset;
#define MEM32(address) (*(volatile uint32_t *)((uintptr_t)(uint32_t)(address) + (uintptr_t)g_xbox_mem_offset))
''' + between(text, '/* TSFP call checks begin.', '/* TSFP call checks end. */') + '\n' + between(
        text, '/* TSFP HLE frames begin.', '/* TSFP HLE frames end. */')


@pytest.mark.parametrize('enabled', [False, True])
@pytest.mark.parametrize('variant', ['production', 'isolated'])
def test_call_and_frame_contracts(tmp_path, enabled, variant):
    run_contract(tmp_path, enabled, variant)


@pytest.mark.parametrize('variant', ['late_snapshot', 'lost_register', 'wrong_stack', 'lost_restore', 'short_frame'])
def test_mutations_compile_and_fail(tmp_path, variant):
    run_contract(tmp_path, True, variant)


def run_contract(tmp_path, enabled, variant):
    text = HEADER.read_text(encoding='utf-8')
    if variant == 'isolated':
        text = isolated(text)
    mutations = {
        'late_snapshot': ('const tsfp_call_snapshot tsfp_before_call_ = tsfp_capture_call(); \\\n    (target)();',
                          '(target)(); \\\n    const tsfp_call_snapshot tsfp_before_call_ = tsfp_capture_call();'),
        'lost_register': ('g_edi == state->destination', '1'),
        'wrong_stack': ('tsfp_before_call_.stack + 4u', 'tsfp_before_call_.stack + 8u'),
        'lost_restore': ('g_esp = frame->entry + frame->cleanup;', 'g_esp += frame->cleanup;'),
        'short_frame': ('index < RECOMP_HLE_MAX_ARGS', 'index < 20u'),
    }
    if variant in mutations:
        old, new = mutations[variant]
        assert old in text
        text = text.replace(old, new)
    (tmp_path/'under_test.h').write_text(('#define RECOMP_ABI_CHECK\n' if enabled else '') + text, encoding='utf-8')
    source = tmp_path/'calls.c'
    source.write_bytes((ROOT/'tests/guest_call_frames.c').read_bytes())
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('calls.exe' if os.name == 'nt' else 'calls')
    command = ([compiler, '/nologo', '/std:c11', '/O2', '/WX', str(source), f'/Fe:{exe}']
               if os.name == 'nt' else
               [compiler, '-std=c11', '-O2', '-Werror', '-fsanitize=undefined',
                '-fno-sanitize-recover=all', str(source), '-lm', '-o', str(exe)])
    build = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=90)
    assert build.returncode == 0, build.stdout + build.stderr
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    if variant in mutations:
        assert run.returncode == 1 and 'line ' in run.stdout, run.stdout + run.stderr
    else:
        assert run.returncode == 0, run.stdout + run.stderr
        assert '448 ABI cases and 24 HLE frame cases passed' in run.stdout
