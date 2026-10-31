/* Project guest virtual-memory services. Mapping and page access are host-owned. */
#ifndef TSFP_GUEST_VIRTUAL_MEMORY_H
#define TSFP_GUEST_VIRTUAL_MEMORY_H
#include "guest_allocation.h"

typedef struct tsfp_guest_vm {
    tsfp_allocations *allocations;
    void *host;
    /* Return a contiguous host span, or NULL. Reject overflow, unmapped pages,
     * and insufficient access (0: read, 1: read/write, -1: mapped only).
     * Mapped-only requests permit restoring access to PAGE_NOACCESS storage.
     * Never create a mapping or trigger MMIO here.
     * Mappings/access and caller buffers must stay stable for the whole call. */
    void *(*resolve)(void *host, uint32_t address, uint32_t bytes, int writable);
    int (*protect)(void *host, uint32_t address, uint32_t bytes,
                   uint32_t protection, uint32_t *previous);
    uint32_t code_begin, ram_size, host_page_size;
} tsfp_guest_vm;

/* All status/results are guest-width. Input/output words may be unaligned.
 * Mandatory output addresses are nonzero. Validation precedes ownership and
 * output changes. Callers own buffers; these calls do not synchronize guest
 * writes or mapping teardown. ZeroBits/protection allocation hints currently
 * have no effect: backing pages already exist in the host mapping.
 * Compatibility policy: nonzero base without RESERVE is an unchanged success;
 * pure RESERVE may shrink by halves after the full heap/reservation attempts.
 * This is NOT demand-paged Xbox VM emulation. */
uint32_t tsfp_guest_vm_allocate(const tsfp_guest_vm *, uint32_t base_word,
    uint32_t zero_bits, uint32_t size_word, uint32_t type, uint32_t protection);
/* RELEASE reclaims an exact heap/reservation owner, clearing supplied outputs.
 * Unowned addresses retain historical success/clear behavior. DECOMMIT and
 * legacy mode retain storage; they do not revoke backing or zero memory. */
uint32_t tsfp_guest_vm_free(const tsfp_guest_vm *, uint32_t base_word,
    uint32_t size_word, uint32_t type, int legacy);
/* Exactly seven little-endian words, not the host's pointer-width record.
 * Retains flat RAM query policy, independently of individual heap ownership. */
uint32_t tsfp_guest_vm_query(const tsfp_guest_vm *, uint32_t address, uint32_t output);
/* The optional old-protection output must lie outside affected host pages,
 * so removing write access cannot make result publication fault. The host
 * callback must leave protection unchanged on failure. */
uint32_t tsfp_guest_vm_protect(const tsfp_guest_vm *, uint32_t base_word,
    uint32_t size_word, uint32_t protection, uint32_t old_word);
#endif
