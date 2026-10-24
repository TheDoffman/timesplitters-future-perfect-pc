#ifndef TSFP_PC_CAMERA_H
#define TSFP_PC_CAMERA_H
#include "pc_guest_memory.h"
#include <math.h>
/* Windows reports a forward span from the queried page. Start at the lower
 * known field so ordinary camera access can share one fresh mapping query.
 * This is only a hint: every consumer still validates its own span/access.
 * Keep the snapshot local to one mouse update, with no intervening guest call. */
static inline PcGuestRegion pc_camera_mouse_region(uint32_t player,uint32_t fov_object,ptrdiff_t offset)
{
    PcGuestRegion empty={0};
    if(!player)return empty;
    uintptr_t first=(uintptr_t)player+offset;
    if(fov_object) {
        uintptr_t field=(uintptr_t)fov_object+offset+0x384;
        if(field<first)first=field;
    }
    return pc_guest_query_region(first);
}
/* Verified US Xbox player/FOV fields, shared by host input adapters.
 * The guest wrapper supplies the current player. Its first word is a guest
 * player ID, not a local input port: network clients can have nonzero IDs. */
static inline int pc_camera_fields_in_region(uint32_t player,uint32_t fov_object,ptrdiff_t offset,
                                    float **yaw,float **pitch,float *fov,PcGuestRegion *region)
{
    uintptr_t p=(uintptr_t)player+offset,f=(uintptr_t)fov_object+offset;
    if(!player || !fov_object || !pc_guest_memory_in_region(region,p,0x108,1) ||
       !pc_guest_memory_in_region(region,f+0x384,4,0))return 0;
    *yaw=(float*)(p+0x100);*pitch=(float*)(p+0x104);*fov=*(const float*)(f+0x384);
    return isfinite(*fov) && *fov>=1 && *fov<=120;
}
static inline int pc_camera_fields(uint32_t player,uint32_t fov_object,ptrdiff_t offset,
                                    float **yaw,float **pitch,float *fov)
{
    PcGuestRegion region={0};
    return pc_camera_fields_in_region(player,fov_object,offset,yaw,pitch,fov,&region);
}
#endif
