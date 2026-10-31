/* Original interval ownership engine for mapped guest storage. Free space is
 * implicit between sorted live spans: releases need no tombstones or split
 * metadata, and alignment gaps remain usable by subsequent allocations. */
#include "guest_allocation.h"
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

enum { OWN_HEAP = 1, OWN_RESERVATION, OWN_PHYSICAL, OWN_PROTECTED };

static void enter(tsfp_allocations *s) {
#if defined(_MSC_VER)
    while (_InterlockedCompareExchange(&s->lock, 1, 0)) { }
#else
    while (__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_allocations *s) {
#if defined(_MSC_VER)
    (void)_InterlockedExchange(&s->lock, 0);
#else
    __atomic_clear(&s->lock, __ATOMIC_RELEASE);
#endif
}
static int power_two(uint32_t n) { return n && !(n & (n - 1u)); }
static uint64_t align_up(uint64_t n, uint32_t alignment) {
    return (n + alignment - 1u) & ~((uint64_t)alignment - 1u);
}
static int host_span(const void *p, size_t size) {
    return p && size && (uintptr_t)p <= UINTPTR_MAX - size;
}
static int overlap(const void *a, size_t an, const void *b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}

int tsfp_allocations_init(tsfp_allocations *s, const tsfp_allocation_config *c,
    tsfp_allocation_record *hr, size_t hn, tsfp_allocation_record *pr, size_t pn) {
    const void *storage[5];
    size_t bytes[5], n = 3;
    tsfp_allocation_config cfg;
    if (!s || !c || !hn || hn > SIZE_MAX / sizeof(*hr) ||
        !host_span(s, sizeof(*s)) || !host_span(hr, hn * sizeof(*hr))) return 0;
    cfg = *c; /* Configuration may be embedded in caller-owned storage. */
    if (!host_span(cfg.ram, cfg.ram_size) || !cfg.heap_begin ||
        cfg.heap_begin >= cfg.ram_size || cfg.reserve_begin < cfg.heap_begin ||
        cfg.reserve_begin > cfg.ram_size) return 0;
    storage[0] = s; bytes[0] = sizeof(*s);
    storage[1] = hr; bytes[1] = hn * sizeof(*hr);
    storage[2] = cfg.ram; bytes[2] = cfg.ram_size;
    if (cfg.physical) {
        uint64_t end = (uint64_t)cfg.physical_base + cfg.physical_size;
        if (!pn || pn > SIZE_MAX / sizeof(*pr) ||
            !host_span(pr, pn * sizeof(*pr)) ||
            !host_span(cfg.physical, cfg.physical_size) ||
            cfg.physical_base < cfg.ram_size || end > UINT32_MAX ||
            (cfg.physical_base & 4095u) || (cfg.physical_begin & 4095u) ||
            (cfg.physical_end & 4095u) ||
            cfg.physical_begin < cfg.physical_base ||
            cfg.physical_begin >= cfg.physical_end || cfg.physical_end > end)
            return 0;
        if ((cfg.protected_begin || cfg.protected_end) &&
            ((cfg.protected_begin & 4095u) || (cfg.protected_end & 4095u) ||
             cfg.protected_begin < cfg.physical_base ||
             cfg.protected_begin >= cfg.protected_end ||
             cfg.protected_end > cfg.physical_end)) return 0;
        storage[3] = pr; bytes[3] = pn * sizeof(*pr);
        storage[4] = cfg.physical; bytes[4] = cfg.physical_size; n = 5;
    } else if (cfg.physical_base || cfg.physical_size || cfg.physical_begin ||
               cfg.physical_end || cfg.protected_begin || cfg.protected_end) return 0;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            if (overlap(storage[i], bytes[i], storage[j], bytes[j])) return 0;
    memset(s, 0, sizeof(*s));
    s->heap.observer = cfg.observer;s->physical.observer = cfg.observer;
    s->heap.memory = cfg.ram; s->heap.begin = cfg.heap_begin;
    s->heap.end = cfg.ram_size; s->heap.records = hr; s->heap.capacity = hn;
    s->reserve_begin = cfg.reserve_begin;
    if (cfg.physical) {
        s->physical.memory = cfg.physical; s->physical.origin = cfg.physical_base;
        s->physical.begin = cfg.physical_begin; s->physical.end = cfg.physical_end;
        s->physical.records = pr; s->physical.capacity = pn;
        if (cfg.protected_end) {
            tsfp_allocation_record r = {cfg.protected_begin, cfg.protected_end,
                cfg.protected_begin, 0, OWN_PROTECTED};
            pr[0] = r; s->physical.count = 1;
        }
    }
    s->active = 1;
    return 1;
}

void tsfp_allocations_stop(tsfp_allocations *s) {
    if (!s) return;
    enter(s); s->active = 0; s->heap.count = s->physical.count = 0; leave(s);
}

/* All arithmetic is wide until both guest bounds and ownership are checked. */
static uint32_t publish(tsfp_allocation_arena *a, size_t at,
    uint64_t begin, uint64_t end, uint32_t address, uint32_t bytes, uint32_t kind) {
    tsfp_allocation_record record;
    if (a->count == a->capacity || begin < a->origin || end > a->end ||
        end <= begin || !address) return 0;
    if ((at && a->records[at-1].end > begin) ||
        (at < a->count && a->records[at].begin < end)) return 0;
    record.begin = (uint32_t)begin; record.end = (uint32_t)end;
    record.address = address; record.bytes = bytes; record.kind = kind;
    memmove(a->records + at + 1, a->records + at,
            (a->count - at) * sizeof(record));
    a->records[at] = record; ++a->count;
    if (kind != OWN_RESERVATION)
        memset(a->memory + (address - a->origin), 0, bytes);
    if(a->observer)a->observer(kind,address,bytes);
    return address;
}

static uint32_t allocate(tsfp_allocation_arena *a, uint32_t floor,
    uint32_t bytes, uint64_t span, uint32_t alignment, uint32_t kind, uint64_t limit) {
    uint64_t next = align_up(floor, alignment);
    uint64_t requested = bytes ? bytes : 1u;
    if (!a->memory || a->count == a->capacity) return 0;
    for (size_t i = 0; i < a->count; ++i) {
        if (next + requested > limit) return 0;
        if (a->records[i].end <= next) continue;
        if (next + span <= a->records[i].begin)
            return publish(a, i, next, next + span, (uint32_t)next, bytes, kind);
        next = align_up(a->records[i].end, alignment);
    }
    if (next > UINT32_MAX || next + requested > limit) return 0;
    return publish(a, a->count, next, next + span, (uint32_t)next, bytes, kind);
}

uint32_t tsfp_heap_allocate(tsfp_allocations *s, uint32_t bytes, uint32_t align) {
    uint32_t address = 0;
    if (!s) return 0;
    if (bytes < 16) bytes = 16;
    if (align < 4) align = 4;
    if (!power_two(align)) return 0;
    enter(s);
    if (s->active) address = allocate(&s->heap, s->heap.begin, bytes, bytes, align, OWN_HEAP, s->heap.end);
    leave(s); return address;
}
uint32_t tsfp_reserve_allocate(tsfp_allocations *s, uint32_t bytes, uint32_t align) {
    uint32_t address = 0;
    if (!s || !bytes) return 0;
    if (!align) align = 4096;
    if (!power_two(align)) return 0;
    enter(s);
    if (s->active) address = allocate(&s->heap, s->reserve_begin, bytes, bytes, align, OWN_RESERVATION, s->heap.end);
    leave(s); return address;
}
uint32_t tsfp_physical_allocate(tsfp_allocations *s, uint32_t bytes, uint32_t align) {
    return tsfp_physical_allocate_range(s, bytes, align, 0, UINT32_MAX);
}
uint32_t tsfp_physical_allocate_range(tsfp_allocations *s, uint32_t bytes,
    uint32_t align, uint32_t low, uint32_t high) {
    uint32_t address = 0;
    if (!s || low > high) return 0;
    if (align < 4096) align = 4096;
    if (!power_two(align)) return 0;
    enter(s);
    if (s->active) address = allocate(&s->physical,
        low > s->physical.begin ? low : s->physical.begin, bytes,
        align_up(bytes ? bytes : 1u, 4096), align, OWN_PHYSICAL, (uint64_t)high + 1u);
    leave(s); return address;
}
uint32_t tsfp_physical_allocate_at(tsfp_allocations *s, uint32_t address, uint32_t bytes) {
    uint32_t result = 0;
    uint64_t begin = address & ~UINT32_C(4095);
    uint64_t end = align_up((uint64_t)address + (bytes ? bytes : 1u), 4096);
    size_t at = 0;
    if (!s) return 0;
    enter(s);
    if (s->active && s->physical.memory) {
        while (at < s->physical.count && s->physical.records[at].begin < begin) ++at;
        result = publish(&s->physical, at, begin, end, address, bytes, OWN_PHYSICAL);
    }
    leave(s); return result;
}

static int release(tsfp_allocation_arena *a, uint32_t address, uint32_t kind) {
    for (size_t i = 0; i < a->count; ++i) {
        if (a->records[i].address != address || a->records[i].kind != kind) continue;
        uint32_t released_bytes=a->records[i].bytes;
        memmove(a->records + i, a->records + i + 1,
                (a->count - i - 1) * sizeof(*a->records));
        --a->count;if(a->observer)a->observer(kind|256u,address,released_bytes);return 1;
    }
    return 0;
}
int tsfp_heap_release(tsfp_allocations *s, uint32_t address) {
    int result = 0;
    if (!s || !address) return 0;
    enter(s); if (s->active) result = release(&s->heap, address, OWN_HEAP);
    leave(s); return result;
}
int tsfp_physical_release(tsfp_allocations *s, uint32_t address) {
    int result = 0;
    if (!s || !address) return 0;
    enter(s); if (s->active) result = release(&s->physical, address, OWN_PHYSICAL);
    leave(s); return result;
}
int tsfp_virtual_release(tsfp_allocations *s, uint32_t address) {
    int result = 0;
    if (!s || !address) return 0;
    enter(s);
    if (s->active) {
        result = release(&s->heap, address, OWN_HEAP);
        if (!result) result = release(&s->heap, address, OWN_RESERVATION);
    }
    leave(s); return result;
}
static uint32_t remaining(tsfp_allocation_arena *a, uint32_t address) {
    for (size_t i = 0; i < a->count; ++i) {
        const tsfp_allocation_record *r = a->records + i;
        if ((r->kind == OWN_HEAP || r->kind == OWN_PHYSICAL) &&
            address >= r->address && address - r->address < r->bytes)
            return r->bytes - (address - r->address);
    }
    return 0;
}
uint32_t tsfp_allocation_remaining(tsfp_allocations *s, uint32_t address) {
    uint32_t result = 0;
    if (!s || !address) return 0;
    enter(s);
    if (s->active) {
        result = remaining(&s->heap, address);
        if (!result) result = remaining(&s->physical, address);
    }
    leave(s); return result;
}
int tsfp_physical_block(tsfp_allocations *s, size_t index, uint32_t *address, uint32_t *bytes) {
    int found = 0;
    if (!s) return 0;
    enter(s);
    if (s->active) for (size_t i = 0; i < s->physical.count; ++i) {
        const tsfp_allocation_record *r = s->physical.records + i;
        if (r->kind != OWN_PHYSICAL) continue;
        if (index) { --index; continue; }
        if (address) *address = r->address;
        if (bytes) *bytes = r->bytes;
        found = 1; break;
    }
    leave(s); return found;
}
uint32_t tsfp_physical_extent(tsfp_allocations *s) {
    uint32_t end = 0;
    if (!s) return 0;
    enter(s);
    if (s->active && s->physical.memory) {
        end = s->physical.begin;
        for (size_t i = 0; i < s->physical.count; ++i)
            if (s->physical.records[i].end > end) end = s->physical.records[i].end;
        end -= s->physical.origin;
    }
    leave(s); return end;
}
