#include "guest_virtual_memory.h"
#include <string.h>

#define OK UINT32_C(0)
#define ARGUMENT UINT32_C(0xc000000d)
#define ACCESS UINT32_C(0xc0000005)
#define EXHAUSTED UINT32_C(0xc0000017)
#define EMPTY UINT32_C(0xc0000045)
#define RESERVE UINT32_C(0x2000)
#define COMMIT UINT32_C(0x1000)
#define RELEASE UINT32_C(0x8000)

static unsigned char *span(const tsfp_guest_vm *v, uint32_t a, uint32_t n, int write) {
    if (!v || !v->resolve || !a || !n || (uint64_t)a + n > UINT64_C(0x100000000))
        return NULL;
    return v->resolve(v->host, a, n, write);
}
static uint32_t read_word(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void write_word(unsigned char *p, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(n >> (8 * i));
}

uint32_t tsfp_guest_vm_allocate(const tsfp_guest_vm *v, uint32_t base_word,
    uint32_t zero_bits, uint32_t size_word, uint32_t type, uint32_t protection) {
    unsigned char *base = span(v, base_word, 4, 1);
    unsigned char *size = span(v, size_word, 4, 1);
    uint32_t requested, hint, result;
    (void)zero_bits; (void)protection;
    if (!base || !size || !v->allocations) return ARGUMENT;
    /* Snapshot both before an allocator zeroes memory or overlapping outputs
     * are written. Publication order is base followed by actual size. */
    hint = read_word(base); requested = read_word(size);
    if (!requested) return EMPTY;
    if (hint && !(type & RESERVE)) return OK;
    result = tsfp_heap_allocate(v->allocations, requested, 4096);
    if (!result && (type & RESERVE) && !(type & COMMIT)) {
        result = tsfp_reserve_allocate(v->allocations, requested, 4096);
        while (!result && requested > 65536) {
            requested /= 2;
            result = tsfp_heap_allocate(v->allocations, requested, 4096);
        }
    }
    if (!result) return EXHAUSTED;
    write_word(base, result);
    write_word(size, requested);
    return OK;
}

uint32_t tsfp_guest_vm_free(const tsfp_guest_vm *v, uint32_t base_word,
    uint32_t size_word, uint32_t type, int legacy) {
    unsigned char *base = span(v, base_word, 4, 0), *size = NULL;
    uint32_t address;
    if (!base || !v->allocations) return ARGUMENT;
    address = read_word(base);
    if (!address) return ARGUMENT;
    if (legacy || !(type & RELEASE)) return OK;
    base = span(v, base_word, 4, 1);
    if (size_word) size = span(v, size_word, 4, 1);
    if (!base || (size_word && !size)) return ARGUMENT;
    (void)tsfp_virtual_release(v->allocations, address);
    write_word(base, 0);
    if (size) write_word(size, 0);
    return OK;
}

uint32_t tsfp_guest_vm_query(const tsfp_guest_vm *v, uint32_t address, uint32_t output) {
    unsigned char *destination = span(v, output, 28, 1);
    uint32_t page = address & ~UINT32_C(4095), words[7];
    int committed;
    if (!destination) return ARGUMENT;
    committed = page >= v->code_begin && page < v->ram_size;
    words[0] = page; words[1] = page; words[2] = committed ? 4u : 0u;
    words[3] = committed ? v->ram_size - page : 4096u;
    words[4] = committed ? 0x1000u : 0x10000u;
    words[5] = 4; words[6] = committed ? 0x20000u : 0u;
    for (unsigned i = 0; i < 7; ++i) write_word(destination + 4 * i, words[i]);
    return OK;
}

uint32_t tsfp_guest_vm_protect(const tsfp_guest_vm *v, uint32_t base_word,
    uint32_t size_word, uint32_t protection, uint32_t old_word) {
    unsigned char *base = span(v, base_word, 4, 0);
    unsigned char *size = span(v, size_word, 4, 0);
    unsigned char *out = old_word ? span(v, old_word, 4, 1) : NULL;
    uint32_t address, bytes, previous = 0;
    uint64_t first, last;
    if (!base || !size || (old_word && !out) || !v->protect ||
        !v->host_page_size || (v->host_page_size & (v->host_page_size - 1u))) return ACCESS;
    address = read_word(base); bytes = read_word(size);
    if (!span(v, address, bytes, -1)) return ACCESS;
    first = address & ~(v->host_page_size - 1u);
    last = ((uint64_t)address + bytes + v->host_page_size - 1u) &
           ~((uint64_t)v->host_page_size - 1u);
    if (old_word && (uint64_t)old_word < last && (uint64_t)old_word + 4 > first)
        return ACCESS;
    if (!v->protect(v->host, address, bytes, protection, &previous)) return ACCESS;
    if (out) write_word(out, previous);
    return OK;
}
