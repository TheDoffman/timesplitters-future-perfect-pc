/* Native pointer-width VM calls. This interface never contains guest addresses. */
#ifndef TSFP_HOST_VIRTUAL_MEMORY_H
#define TSFP_HOST_VIRTUAL_MEMORY_H
#include <stddef.h>
#include <stdint.h>

typedef struct tsfp_host_vm {
    void *host;
    void *(*allocate)(void *, void *hint, size_t bytes, uint32_t type, uint32_t protection);
    int (*release)(void *, void *address, size_t bytes, uint32_t type);
    size_t (*query)(void *, const void *address, void *record, size_t bytes);
} tsfp_host_vm;

/* Native buffers must be valid host storage and stable for each call. Host
 * callbacks own mappings and validate host flags/ranges. Failed allocation or
 * release leaves caller outputs unchanged. ZeroBits retains no-effect policy. */
uint32_t tsfp_host_vm_allocate(const tsfp_host_vm *, void **base, uintptr_t zero_bits,
    size_t *size, uint32_t type, uint32_t protection);
uint32_t tsfp_host_vm_free(const tsfp_host_vm *, void **base, size_t *size, uint32_t type);
/* The adapter supplies a real native query-record scratch buffer and its size.
 * This retains the host ABI including pointer widths, independent of the
 * 28-byte guest record. Scratch is zero-initialized before the host query;
 * callbacks return a full record or failure. Copy is bounded by output size;
 * returned length changes only on success. Scratch must not overlap outputs. */
uint32_t tsfp_host_vm_query(const tsfp_host_vm *, const void *address, void *output,
    uint32_t output_size, uint32_t *returned, void *scratch, size_t scratch_size);
uint32_t tsfp_host_vm_protection(uint32_t protection);
#endif
