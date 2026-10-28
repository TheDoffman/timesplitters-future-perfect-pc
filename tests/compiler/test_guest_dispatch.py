"""Execute dispatch against an independent state/event oracle on both hosts."""
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest
from native_compiler import native_compiler
from test_guest_abi import between

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h'


def test_generated_dispatch_table_includes_production_header(tmp_path):
    """The table TU suppresses public dispatch declarations until after include."""
    sys.path.insert(0, str(ROOT/'tools'))
    from translation_batch import BatchTranslator
    from test_translation_product import _compile
    (tmp_path/'recomp_types.h').write_bytes(HEADER.read_bytes())
    (tmp_path/'functions.h').write_text('#include "recomp_types.h"\nvoid guest(void);\n', encoding='utf-8')
    code = BatchTranslator._dispatch_source(SimpleNamespace(title='Dispatch include contract'),
                                            [(0x1000, 'guest', None)], 'functions.h')
    code += '''
static unsigned calls;
void guest(void) { ++calls; }
int main(void) {
    if (recomp_lookup(0x1000) != guest || recomp_lookup(0x1001)) return 1;
    recomp_lookup(0x1000)();
    return calls != 1;
}
'''
    source = tmp_path/'table.c'
    source.write_text(code, encoding='utf-8')
    result = subprocess.run([str(_compile(tmp_path, source))], capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr


def isolated(text):
    """Only dispatch, existing ABI diagnostics, and their declared inputs."""
    prelude = '''#include <stdint.h>
#define RECOMP_TLS
extern uint32_t g_eax,g_ebx,g_esi,g_edi,g_esp;
extern uint32_t g_icall_saved_esp,g_icall_dispatch_form;
extern uint32_t g_xbox_code_lo,g_xbox_code_hi;
extern volatile uint32_t g_icall_trace[16],g_icall_trace_idx;
extern volatile uint64_t g_icall_count;
#define eax g_eax
#define ICALL_TRACE_SIZE 16
#define RECOMP_ICALL_IS_CODE(v) ((v)>=0xFE000000u || !g_xbox_code_hi || ((v)>=g_xbox_code_lo && (v)<g_xbox_code_hi))
typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup_manual(uint32_t);
recomp_func_t recomp_lookup(uint32_t);
recomp_func_t recomp_lookup_kernel(uint32_t);
void recomp_icall_fail_log(uint32_t);
void recomp_icall_not_code_log(uint32_t,uint32_t);
#include "tsfp_branch_feedback.h"
'''
    abi_start = text.index('#ifdef RECOMP_ABI_CHECK', text.index('recomp_func_t recomp_lookup_manual'))
    abi = text[abi_start:text.index('\n#endif', abi_start)+len('\n#endif')]
    support = between(text, '/* TSFP indirect dispatch support begin.',
                      '/* TSFP indirect dispatch support end. */')
    start = text.index('#define RECOMP_ICALL(xbox_va)')
    plain = text[start:text.index('\n\n', start)]
    wrappers = between(text, '/* TSFP indirect dispatch wrappers begin.',
                       '/* TSFP indirect dispatch wrappers end. */')
    return '\n'.join((prelude, abi, support, plain, wrappers))


@pytest.mark.parametrize('abi,feedback', [(0,0), (0,1), (1,0), (1,1)])
@pytest.mark.parametrize('variant', ['production', 'isolated'])
def test_dispatch_state_and_callback_order(tmp_path, variant, abi, feedback):
    run_contract(tmp_path, variant, abi, feedback)


@pytest.mark.parametrize('variant', ['lookup_priority', 'early_recovery', 'missing_trace', 'tail_form'])
def test_dispatch_mutations_compile_but_fail_contract(tmp_path, variant):
    run_contract(tmp_path, variant, 1, 1)


def run_contract(tmp_path, variant, abi, feedback):
    text = HEADER.read_text(encoding='utf-8')
    if variant == 'isolated':
        text = isolated(text)
    mutations = {
        'lookup_priority': ('tsfp_guest_entry_t resolved = recomp_lookup_manual(target);',
                            'tsfp_guest_entry_t resolved = recomp_lookup(target);'),
        'early_recovery': ('recomp_icall_fail_log(target);', 'g_esp += 4; recomp_icall_fail_log(target);'),
        'missing_trace': ('++g_icall_trace_idx;', '(void)g_icall_trace_idx;'),
        'tail_form': ('tsfp_invoke_guest_target(tsfp_tail_target_, 2u)',
                      'tsfp_invoke_guest_target(tsfp_tail_target_, 1u)'),
    }
    if variant in mutations:
        old, new = mutations[variant]
        assert text.count(old) == 1
        text = text.replace(old, new)
    definitions = ('#define RECOMP_ABI_CHECK\n' if abi else '')
    definitions += '#define RECOMP_ICALL_FEEDBACK\n' if feedback else ''
    (tmp_path/'under_test.h').write_text(definitions + text, encoding='utf-8')
    # Instrument only the existing feedback boundary; the dispatcher and ABI
    # check are compiled verbatim. Real recorder coverage remains in its suite.
    (tmp_path/'tsfp_branch_feedback.h').write_text('''#include <stdint.h>
void tsfp_feedback_guard(unsigned);
void observe(uint32_t,unsigned);
void observe_site(uint32_t,uint32_t);
#define RECOMP_ICALL_SEEN_RESOLVED 1u
#define RECOMP_ICALL_SEEN_UNRESOLVED 2u
#ifdef RECOMP_ICALL_FEEDBACK
#define RECOMP_ICALL_OBSERVE(v,f) observe(v,f)
#define RECOMP_ICALL_OBSERVE_SITE(s,v) observe_site(s,v)
#else
#define RECOMP_ICALL_OBSERVE(v,f) ((void)0)
#define RECOMP_ICALL_OBSERVE_SITE(s,v) ((void)0)
#endif
''', encoding='utf-8')
    source = tmp_path/'dispatch.c'
    source.write_bytes((ROOT/'tests/compiler/guest_dispatch.c').read_bytes())
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('dispatch.exe' if os.name == 'nt' else 'dispatch')
    command = ([compiler, '/nologo', '/std:c11', '/O2', '/WX', str(source), f'/Fe:{exe}']
               if os.name == 'nt' else
               [compiler, '-std=c11', '-O2', '-Werror', '-fsanitize=undefined',
                '-fno-sanitize-recover=all', str(source), '-lm', '-o', str(exe)])
    build = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=90)
    assert build.returncode == 0, build.stdout + build.stderr
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    if variant in mutations:
        assert run.returncode == 1, run.stdout + run.stderr
        assert 'line ' in run.stdout or 'events ' in run.stdout
    else:
        assert run.returncode == 0, run.stdout + run.stderr
        assert '4096 dispatch state/event cases passed' in run.stdout
