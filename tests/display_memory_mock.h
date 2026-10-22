/* Camera fixtures model a single writable 8 MiB guest mapping. Separate
 * real-VM tests cover protection and mapping boundaries on each host. */
#define TSFP_PC_GUEST_MEMORY_H
typedef struct PcGuestRegion { uintptr_t base; size_t size; int writable; } PcGuestRegion;
static unsigned display_region_queries;
static PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    PcGuestRegion region={(uintptr_t)g_xbox_mem_offset,0x800000,1};
    ++display_region_queries;
    if(address<region.base || address-region.base>=region.size)region.size=0;
    return region;
}
static size_t pc_guest_region_remaining(const PcGuestRegion *r,uintptr_t a,int writing)
{
    if((writing&&!r->writable) || a<r->base || a-r->base>=r->size)return 0;
    return r->size-(a-r->base);
}
#include "pc_display_memory.h"
