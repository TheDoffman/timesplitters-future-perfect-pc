#ifndef TSFP_PC_DISPLAY_MEMORY_H
#define TSFP_PC_DISPLAY_MEMORY_H
#include "pc_guest_memory.h"

/* Caller-owned snapshot for one uninterrupted camera/projection correction.
 * Initialize after the original guest call; never retain across guest calls,
 * callbacks, allocations or protection changes. The OS query still supplies
 * the complete mapping bounds and write permission. */
static inline int pc_display_writable(PcGuestRegion *region,uintptr_t offset,
                                      uint32_t address,size_t bytes)
{
    uintptr_t host;
    size_t available;
    if(!address || bytes>UINT32_MAX-address || address>UINTPTR_MAX-offset)
        return 0;
    host=offset+address;
    if(bytes>UINTPTR_MAX-host) return 0;
    available=pc_guest_region_remaining(region,host,1);
    if(!available || bytes>available) {
        *region=pc_guest_query_region(host);
        available=pc_guest_region_remaining(region,host,1);
    }
    return available && bytes<=available;
}
#endif
