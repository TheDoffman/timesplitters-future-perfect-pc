/* Supported guest Mm policy. Host mappings and allocation ownership are inputs. */
#ifndef TSFP_GUEST_MEMORY_SERVICES_H
#define TSFP_GUEST_MEMORY_SERVICES_H
#include "guest_virtual_memory.h"
#include "memory_statistics.h"

typedef struct tsfp_guest_mm {
    tsfp_guest_vm vm;
    uint32_t physical_base,physical_size,gpu_default;
    uint32_t (*query_access)(void *host,const void *address);
    int (*set_access)(void *host,void *address,uint32_t bytes,uint32_t protection);
    tsfp_memory_available available;
} tsfp_guest_mm;

enum tsfp_guest_mm_kind {
    TSFP_MM_CONTIGUOUS=1, TSFP_MM_SYSTEM, TSFP_MM_DEBUG, TSFP_MM_IO, TSFP_MM_STACK
};
/* Guest backing is already mapped. SYSTEM and DEBUG reject zero; IO/STACK
 * retain the heap minimum; CONTIGUOUS retains a zero-byte page owner. Allocation
 * protection and IO physical hints are intentionally not host mapping requests.
 * STACK returns base+requested length; release must receive its low address. */
uint32_t tsfp_guest_mm_allocate(const tsfp_guest_mm *,enum tsfp_guest_mm_kind,uint32_t bytes);
void tsfp_guest_mm_release(const tsfp_guest_mm *,enum tsfp_guest_mm_kind,uint32_t address);
uint32_t tsfp_guest_mm_contiguous_ex(const tsfp_guest_mm *,uint32_t bytes,
    uint32_t low,uint32_t high,uint32_t alignment);
uint32_t tsfp_guest_mm_size(const tsfp_guest_mm *,uint32_t address);
uint32_t tsfp_guest_mm_physical(const tsfp_guest_mm *,uint32_t address);
/* GPU instance claims retain the fixed tail-address convention and do not
 * allocate/zero or assert exclusive ownership. The default tail is reserved by
 * layout initialization. Larger bounded claims can overlap the ordinary arena,
 * as before; callers must coordinate their use. Reject size arithmetic overflow
 * and invalid padding output before any publication. UINT32_MAX uses default;
 * zero returns the representable one-past-window address, retaining old policy. */
uint32_t tsfp_guest_mm_gpu(const tsfp_guest_mm *,uint32_t bytes,uint32_t padding_word);
/* Complete guest output spans must resolve before publication. Word writes are
 * unaligned little-endian, independent of host struct layout. */
uint32_t tsfp_guest_mm_statistics(const tsfp_guest_mm *,uint32_t output);
/* Publish the supported self-referential MmGlobalData placeholder only after
 * validating its entire guest word. This is not a real kernel MM database. */
uint32_t tsfp_guest_mm_global(const tsfp_guest_mm *,uint32_t address);
/* Only mapped guest spans reach host page operations. Query null keeps zero;
 * other unresolved queries report NOACCESS. Protection returns bool to the
 * adapter, whose void guest ABI still returns zero. Mapping/access must remain
 * stable through callbacks; these operations do not join guest workers. */
uint32_t tsfp_guest_mm_query_access(const tsfp_guest_mm *,uint32_t address);
int tsfp_guest_mm_set_access(const tsfp_guest_mm *,uint32_t address,uint32_t bytes,
    uint32_t protection);

enum tsfp_guest_mm_advisory { TSFP_MM_NOOP, TSFP_MM_VALID_ADDRESS,
    TSFP_MM_AVAILABLE_PAGES, TSFP_MM_WRITE_CHECK };
/* Retained advisory/debug compatibility, not actual address/free-space proof:
 * valid-address says1, available-pages says0x10000, write-check echoes input,
 * and persist/locking/release-address hints have no effect. */
uint32_t tsfp_guest_mm_advisory(enum tsfp_guest_mm_advisory,uint32_t address);
#endif
