/* Project ownership for native Mm allocations. Never stores guest addresses. */
#ifndef TSFP_HOST_BLOCKS_H
#define TSFP_HOST_BLOCKS_H
#include <stddef.h>
#include <stdint.h>

typedef struct tsfp_host_block {
    void *mapping, *address;
    size_t mapping_size, bytes;
    uint32_t kind;
} tsfp_host_block;

typedef struct tsfp_host_block_backend {
    void *context;
    /* Return zeroed storage with page_size alignment and the requested access,
     * or NULL without ownership. release returns true only after destroying
     * that exact mapping. Callbacks must not reenter the same owner. */
    void *(*map)(void *, size_t bytes, uint32_t protection);
    int (*unmap)(void *, void *mapping, size_t bytes);
} tsfp_host_block_backend;

typedef struct tsfp_host_blocks {
    tsfp_host_block_backend backend;
    tsfp_host_block *records;
    size_t capacity, live, page_size;
    unsigned active;
#ifdef _MSC_VER
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_host_blocks;

/* Start with zeroed state. Init/reinit requires quiescent callers and refuses
 * active or still-owned storage. Metadata and callbacks must remain alive.
 * Invalid init changes neither state nor metadata. page_size is a power of 2. */
int tsfp_host_blocks_init(tsfp_host_blocks *, const tsfp_host_block_backend *,
    size_t page_size, tsfp_host_block *records, size_t capacity);
/* kind is nonzero and selected by the Mm service. Alignment 0 uses host pages;
 * any other alignment must be a power of two. A zero-byte request owns one
 * byte of backing but reports logical size zero. Callers that forbid empty
 * allocations must reject those requests before this engine. Overflow and
 * capacity failure never call the backend or publish an untracked block. */
void *tsfp_host_block_allocate(tsfp_host_blocks *, size_t bytes,
    size_t alignment, uint32_t protection, uint32_t kind);
/* Release an exact returned address of matching kind; kind0 accepts any kind.
 * Failed host release retains ownership for retry. Foreign/interior/double
 * releases do not reach the backend. Records disappear immediately on success.
 * Addresses are not generation tokens: after host address reuse, a stale exact
 * pointer cannot be distinguished from the new owner. Callers must not retain
 * or release pointers after their allocation lifetime. */
int tsfp_host_block_release(tsfp_host_blocks *, void *address, uint32_t kind);
/* Snapshot metadata for an interior requested byte (or exact empty-block base).
 * Padding is not part of the public allocation. Output unchanged on failure.
 * Snapshot does not extend storage lifetime against concurrent release. */
int tsfp_host_block_find(tsfp_host_blocks *, const void *address, tsfp_host_block *out);
/* Stop admissions and try every owned mapping once, retaining failed releases.
 * Return remaining owned count. Call again to retry; it does not join users of
 * the mappings. Caller must establish quiescence before teardown. */
size_t tsfp_host_blocks_stop(tsfp_host_blocks *);
#endif
