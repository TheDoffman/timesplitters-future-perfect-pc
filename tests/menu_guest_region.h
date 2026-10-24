/* Controlled mapping/protection service for the real menu-row scanner.
 * Other menu validation continues through the fixture's guest_writable. */
#define TSFP_PC_GUEST_MEMORY_H
typedef struct PcGuestRegion {
    uintptr_t base;
    size_t size;
    int writable;
} PcGuestRegion;
static unsigned region_queries;
static uint32_t region_limit=sizeof memory;
static uint32_t region_split;
static int region_writable=1;
static int region_second_writable=1;
static PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    ++region_queries;
    uintptr_t base=(uintptr_t)memory+0x1000;
    if(address<base || address>=(uintptr_t)memory+region_limit)
        return (PcGuestRegion){0};
    if(region_split) {
        uintptr_t split=(uintptr_t)memory+region_split;
        if(address<split)return (PcGuestRegion){base,split-base,region_writable};
        return (PcGuestRegion){split,region_limit-region_split,region_second_writable};
    }
    return (PcGuestRegion){base,region_limit-0x1000,region_writable};
}
static size_t pc_guest_region_remaining(const PcGuestRegion *r,uintptr_t address,int writing)
{
    if((writing && !r->writable) || address<r->base || address-r->base>=r->size)return 0;
    return r->size-(address-r->base);
}
