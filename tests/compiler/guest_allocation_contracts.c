#include "guest_allocation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct GuardedMemory { unsigned char before[64], data[65536], after[64]; } GuardedMemory;
static GuardedMemory ram, physical;
static tsfp_allocations state;
static tsfp_allocation_record hr[64], pr[32];

static tsfp_allocation_config config(void) {
    tsfp_allocation_config c = {0};
    c.ram = ram.data; c.ram_size = sizeof(ram.data); c.heap_begin = 256; c.reserve_begin = 32768;
    c.physical = physical.data; c.physical_base = 0xffff0000u;
    c.physical_size = 0xf000u; c.physical_begin = 0xffff1000u; c.physical_end = 0xfffff000u;
    c.protected_begin = 0xffff5000u; c.protected_end = 0xffff6000u;
    return c;
}
static int initialize(const tsfp_allocation_config *c) {
    return tsfp_allocations_init(&state, c, hr, 64, pr, 32);
}
static void same_bytes(const unsigned char *p, size_t n, unsigned char byte) {
    for (size_t i = 0; i < n; ++i) assert(p[i] == byte);
}
static void invalid_configuration(void) {
    tsfp_allocation_config original = config(), changed;
    tsfp_allocations before;
    tsfp_allocation_record oldhr[64], oldpr[32];
    assert(initialize(&original));
    assert(tsfp_heap_allocate(&state, 17, 4) == 256);
    assert(tsfp_physical_allocate(&state, 33, 4096) == 0xffff1000u);
    memcpy(&before, &state, sizeof(state)); memcpy(oldhr, hr, sizeof(hr)); memcpy(oldpr, pr, sizeof(pr));
    for (unsigned i = 0; i < 23; ++i) {
        changed = original;
        switch (i) {
        case 0: changed.ram = NULL; break;
        case 1: changed.ram_size = 0; break;
        case 2: changed.heap_begin = 0; break;
        case 3: changed.heap_begin = changed.ram_size; break;
        case 4: changed.reserve_begin = changed.heap_begin - 1; break;
        case 5: changed.reserve_begin = changed.ram_size + 1; break;
        case 6: changed.ram = (void *)(UINTPTR_MAX - 256); break;
        case 7: changed.physical = NULL; break;
        case 8: changed.physical_size = 0; break;
        case 9: changed.physical_size = 0x10000u; break; /* 32-bit exclusive end overflow */
        case 10: changed.physical_base = 0x1000u; break; /* guest windows overlap */
        case 11: changed.physical_begin++; break;
        case 12: changed.physical_end++; break;
        case 13: changed.physical_begin = changed.physical_end; break;
        case 14: changed.physical_begin = changed.physical_base - 4096; break;
        case 15: changed.protected_begin++; break;
        case 16: changed.protected_end++; break;
        case 17: changed.protected_end = changed.protected_begin; break;
        case 18: changed.protected_end = 0xfffff001u; break;
        case 19: changed.physical = ram.data; break;
        case 20: changed.ram = hr; break;
        case 21: changed.ram = &state; break;
        case 22: changed.physical = pr; break;
        }
        assert(!initialize(&changed));
        assert(!memcmp(&state, &before, sizeof(state)));
        assert(!memcmp(hr, oldhr, sizeof(hr))); assert(!memcmp(pr, oldpr, sizeof(pr)));
    }
    assert(!tsfp_allocations_init(&state, &original, hr, 0, pr, 32));
    assert(!tsfp_allocations_init(&state, &original, hr, SIZE_MAX, pr, 32));
    assert(!tsfp_allocations_init(&state, &original, hr, 64, pr, SIZE_MAX));
    assert(!tsfp_allocations_init(&state, &original, hr, 64, hr, 32));
    assert(!tsfp_allocations_init(&state, &original, NULL, 64, pr, 32));
    assert(!tsfp_allocations_init(&state, &original, hr, 64, NULL, 32));
    assert(!tsfp_allocations_init(NULL, &original, hr, 64, pr, 32));
    assert(!tsfp_allocations_init(&state, NULL, hr, 64, pr, 32));
    assert(!memcmp(&state, &before, sizeof(state)));
    assert(!memcmp(hr, oldhr, sizeof(hr))); assert(!memcmp(pr, oldpr, sizeof(pr)));
}

static void boundaries(void) {
    tsfp_allocation_config c = config();
    uint32_t a = 77, n = 88;
    assert(initialize(&c));
    assert(!tsfp_physical_block(&state, SIZE_MAX, &a, &n) && a == 77 && n == 88);
    assert(tsfp_physical_extent(&state) == 0x6000u); /* protected hole also has ownership */
    assert(tsfp_physical_allocate_at(&state, 0xffffefffu, 1) == 0xffffefffu);
    assert(tsfp_allocation_remaining(&state, 0xffffefffu) == 1);
    assert(!tsfp_allocation_remaining(&state, 0xffffe000u));
    assert(tsfp_physical_extent(&state) == 0xf000u);
    assert(!tsfp_physical_allocate_at(&state, 0xffffefffu, 2));
    assert(!tsfp_physical_allocate_at(&state, 0xffffffffu, 0xffffffffu));
    assert(!tsfp_physical_allocate(&state, 1, 0x80000000u));
    assert(tsfp_physical_allocate(&state, 16384, 4096) == 0xffff1000u);
    assert(tsfp_physical_allocate(&state, 1, 4096) == 0xffff6000u);
    assert(!tsfp_physical_allocate_at(&state, 0xffff5001u, 1));
    assert(!tsfp_physical_release(&state, 0xffff5000u));
    assert(tsfp_physical_block(&state, 0, NULL, NULL));
    assert(tsfp_physical_block(&state, 2, &a, &n) && a == 0xffffefffu && n == 1);
    assert(tsfp_physical_release(&state, 0xffffefffu));
    assert(tsfp_physical_extent(&state) == 0x7000u);
    assert(!tsfp_physical_release(&state, 0xffffefffu));
    assert(!tsfp_allocation_remaining(&state, 0xffffefffu));
    assert(!tsfp_physical_allocate_range(&state, 16, 4096, 0xffff9000u, 0xffff8000u));
    assert(tsfp_physical_allocate_range(&state, 17, 4096, 0xffff9001u, 0xffffa010u) == 0xffffa000u);
    assert(!tsfp_physical_allocate_range(&state, 17, 4096, 0xffffc001u, 0xffffd00fu));
    assert(tsfp_physical_allocate_range(&state, 17, 4096, 0xffffc001u, 0xffffd010u) == 0xffffd000u);
    assert(!tsfp_physical_allocate_range(&state, 17, 4096, 0xfffffff0u, 0xffffffffu));
    assert(tsfp_physical_allocate_range(&state, 0, 4096, 0xffffb000u, 0xffffb000u) == 0xffffb000u);
    assert(!tsfp_physical_allocate_range(&state, 0, 4096, 0xffffc001u, 0xffffc001u));
    tsfp_allocations_stop(&state);
    assert(!tsfp_physical_block(&state, 0, &a, &n));
    assert(!tsfp_physical_extent(&state));
    assert(!tsfp_heap_allocate(&state, 1, 0));
    assert(!tsfp_reserve_allocate(&state, 1, 0));
    assert(!tsfp_physical_release(&state, 0xffff1000u));
    memset(&c, 0, sizeof(c)); c.ram = ram.data; c.ram_size = sizeof(ram.data);
    c.heap_begin = 256; c.reserve_begin = c.ram_size;
    assert(tsfp_allocations_init(&state, &c, hr, 64, NULL, 0));
    assert(!tsfp_physical_allocate(&state, 1, 4096));
    assert(!tsfp_physical_allocate_at(&state, 0x80001000u, 1));
    assert(!tsfp_reserve_allocate(&state, 1, 0));
    assert(!tsfp_physical_block(&state, 0, NULL, NULL));
    assert(!tsfp_physical_extent(&state));
    assert(tsfp_heap_allocate(&state, sizeof(ram.data)-256, 0) == 256);
    assert(!tsfp_heap_allocate(&state, 1, 0));
    same_bytes(ram.data+256, sizeof(ram.data)-256, 0);
    assert(tsfp_heap_release(&state, 256));
    assert(tsfp_heap_allocate(&state, 0, 0) == 256);
    assert(tsfp_allocation_remaining(&state, 256) == 16);
}
int main(void) {
    memset(&ram, 0x92, sizeof(ram)); memset(&physical, 0x81, sizeof(physical));
    invalid_configuration(); boundaries();
    same_bytes(ram.before, 64, 0x92); same_bytes(ram.after, 64, 0x92);
    same_bytes(physical.before, 64, 0x81); same_bytes(physical.after, 64, 0x81);
    same_bytes(physical.data+0xf000, 4096, 0x81);
    same_bytes(physical.data+0x5000, 4096, 0x81);
    puts("allocation lifecycle, invalid configuration and guarded boundaries passed");
    return 0;
}
