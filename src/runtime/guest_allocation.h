/* Project guest allocation ownership. Host mappings belong to the caller. */
#ifndef TSFP_GUEST_ALLOCATION_H
#define TSFP_GUEST_ALLOCATION_H
#include <stddef.h>
#include <stdint.h>

typedef struct tsfp_allocation_record {
    uint32_t begin, end, address, bytes, kind;
} tsfp_allocation_record;

typedef struct tsfp_allocation_arena {
    unsigned char *memory;
    uint32_t origin, begin, end;
    tsfp_allocation_record *records;
    size_t count, capacity;
} tsfp_allocation_arena;

typedef struct tsfp_allocations {
    tsfp_allocation_arena heap, physical;
    uint32_t reserve_begin;
    unsigned active;
#if defined(_MSC_VER)
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_allocations;

typedef struct tsfp_allocation_config {
    void *ram;
    uint32_t ram_size, heap_begin, reserve_begin;
    /* NULL physical disables that arena; its integer fields must be zero. */
    void *physical;
    uint32_t physical_base, physical_size, physical_begin, physical_end;
    /* Optional permanent exclusion, e.g. the runtime's synthetic kernel page. */
    uint32_t protected_begin, protected_end;
} tsfp_allocation_config;

/* Context must initially be zeroed. Init/reinit requires quiescent callers;
 * all other operations serialize metadata and allocation zeroing. Supplied
 * writable mappings and record arrays must remain alive until stop completes.
 * Each arena needs at least one record. Init never writes guest memory and
 * leaves context/record arrays unchanged on invalid input. Valid reinit drops
 * ownership, so it belongs only to a new guest-memory lifetime. */
int tsfp_allocations_init(tsfp_allocations *state,
    const tsfp_allocation_config *config,
    tsfp_allocation_record *heap_records, size_t heap_capacity,
    tsfp_allocation_record *physical_records, size_t physical_capacity);
void tsfp_allocations_stop(tsfp_allocations *state);

/* Aligned lowest-fit allocations; every live ownership span is disjoint.
 * Heap uses minimum size 16/alignment 4 and zeros the complete returned size.
 * Reservations use default alignment 4096, no zeroing or size-query API,
 * and share heap ownership above reserve_begin. Zero-size reservations fail.
 * Physical blocks own page-rounded spans and zero only requested bytes; a
 * zero-size request still owns one page. Non-power-of-two alignments above
 * the minimum fail. Exhaustion/overflow/capacity failure has no side effects. */
uint32_t tsfp_heap_allocate(tsfp_allocations *, uint32_t size, uint32_t alignment);
uint32_t tsfp_reserve_allocate(tsfp_allocations *, uint32_t size, uint32_t alignment);
uint32_t tsfp_physical_allocate(tsfp_allocations *, uint32_t size, uint32_t alignment);
/* Inclusive guest-address bounds apply to requested bytes (one byte for a
 * zero-size allocation). Page padding may extend past high, but never past
 * the usable physical window or into another owner. */
uint32_t tsfp_physical_allocate_range(tsfp_allocations *, uint32_t size,
    uint32_t alignment, uint32_t low, uint32_t high);

/* Fixed address may be below the ordinary physical arena floor, but must be
 * inside the mapped usable window. Page ownership excludes other live blocks
 * and protected storage. Validation precedes zeroing; returns address or 0.
 * Exact requested base is retained even for a non-page-aligned address. */
uint32_t tsfp_physical_allocate_at(tsfp_allocations *, uint32_t address, uint32_t size);
int tsfp_heap_release(tsfp_allocations *, uint32_t address);
/* Virtual-memory RELEASE accepts an exact heap or reservation base. It never
 * frees physical/protected ownership and does not change backing-page access. */
int tsfp_virtual_release(tsfp_allocations *, uint32_t address);
int tsfp_physical_release(tsfp_allocations *, uint32_t address);
/* Remaining requested bytes at an interior address, with heap minimum applied.
 * Physical padding, reservations and protected ranges return zero. */
uint32_t tsfp_allocation_remaining(tsfp_allocations *, uint32_t address);
/* Ascending-address live physical blocks only. Each call is coherent; a whole
 * iteration is not a snapshot against concurrent allocate/free. Output values
 * stay unchanged on failure. Zero-size live blocks are returned with size 0. */
int tsfp_physical_block(tsfp_allocations *, size_t index, uint32_t *address, uint32_t *size);
/* Extent from physical origin through the highest live span, at least the
 * normal arena floor. This is address coverage, NOT a sum of live bytes. */
uint32_t tsfp_physical_extent(tsfp_allocations *);
#endif
