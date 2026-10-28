"""Thread storage contracts with independent byte layouts and native workers."""
import os
import hashlib
from pathlib import Path
import subprocess
import sys

import pytest

HERE = Path(__file__).parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
sys.path.insert(0, str(ROOT / 'tests/compiler'))
from test_translation_product import _compile


def compile_storage(folder, mutation=None, harness_source=None):
    provider = HERE if (HERE / 'guest_thread_memory.c').exists() else ROOT / 'src/runtime'
    source = (provider / 'guest_thread_memory.c').read_text(encoding='utf-8')
    if mutation:
        old, new = mutation
        assert source.count(old) == 1
        source = source.replace(old, new)
    (folder / 'guest_thread_memory.c').write_text(source, encoding='utf-8')
    (folder / 'guest_thread_memory.h').write_bytes((provider / 'guest_thread_memory.h').read_bytes())
    for name in ('guest_allocation.c', 'guest_allocation.h', 'guest_image.c', 'guest_image.h'):
        (folder / name).write_bytes((ROOT / 'src/runtime' / name).read_bytes())
    harness = folder / 'guest_thread_memory_contracts.c'
    harness.write_bytes((HERE / harness.name).read_bytes())
    if harness_source:
        harness = folder / 'integration.c'
        harness.write_text(harness_source, encoding='utf-8')
    warnings = ['/W4'] if os.name == 'nt' else ['-Wall', '-Wextra', '-pthread']
    return _compile(folder, harness, extra=[folder / 'guest_thread_memory.c',
        folder / 'guest_allocation.c', folder / 'guest_image.c', *warnings])


@pytest.fixture(scope='module')
def storage(tmp_path_factory):
    return compile_storage(tmp_path_factory.mktemp('thread-memory'))


@pytest.mark.parametrize('mode', ['main', 'private', 'failure', 'stacks', 'concurrent'])
def test_thread_storage_contracts(storage, mode):
    run = subprocess.run([str(storage), mode], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, run.stdout + run.stderr
    assert 'passed' in run.stdout


def test_supported_us_thread_bootstrap(storage):
    path = ROOT / 'private/game/default.xbe'
    if not path.exists(): pytest.skip('Private supported executable unavailable')
    assert hashlib.sha256(path.read_bytes()).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    run = subprocess.run([str(storage), 'us', str(path)], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, run.stdout + run.stderr
    assert 'supported US bootstrap passed' in run.stdout


@pytest.mark.parametrize('mode,mutation', [
    ('main', ('write32(ram + MAIN_TIB + 4, tls + total);', 'write32(ram + MAIN_TIB + 4, tls);')),
    ('private', ('write32(s->ram + tls, data);', '(void)data; write32(s->ram + tls, s->low_shift + DATA_OFFSET);')),
    ('private', ('++s->next_id;', 's->next_id += 0;')),
    ('stacks', ('if (s->stacks[i] != top) continue;', 'if (!s->stacks[i]) continue;')),
    ('stacks', ('if (s->worker_mask & bit) continue;', 'if (0) continue;')),
])
def test_compiled_thread_storage_negative_controls(tmp_path, mode, mutation):
    exe = compile_storage(tmp_path, mutation)
    run = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=60)
    assert run.returncode != 0, 'Broken provider unexpectedly passed its runtime contract'


def test_actual_thread_memory_adapters_and_failure_cleanup(tmp_path):
    kernel = HERE / 'integration/src/kernel'
    if not kernel.exists(): kernel = ROOT / 'external/ts3FPrecomp/src/kernel'
    layout = (kernel / 'xbox_memory_layout.c').read_text(encoding='utf-8')
    bridge = (kernel / 'kernel_bridge.c').read_text(encoding='utf-8')
    threads = (kernel / 'kernel_thread.c').read_text(encoding='utf-8')
    adapters = layout[layout.index('/* Project thread-memory ABI adapters. */'):
                      layout.index('/* End project thread-memory ABI adapters. */')]
    initialization = layout[layout.index('    /* Project main-thread storage is ready'):
                            layout.index('    fprintf(stderr, "  Project allocations: heap ')]
    spawn = bridge[bridge.index('static HANDLE bridge_spawn_thread('):
                   bridge.index('/* Two ways a title expects its first PsCreateSystemThreadEx')]
    timer_failure = bridge[bridge.index('static DWORD WINAPI kernel_timer_thread('):
                           bridge.index('    xbox_NameCurrentThread(L"xbox timer/vblank");')]
    assert layout.index('if (!xbox_AllocationInit())') < layout.index(initialization) < layout.index('xbox_Nv2aAckStart();')
    shutdown = layout[layout.index('void xbox_MemoryLayoutShutdown(void)'):]
    assert shutdown.index('tsfp_thread_memory_stop(') < shutdown.index('tsfp_allocations_stop(') < shutdown.index('tsfp_guest_map_stop(')
    for obsolete in ('g_next_guest_thread_id', 'g_tls_template_va', 'g_thread_stacks_used', 'g_worker_stack_used'):
        assert obsolete not in layout + threads
    assert 'guest_thread_memory.c' in (kernel / 'CMakeLists.txt').read_text(encoding='utf-8')
    source = '''#define main provider_contract_main
#include "guest_thread_memory_contracts.c"
#undef main
#ifndef _WIN32
typedef void *HANDLE;
typedef void *LPVOID;
typedef unsigned long DWORD;
#define WINAPI
#endif
typedef void (*recomp_func_t)(void);
#define g_guest_threads state
#define g_guest_allocations allocations
static uint32_t g_fs_base, g_esp;
static int shutdown_calls;
#define XBOX_TIB_MAIN 0x1000u
#define XBOX_STACK_TOP (state.low_shift+0xf7fff0u)
#define XBOX_WORKER_STACK_TOP(n) (state.low_shift+0x780000u+262144u*((uint32_t)(n)+1u)-16u)
#ifndef FALSE
#define FALSE 0
#endif
static void xbox_MemoryLayoutShutdown(void) { ++shutdown_calls; }
''' + adapters + '''
static int integration_init(void) {
    const tsfp_image_info image = info;
    uint32_t image_low_shift=0x1a0000, image_tls_total=20;
''' + initialization + '''
    return 1;
}
struct bridge_thread_start { recomp_func_t fn; uint32_t ctx1, ctx2, stack_top; };
static int fail_malloc, fail_create, descriptor_frees, recorded;
static void *published;
static void routine(void) { }
static DWORD WINAPI bridge_thread_main(LPVOID parameter) { (void)parameter; return 0; }
static void *descriptor_allocate(size_t size) { return fail_malloc ? NULL : malloc(size); }
static void descriptor_release(void *p) { ++descriptor_frees; free(p); }
static void xbox_set_game_thread(void *handle) { assert(handle==(void *)(uintptr_t)0x1234); ++recorded; }
static HANDLE create_stub(void *attributes, size_t stack, DWORD (WINAPI *entry)(LPVOID),
                           void *parameter, DWORD flags, void *id) {
    struct bridge_thread_start *s=parameter;
    assert(!attributes && !stack && entry==bridge_thread_main && !flags && !id);
    assert(s->fn==routine && s->ctx1==3 && s->ctx2==7);
    if (fail_create) return NULL;
    published=parameter; return (HANDLE)(uintptr_t)0x1234;
}
#define malloc descriptor_allocate
#define free descriptor_release
#define CreateThread create_stub
''' + spawn + '''
#undef CreateThread
#undef free
#undef malloc
''' + timer_failure + '''
    return 0; /* Only actual early-failure paths are executed in this probe. */
}
int main(void) {
    uint32_t top, tib;
    ram=malloc(RAM_SIZE); expected=malloc(RAM_SIZE); assert(ram && expected);
    setup(16,0,1,0x1a0000);
    assert(integration_init());
    assert(!shutdown_calls && g_fs_base==0x1000 && g_esp==0x111fff0);
    assert(xbox_CurrentThreadObject()==0x900000);
    tib=xbox_AllocThreadTib(); assert(tib); g_fs_base=tib;
    assert(xbox_CurrentThreadObject()==word(tib+0x28));
    top=xbox_AllocThreadStack(); assert(top); fail_malloc=1;
    assert(!bridge_spawn_thread(routine,3,7,top));
    assert(!tsfp_allocation_remaining(&allocations,top) && !descriptor_frees);
    top=xbox_AllocThreadStack(); assert(top); fail_malloc=0; fail_create=1;
    assert(!bridge_spawn_thread(routine,3,7,top));
    assert(!tsfp_allocation_remaining(&allocations,top) && descriptor_frees==1);
    top=xbox_AllocThreadStack(); assert(top); fail_create=0;
    assert(bridge_spawn_thread(routine,3,7,top));
    assert(recorded==1 && descriptor_frees==1 && tsfp_allocation_remaining(&allocations,top)==16);
    free(published); xbox_FreeThreadStack(top);
    assert(!tsfp_allocation_remaining(&allocations,top));
    setup(0,0,0,0); assert(tsfp_thread_memory_init(&state,&allocations,&info));
    assert(kernel_timer_thread(NULL)==0 && !state.worker_mask);
    for (int i=0;i<16;++i) assert(xbox_worker_stack_alloc()==i);
    assert(kernel_timer_thread(NULL)==0 && state.worker_mask==0xffff);
    for (int i=0;i<16;++i) xbox_worker_stack_free(i);
    assert(!state.worker_mask);
    free(expected); free(ram); puts("actual thread adapters and failed-host-create/timer cleanup passed");
    return 0;
}
'''
    exe = compile_storage(tmp_path, harness_source=source)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, run.stdout + run.stderr
    assert 'passed' in run.stdout
