#include "guest_thread_memory.h"
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

enum { MAIN_TIB = 0x1000, TIB_BYTES = 64, OBJECT_BYTES = 512, DATA_BYTES = 64,
       OBJECT_OFFSET = 0x760000, PRCB_OFFSET = 0x761000,
       TLS_OFFSET = 0x770000, DATA_OFFSET = 0x770200,
       STACK_OFFSET = 0x780000, STACK_END = 0xf80000, FALLBACK_OFFSET = 0x700000 };

static void enter(tsfp_thread_memory *s) {
#if defined(_MSC_VER)
    while (_InterlockedCompareExchange(&s->lock, 1, 0)) { }
#else
    while (__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_thread_memory *s) {
#if defined(_MSC_VER)
    (void)_InterlockedExchange(&s->lock, 0);
#else
    __atomic_clear(&s->lock, __ATOMIC_RELEASE);
#endif
}
static uint32_t read32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void write32(unsigned char *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> (8 * i));
}
static int valid_host_span(const void *p, size_t n) {
    return p && n && (uintptr_t)p <= UINTPTR_MAX - n;
}
static int host_overlap(const void *a, size_t an, const void *b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}

int tsfp_thread_memory_init(tsfp_thread_memory *s, tsfp_allocations *a,
                            const tsfp_image_info *image) {
    tsfp_image_info info;
    uint32_t shift, total, object, prcb, tls, data, stack, end;
    unsigned char *ram;
    if (!s || !a || !image || !valid_host_span(s, sizeof(*s)) ||
        !valid_host_span(a, sizeof(*a)) || host_overlap(s, sizeof(*s), a, sizeof(*a))) return 0;
    info = *image;
    ram = a->heap.memory;
    if (!a->active || a->heap.origin || !valid_host_span(ram, a->heap.end) ||
        host_overlap(s, sizeof(*s), ram, a->heap.end) ||
        host_overlap(a, sizeof(*a), ram, a->heap.end) ||
        info.base < MAIN_TIB + TIB_BYTES || info.base >= info.image_end ||
        tsfp_image_bootstrap(&info, a->heap.end, &shift, &total) != TSFP_IMAGE_OK)
        return 0;
    end = shift + STACK_END;
    if (a->heap.begin < end || info.image_end > shift + FALLBACK_OFFSET ||
        (!info.tls_directory && (info.tls_start || info.tls_bytes || info.tls_zero_fill)) ||
        (info.tls_bytes && (info.tls_start < info.base ||
          (uint64_t)info.tls_start + info.tls_bytes > info.image_end))) return 0;
    object = shift + OBJECT_OFFSET; prcb = shift + PRCB_OFFSET;
    tls = shift + TLS_OFFSET; data = shift + DATA_OFFSET; stack = shift + STACK_OFFSET;

    /* All validation precedes writes. The reserved ranges are disjoint from
     * loaded image data and the allocator's heap by the checks above. */
    memset(ram + MAIN_TIB, 0, TIB_BYTES);
    memset(ram + object, 0, OBJECT_BYTES);
    memset(ram + prcb, 0, 1024);
    write32(ram + MAIN_TIB, UINT32_MAX);
    write32(ram + MAIN_TIB + 4, end - 16);
    write32(ram + MAIN_TIB + 8, stack);
    write32(ram + MAIN_TIB + 0x18, MAIN_TIB);
    write32(ram + MAIN_TIB + 0x20, prcb);
    write32(ram + MAIN_TIB + 0x28, object);
    write32(ram + object + 0x28, shift + FALLBACK_OFFSET);
    write32(ram + object + 0x12c, 0x1000);
    if (total) {
        memset(ram + tls, 0, total);
        memset(ram + data, 0, DATA_BYTES);
        if (info.tls_bytes) memcpy(ram + tls, ram + info.tls_start, info.tls_bytes);
        write32(ram + tls, data);
        write32(ram + MAIN_TIB + 4, tls + total);
        write32(ram + object + 0x28, tls);
    }
    memset(s, 0, sizeof(*s));
    s->allocations = a; s->ram = ram; s->ram_size = a->heap.end;
    s->low_shift = shift; s->tls_total = total; s->next_id = 0x1001; s->active = 1;
    return 1;
}

void tsfp_thread_memory_stop(tsfp_thread_memory *s) {
    if (!s) return;
    enter(s); s->active = 0; leave(s);
}

uint32_t tsfp_thread_tib_create(tsfp_thread_memory *s) {
    uint32_t tib = 0;
    if (!s) return 0;
    enter(s);
    if (s->active && s->tls_total && s->next_id) {
        uint32_t object_offset = (TIB_BYTES + s->tls_total + DATA_BYTES + 15u) & ~15u;
        tib = tsfp_heap_allocate(s->allocations, object_offset + OBJECT_BYTES, 16);
        if (tib) {
            uint32_t tls = tib + TIB_BYTES, data = tls + s->tls_total;
            uint32_t object = tib + object_offset;
            memcpy(s->ram + tib, s->ram + MAIN_TIB, TIB_BYTES);
            memcpy(s->ram + tls, s->ram + s->low_shift + TLS_OFFSET, s->tls_total);
            memcpy(s->ram + object, s->ram + s->low_shift + OBJECT_OFFSET, OBJECT_BYTES);
            write32(s->ram + tib, UINT32_MAX);
            write32(s->ram + tib + 4, tls + s->tls_total);
            write32(s->ram + tib + 0x28, object);
            write32(s->ram + tls, data);
            write32(s->ram + object + 0x12c, s->next_id);
            ++s->next_id; /* Zero after UINT32_MAX means identity exhaustion. */
        }
    }
    leave(s); return tib;
}

uint32_t tsfp_thread_current_object(tsfp_thread_memory *s, uint32_t fs) {
    uint32_t result = 0;
    if (!s) return 0;
    enter(s);
    if (s->active && fs >= MAIN_TIB && (uint64_t)fs + 0x2c <= s->ram_size)
        result = read32(s->ram + fs + 0x28);
    leave(s); return result;
}

uint32_t tsfp_thread_stack_acquire(tsfp_thread_memory *s) {
    uint32_t result = 0;
    if (!s) return 0;
    enter(s);
    if (s->active) for (unsigned i = 0; i < TSFP_THREAD_STACK_COUNT; ++i) {
        uint32_t base;
        if (s->stacks[i]) continue;
        base = tsfp_heap_allocate(s->allocations, TSFP_THREAD_STACK_BYTES, 4096);
        if (base) result = s->stacks[i] = base + TSFP_THREAD_STACK_BYTES - 16;
        break;
    }
    leave(s); return result;
}
int tsfp_thread_stack_release(tsfp_thread_memory *s, uint32_t top) {
    int result = 0;
    if (!s || !top) return 0;
    enter(s);
    if (s->active) for (unsigned i = 0; i < TSFP_THREAD_STACK_COUNT; ++i) {
        if (s->stacks[i] != top) continue;
        result = tsfp_heap_release(s->allocations, top + 16 - TSFP_THREAD_STACK_BYTES);
        if (result) s->stacks[i] = 0;
        break;
    }
    leave(s); return result;
}

int tsfp_worker_stack_acquire(tsfp_thread_memory *s) {
    int result = -1;
    if (!s) return -1;
    enter(s);
    if (s->active) for (int i = 0; i < TSFP_WORKER_STACK_COUNT; ++i) {
        uint32_t bit = UINT32_C(1) << i;
        if (s->worker_mask & bit) continue;
        s->worker_mask |= bit; result = i; break;
    }
    leave(s); return result;
}
int tsfp_worker_stack_release(tsfp_thread_memory *s, int index) {
    int result = 0;
    if (!s || index < 0 || index >= TSFP_WORKER_STACK_COUNT) return 0;
    enter(s);
    if (s->active) {
        uint32_t bit = UINT32_C(1) << index;
        result = (s->worker_mask & bit) != 0;
        s->worker_mask &= ~bit;
    }
    leave(s); return result;
}
