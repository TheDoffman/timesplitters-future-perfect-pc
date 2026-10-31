"""Native allocation ownership against an independent byte occupancy model."""
import os
from pathlib import Path
import random
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
if ROOT.name == 'logs':
    ROOT = ROOT.parent
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
sys.path.insert(0, str(ROOT / 'tests/compiler'))
from test_translation_product import _compile

HERE = Path(__file__).parent
PROVIDER = HERE if (HERE / 'guest_allocation.c').exists() else ROOT / 'src/runtime'
BASE = 0x80000000

def compile_provider(folder, mutation=None, harness='guest_allocation_runtime.c'):
    source = (PROVIDER / 'guest_allocation.c').read_text(encoding='utf-8')
    if mutation:
        before, after = mutation
        assert source.count(before) == 1
        source = source.replace(before, after)
    (folder / 'guest_allocation.c').write_text(source, encoding='utf-8')
    (folder / 'guest_allocation.h').write_bytes((PROVIDER / 'guest_allocation.h').read_bytes())
    test = folder / harness
    test.write_bytes((HERE / harness).read_bytes())
    warnings = ['/W4'] if os.name == 'nt' else ['-Wall', '-Wextra', '-pthread']
    return _compile(folder, test, extra=[folder / 'guest_allocation.c', *warnings])

@pytest.fixture(scope='module')
def allocator(tmp_path_factory):
    return compile_provider(tmp_path_factory.mktemp('guest-allocation'))

class Model:
    """Byte occupancy determines fit; allocation records only answer queries."""
    def __init__(self, heap_capacity=1024, physical_capacity=128):
        self.ram = bytearray([0xa5]) * 65536
        self.physical = bytearray([0xb6]) * 131072
        self.reset(heap_capacity, physical_capacity)

    def reset(self, hn, pn):
        self.active = True
        self.cap = {'H': hn, 'C': pn}
        self.heap = {}
        self.blocks = {}
        self.used_heap = bytearray(65536)
        self.used_physical = bytearray(131072)
        self.used_physical[4096:8192] = bytes([1]) * 4096

    def snapshot(self):
        return bytes(self.ram + self.physical)

    def view(self, result):
        if not self.active:
            return [result, 0]
        extent = max([16384] + [(a - BASE) // 4096 * 4096 + r[1]
                               for a, r in self.blocks.items()])
        values = [result, extent]
        for a, (size, _, _) in sorted(self.blocks.items()):
            values += [a, size]
        return values

    def step(self, op, a=0, b=0, low=None, high=None):
        if op == 'D': return self.view(0)
        if op in ('W', 'V'):
            (self.ram if op == 'W' else self.physical)[a] = b & 255
            return self.view(0)
        if op == 'S':
            self.active = False
            return self.view(0)
        if op == 'N':
            if not a or not b: return self.view(0)
            self.reset(a, b)
            return self.view(1)
        if not self.active: return self.view(0)
        if op == 'Q':
            for address, (size, _, kind) in [*self.heap.items(), *self.blocks.items()]:
                if kind != 'R' and address <= a < address + size:
                    return self.view(address + size - a)
            return self.view(0)
        if op in ('F', 'G'):
            records, bitmap, origin = ((self.heap, self.used_heap, 0) if op == 'F' else
                                       (self.blocks, self.used_physical, BASE))
            if a not in records or records[a][2] == 'R': return self.view(0)
            size, span, _ = records.pop(a)
            first = a - origin if op == 'F' else (a - origin) // 4096 * 4096
            bitmap[first:first+span] = bytes(span)
            return self.view(1)
        physical = op in ('C', 'P', 'B')
        records = self.blocks if physical else self.heap
        if len(records) + int(physical) >= self.cap['C' if physical else 'H']:
            return self.view(0)
        bitmap = self.used_physical if physical else self.used_heap
        memory = self.physical if physical else self.ram
        origin = BASE if physical else 0
        limit = 0x1e000 if physical else 65536
        if op == 'P':
            address, size = a, b
            first = (address - BASE) // 4096 * 4096
            last = ((address - BASE + max(size, 1) + 4095) // 4096) * 4096
            if first < 0 or last > limit or any(bitmap[first:last]): return self.view(0)
        else:
            size = max(a, 16) if op == 'H' else a
            if op == 'R' and not size: return self.view(0)
            alignment = max(b, 4096 if physical else 4) if op != 'R' else (b or 4096)
            if alignment & (alignment - 1): return self.view(0)
            span = (max(size, 1) + 4095) // 4096 * 4096 if physical else size
            floor = 16384 if physical else (32768 if op == 'R' else 256)
            if op == 'B':
                if low > high: return self.view(0)
                floor = max(floor, low-origin)
            first = ((origin + floor + alignment - 1) // alignment) * alignment - origin
            # Each occupied byte rules out a starting address. Independent of
            # the provider's sorted record/gap walk and metadata insertion.
            while first + span <= limit and any(bitmap[first:first+span]):
                first += alignment
            last = first + span
            if last > limit: return self.view(0)
            address = origin + first
            if op == 'B' and address + max(size, 1) - 1 > high: return self.view(0)
        span = last - first
        bitmap[first:last] = bytes([1]) * span
        records[address] = (size, span, op)
        if op != 'R': memory[address-origin:address-origin+size] = bytes(size)
        return self.view(address)

def run_model(exe, folder, commands, hn=1024, pn=128):
    model = Model(hn, pn)
    expected, snapshots = [], bytearray()
    for command in commands:
        expected.append(model.step(*command))
        if command[0] == 'D': snapshots.extend(model.snapshot())
    dump = folder / 'memory.bin'
    run = subprocess.run([str(exe), str(dump), str(hn), str(pn)],
        input=''.join(' '.join(map(str, command))+'\n' for command in commands),
        capture_output=True, text=True, timeout=45)
    assert run.returncode == 0, run.stdout + run.stderr
    actual = [list(map(int, line.split())) for line in run.stdout.splitlines()]
    assert len(actual) == len(expected)
    for i, (got, wanted) in enumerate(zip(actual, expected)):
        assert got == wanted, (i, commands[i], got, wanted)
    actual_memory = dump.read_bytes()
    if actual_memory != snapshots:
        first = next((i for i, (a, b) in enumerate(zip(actual_memory, snapshots)) if a != b),
                     min(len(actual_memory), len(snapshots)))
        raise AssertionError(f'Allocation memory differs at byte {first}; '
                             f'actual length {len(actual_memory)}, expected {len(snapshots)}')

def test_sizes_alignment_queries_and_reuse(allocator, tmp_path):
    run_model(allocator, tmp_path, [
        ('H', 0, 0), ('H', 17, 16), ('H', 19, 8), ('D', 0, 0),
        ('Q', 256, 0), ('Q', 271, 0), ('Q', 272, 0), ('Q', 288, 0),
        ('F', 257, 0), ('F', 256, 0), ('F', 256, 0),
        ('W', 258, 77), ('H', 9, 4), ('D', 0, 0),
        ('C', 0, 0), ('C', 4097, 16384), ('C', 4095, 4096),
        ('Q', BASE+32768, 0), ('Q', BASE+32768+4096, 0),
        ('Q', BASE+32768+4097, 0), ('G', BASE+32768+1, 0),
        ('G', BASE+32768, 0), ('C', 8192, 4096), ('D', 0, 0)])

def test_shared_reservation_ownership_and_nonzeroing(allocator, tmp_path):
    run_model(allocator, tmp_path, [
        ('R', 4096, 0), ('H', 40000, 4096), ('H', 32000, 16),
        ('R', 5000, 16), ('H', 16, 4), ('Q', 32768, 0), ('F', 32768, 0),
        ('D', 0, 0), ('F', 256, 0), ('H', 32768, 16), ('D', 0, 0)])

def test_fixed_physical_ownership_protection_and_tail(allocator, tmp_path):
    run_model(allocator, tmp_path, [
        ('P', BASE+0x610, 3), ('P', BASE+0x611, 1),
        ('P', BASE+0x1000, 1), ('P', BASE+0xfff, 2),
        ('P', BASE+0x13fff, 2), ('C', 0x10000, 4096),
        ('Q', BASE+0x14000, 0), ('Q', BASE+0x14001, 0),
        ('G', BASE+0x13000, 0), ('G', BASE+0x13fff, 0),
        ('P', BASE+0x1dfff, 1), ('P', BASE+0x1e000, 0),
        ('P', BASE-1, 1), ('P', 0xffffffff, 0xffffffff),
        ('D', 0, 0), ('G', BASE+0x610, 0), ('D', 0, 0)])

@pytest.mark.parametrize('op', ['H', 'R', 'C'])
def test_overflow_invalid_alignment_and_failure_atomicity(allocator, tmp_path, op):
    commands = [(op, 16, 0), ('D', 0, 0)]
    for size, alignment in [(0xffffffff, 0), (0x80000000, 4096),
                            (32, 8193), (32, 0x80000000), (65536, 4096)]:
        commands += [(op, size, alignment), ('D', 0, 0)]
    run_model(allocator, tmp_path, commands)

def test_metadata_capacity_release_and_restart(allocator, tmp_path):
    run_model(allocator, tmp_path, [
        ('H', 16, 0), ('R', 16, 0), ('H', 16, 0),
        ('C', 0, 0), ('P', BASE+0x8000, 2), ('C', 16, 0), ('D', 0, 0),
        ('F', 256, 0), ('H', 32, 0), ('G', BASE+0x4000, 0), ('C', 32, 0),
        ('S', 0, 0), ('H', 16, 0), ('P', BASE+0x5000, 16), ('Q', 256, 0),
        ('N', 0, 3), ('H', 16, 0), ('D', 0, 0),
        ('N', 2, 3), ('H', 16, 0), ('C', 16, 0), ('D', 0, 0)], hn=2, pn=3)

@pytest.mark.parametrize('seed', [1, 19, 713])
def test_randomized_fragmentation_against_byte_occupancy(allocator, tmp_path, seed):
    rng = random.Random(seed)
    model = Model(48, 18)
    commands = []
    for i in range(700):
        op = rng.choices(['H', 'R', 'C', 'P', 'F', 'G', 'Q', 'B'], [5, 1, 3, 1, 4, 3, 3, 2])[0]
        if op in ('H', 'R', 'C'):
            command = (op, rng.choice([0, 1, 15, 17, 63, 255, 1000, 4095, 4096, 8193, 0xffffffff]),
                       rng.choice([0, 4, 16, 64, 4096, 8192, 16384, 8193]))
        elif op == 'P':
            command = (op, BASE + rng.randrange(0x20000), rng.choice([0, 1, 111, 4096, 8192]))
        elif op == 'B':
            lo, hi = sorted([BASE+rng.randrange(0x22000), BASE+rng.randrange(0x22000)])
            command = (op, rng.choice([0, 16, 255, 4095, 8193]), rng.choice([0, 4096, 8192]), lo, hi)
        else:
            choices = list(model.heap if op == 'F' else model.blocks)
            if op == 'Q': choices += list(model.heap)
            address = rng.choice(choices) if choices else 1
            command = (op, address + rng.choice([0, 0, 0, 1, 17, 4096]), 0)
        model.step(*command); commands.append(command)
        if i % 40 == 0: commands.append(('D', 0, 0))
    commands.append(('D', 0, 0))
    run_model(allocator, tmp_path, commands, hn=48, pn=18)

@pytest.mark.parametrize('mutation', [
    ('s->reserve_begin, bytes, bytes, align, OWN_RESERVATION',
     's->heap.begin, bytes, bytes, align, OWN_RESERVATION'),
    ('if (kind != OWN_RESERVATION)', 'if (kind == OWN_HEAP)'),
    ('if (s->physical.records[i].end > end) end = s->physical.records[i].end;',
     'if (s->physical.records[i].bytes > end) end = s->physical.records[i].bytes;'),
])
def test_compiled_negative_controls_are_rejected(tmp_path, mutation):
    broken = compile_provider(tmp_path, mutation)
    with pytest.raises(AssertionError):
        run_model(broken, tmp_path, [('R', 16, 0), ('C', 33, 8192),
                                   ('P', BASE+0x17000, 16), ('D', 0, 0)])

@pytest.mark.parametrize('mutation,commands', [
    (('a->records[at].begin < end', 'a->records[at].end < begin'),
     [('P', BASE+0x8001, 16), ('D', 0, 0), ('V', 0x8001, 0x34),
      ('P', BASE+0x8002, 32), ('D', 0, 0)]),
    (('(uint64_t)high + 1u', 'UINT64_C(0x100000000)'),
     [('B', 32, 4096, BASE+0x4000, BASE+0x400f), ('D', 0, 0)]),
])
def test_compiled_pin_overlap_and_range_controls_are_rejected(tmp_path, mutation, commands):
    broken = compile_provider(tmp_path, mutation)
    with pytest.raises(AssertionError):
        run_model(broken, tmp_path, commands)

def test_bounded_physical_fit_and_requested_byte_limits(allocator, tmp_path):
    run_model(allocator, tmp_path, [
        ('B', 16, 4096, BASE+0x8000, BASE+0x800f),
        ('B', 16, 4096, BASE+0x8000, BASE+0x800f),
        ('B', 16, 4096, BASE+0x9000, BASE+0x900e),
        ('B', 16, 4096, BASE+0x9000, BASE+0x8000),
        ('B', 16, 4096, BASE+0x9001, BASE+0xa00f),
        ('B', 16, 4096, BASE+0xb001, BASE+0xc00e),
        ('B', 0, 4096, BASE+0xd000, BASE+0xd000),
        ('B', 0xffffffff, 4096, BASE, 0xffffffff),
        ('B', 16, 0x80000000, BASE+1, 0xffffffff), ('D', 0, 0)])

@pytest.mark.parametrize('harness', ['guest_allocation_contracts.c', 'guest_allocation_threads.c'])
def test_lifecycle_guards_and_concurrent_ownership(tmp_path, harness):
    exe = compile_provider(tmp_path, harness=harness)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'passed' in result.stdout

def test_actual_allocation_adapters_and_kernel_bridge(tmp_path):
    kernel = HERE / 'integration/src/kernel'
    if not kernel.exists(): kernel = ROOT / 'external/ts3FPrecomp/src/kernel'
    layout = (kernel / 'xbox_memory_layout.c').read_text(encoding='utf-8')
    bridge = (kernel / 'kernel_bridge.c').read_text(encoding='utf-8')
    initialization = layout[layout.index('/* Project allocation provider storage.'):
                            layout.index('/* End project allocation initialization. */')]
    adapters = layout[layout.index('/* Project allocation ABI adapters;'):
                      layout.index('/* End project allocation ABI adapters. */')]
    allocate_ex = bridge[bridge.index('static void bridge_MmAllocateContiguousMemoryEx(void)'):
                         bridge.index('/* ── MmFreeContiguousMemory')]
    # Production lifetime ordering, separate from the compiled adapter contract.
    init = layout[layout.index('BOOL xbox_MemoryLayoutInit('):]
    assert init.index('if (!xbox_AllocationInit())') < init.index('xbox_Nv2aAckStart();')
    shutdown = layout[layout.index('void xbox_MemoryLayoutShutdown(void)'):]
    assert shutdown.index('tsfp_allocations_stop(') < shutdown.index('tsfp_guest_map_stop(')
    for obsolete in ('g_heap_blocks', 'g_heap_next', 'g_contig_blocks', 'g_reserve_next',
                     'xbox_ContiguousPin', 'contig_take_free'):
        assert obsolete not in layout
    assert 'memset(' not in allocate_ex
    assert 'guest_allocation.c' in (kernel / 'CMakeLists.txt').read_text(encoding='utf-8')
    for name in ('guest_allocation.h', 'guest_allocation.c', 'guest_virtual_memory.h',
                 'guest_memory_services.c', 'guest_memory_services.h', 'memory_statistics.c', 'memory_statistics.h'):
        (tmp_path / name).write_bytes((PROVIDER / name).read_bytes())
    harness = tmp_path / 'adapters.c'
    harness.write_text('''#include "guest_allocation.h"
#include "guest_memory_services.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define XBOX_CONTIG_BASE 0x80000000u
#define XBOX_CONTIG_SIZE (64u * 1024u * 1024u)
#define XBOX_CONTIG_RESERVED_LOW (1024u * 1024u)
#define XBOX_GPU_INSTANCE_DEFAULT (128u * 1024u)
#define XBOX_HEAP_BASE 0x01120000u
static void *g_memory_base, *g_contig_memory;
static size_t g_memory_size = 64u * 1024u * 1024u;
static size_t g_xbox_total_ram = 64u * 1024u * 1024u;
static uint32_t arguments[5], g_eax;
#define STACK_ARG(i) arguments[i]
#define KERNEL_LOG_ON_HALF() 0
''' + initialization + adapters + '''
static void xbox_GetGuestMemoryServices(tsfp_guest_mm *memory) {
    memset(memory,0,sizeof(*memory));memory->vm.allocations=&g_guest_allocations;
    memory->physical_base=XBOX_CONTIG_BASE;memory->physical_size=XBOX_CONTIG_SIZE;
    memory->gpu_default=XBOX_GPU_INSTANCE_DEFAULT;
}
''' + allocate_ex + '''
static uint32_t request(uint32_t size, uint32_t low, uint32_t high, uint32_t alignment) {
    arguments[0]=size; arguments[1]=low; arguments[2]=high; arguments[3]=alignment;
    bridge_MmAllocateContiguousMemoryEx(); return g_eax;
}
int main(void) {
    uint32_t address=77, bytes=88;
    unsigned char *physical;
    assert(!xbox_HeapAlloc(1, 4));
    g_memory_base = malloc(g_memory_size); g_contig_memory = malloc(XBOX_CONTIG_SIZE);
    assert(g_memory_base && g_contig_memory);
    memset(g_memory_base, 0x5a, g_memory_size); memset(g_contig_memory, 0x7e, XBOX_CONTIG_SIZE);
    physical = g_contig_memory;
    assert(xbox_AllocationInit());
    assert(xbox_HeapAlloc(0, 0) == XBOX_HEAP_BASE);
    assert(xbox_HeapAlloc(17, 4096) == XBOX_HEAP_BASE+4096);
    assert(xbox_HeapBlockSize(XBOX_HEAP_BASE+15) == 1);
    assert(!xbox_ReserveAlloc(32, 0));
    xbox_HeapFree(XBOX_HEAP_BASE+1);
    assert(xbox_HeapBlockSize(XBOX_HEAP_BASE) == 16);
    xbox_HeapFree(XBOX_HEAP_BASE);
    assert(!xbox_HeapBlockSize(XBOX_HEAP_BASE));
    assert(request(0x900, 0, UINT32_MAX, 0) == 0x80100000u);
    assert(xbox_HeapBlockSize(0x80100001u) == 0x8ff);
    assert(!xbox_HeapBlockSize(0x80100900u));
    assert(!xbox_ContiguousBlock(-1, &address, &bytes) && address==77 && bytes==88);
    assert(request(0x200, 0x61000, 0x611ff, 4096) == 0x80061000u);
    assert(physical[0x61000]==0 && physical[0x611ff]==0 && physical[0x61200]==0x7e);
    physical[0x61080]=0x2a;
    assert(!request(0x80, 0x61080, 0x610ff, 4096));
    assert(physical[0x61080]==0x2a);
    assert(!request(16, 0x10000, 0x1000f, 4096) && physical[0x10000]==0x7e);
    assert(!request(32, 0x70000, 0x7000f, 4096));
    assert(!request(32, 0x80000, 0x7ffff, 4096));
    assert(!request(UINT32_MAX, 0xfffffff0u, UINT32_MAX, 4096));
    assert(!request(16, XBOX_CONTIG_SIZE, UINT32_MAX, 4096));
    assert(request(512, 0x200001, 0x210000, 4096) == 0x80201000u);
    assert(xbox_ContiguousAllocatedBytes()==0x202000u);
    assert(xbox_ContiguousBlock(0, &address, &bytes) && address==0x80061000u && bytes==512);
    assert(xbox_ContiguousBlock(2, &address, &bytes) && address==0x80201000u && bytes==512);
    assert(xbox_ContiguousFree(0x80201000u));
    assert(xbox_ContiguousAllocatedBytes()==0x101000u);
    assert(!xbox_ContiguousBlock(2, &address, &bytes));
    assert(xbox_ContiguousFree(0x80061000u));
    tsfp_allocations_stop(&g_guest_allocations);
    assert(!request(16, 0, UINT32_MAX, 4096));
    free(g_memory_base); free(g_contig_memory);
    puts("actual allocation initialization, adapters and kernel bridge passed");
    return 0;
}
''', encoding='utf-8')
    warnings = ['/W4'] if os.name == 'nt' else ['-Wall', '-Wextra']
    exe = _compile(tmp_path, harness, extra=[tmp_path / 'guest_allocation.c', tmp_path / 'guest_memory_services.c',
        tmp_path / 'memory_statistics.c', *warnings])
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'passed' in result.stdout

def test_actual_audio_enumeration_and_gpu_extent_consumers(tmp_path):
    runtime = ROOT / 'external/ts3FPrecomp/src'
    apu = (runtime/'apu/apu_dsp.c').read_text(encoding='utf-8')
    gpu = (runtime/'kernel/nv2a_pb_exec.c').read_text(encoding='utf-8')
    audio = apu[apu.index('#define APU_DSP_DOORBELL_OFFSET'):
                apu.index('/* Physical address of the first page a DSP scatter-gather table')]
    image_test = gpu[gpu.index('static int surface_hits_image('):
                     gpu.index('/* Where a DMA-object offset actually lives.')]
    resolver = gpu[gpu.index('static uint32_t dma_resolve('):
                   gpu.index('static int surface_write_refused(')]
    for name in ('guest_allocation.h', 'guest_allocation.c'):
        (tmp_path / name).write_bytes((PROVIDER / name).read_bytes())
    harness = tmp_path/'consumers.c'
    # Relocate the fixture's physical window within one C array so APU guest
    # pointer arithmetic remains within an object. Real 0x80000000 addressing
    # is exercised separately by the actual bridge and high-address probes.
    harness.write_text('''#include "guest_allocation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define XBOX_CONTIG_BASE 0x10000u
#define XBOX_CONTIG_SIZE 0x20000u
static unsigned char memory[0x30000];
static uint32_t g_xbox_image_lo=0x4000, g_xbox_image_hi=0x6000;
static tsfp_allocations state;
static tsfp_allocation_record hr[32], pr[32];
typedef struct { uint8_t *ram_ptr; } MCPXAPUState;
int xbox_ContiguousBlock(int index, uint32_t *addr, uint32_t *size) {
    return index>=0 && tsfp_physical_block(&state, (size_t)index, addr, size);
}
uint32_t xbox_ContiguousAllocatedBytes(void) { return tsfp_physical_extent(&state); }
''' + audio + image_test + resolver + '''
static void write_word(uint32_t address, uint32_t word) { memcpy(memory+address, &word, 4); }
static uint32_t read_word(uint32_t address) { uint32_t word; memcpy(&word, memory+address, 4); return word; }
int main(void) {
    tsfp_allocation_config c={0}; MCPXAPUState apu={memory};
    uint32_t first, second, pinned, high;
    c.ram=memory; c.ram_size=0x10000; c.heap_begin=256; c.reserve_begin=0x10000;
    c.physical=memory+0x10000; c.physical_size=0x20000; c.physical_base=0x10000;
    c.physical_begin=0x14000; c.physical_end=0x2e000;
    assert(tsfp_allocations_init(&state, &c, hr, 32, pr, 32));
    first=tsfp_physical_allocate(&state, 0x900, 4096);
    second=tsfp_physical_allocate(&state, 0x900, 4096);
    assert(first==0x14000 && second==0x15000);
    write_word(first+0x810, 2); write_word(second+0x810, 3);
    for (int i=0; i<4; ++i) dsp_ack_discovered(&apu);
    assert(read_word(first+0x810)==2 && read_word(second+0x810)==3);
    assert(tsfp_physical_release(&state, first));
    for (int i=0; i<4; ++i) dsp_ack_discovered(&apu);
    assert(read_word(first+0x810)==2); /* freed allocations are no longer scanned */
    assert(read_word(second+0x810)==0); /* reindexing retained address-keyed history */
    pinned=tsfp_physical_allocate_at(&state, 0x12000, 0x900);
    assert(pinned==0x12000);
    write_word(pinned+0x810, 4); write_word(second+0x810, 5);
    dsp_ack_discovered(&apu);
    assert(read_word(second+0x810)==0 && read_word(pinned+0x810)==4);
    for (int i=0; i<7; ++i) dsp_ack_discovered(&apu);
    assert(read_word(pinned+0x810)==0);
    assert(dma_resolve(0x17000)==0x17000);
    high=tsfp_physical_allocate_at(&state, 0x28000, 0x100);
    assert(high==0x28000 && tsfp_physical_extent(&state)==0x19000);
    assert(tsfp_physical_release(&state, second));
    assert(dma_resolve(0x18000)==0x28000); /* high live buffer */
    assert(dma_resolve(0x17000)==0x27000); /* extent includes gaps */
    assert(dma_resolve(0x19000)==0x19000); /* exact upper boundary excluded */
    assert(tsfp_physical_release(&state, high));
    assert(tsfp_physical_extent(&state)==0x4000);
    assert(dma_resolve(0x17000)==0x17000);
    assert(dma_resolve(0x5000)==0x15000); /* image overlap fallback still applies */
    puts("actual APU address-keyed discovery and GPU physical extent consumers passed");
    return 0;
}
''', encoding='utf-8')
    warnings = ['/W4'] if os.name == 'nt' else ['-Wall', '-Wextra']
    exe = _compile(tmp_path, harness, extra=[tmp_path/'guest_allocation.c', *warnings])
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'passed' in result.stdout
