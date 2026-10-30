#ifndef TSFP_PC_CAMERA_H
#define TSFP_PC_CAMERA_H
#include "pc_guest_span.h"
#include <math.h>
/* Start a fresh bounded query at the lower known field. A full-region fallback
 * may cover both fields; resident-page results need separate checks elsewhere.
 * This is only a hint: every consumer still validates its own span/access.
 * Keep the snapshot local to one mouse update, with no intervening guest call. */
static inline PcGuestRegion pc_camera_mouse_region(uint32_t player,uint32_t fov_object,ptrdiff_t offset)
{
    PcGuestRegion empty={0};
    if(!player)return empty;
    uintptr_t first=(uintptr_t)player+offset;
    size_t bytes=0x118; /* Mode, local slot and ordinary camera fields. */
    if(fov_object) {
        uintptr_t field=(uintptr_t)fov_object+offset+0x384;
        if(field<first){first=field;bytes=4;}
    }
    return pc_guest_query_span(first,bytes);
}
/* Keyboard and mouse belong to local input port 1 (slot zero). The player's
 * first word is a game/network ID and cannot identify the local input port.
 * Do not consume motion while another split-screen player's camera updates. */
static inline int pc_camera_mouse_primary_in_region(uint32_t player,ptrdiff_t offset,
                                                     PcGuestRegion *region)
{
    uintptr_t p=(uintptr_t)player+offset;
    return player && pc_guest_memory_in_span(region,p,8,0) &&
           *(const uint32_t*)(p+4)==0;
}
/* Verified US Xbox player/FOV fields, shared by host input adapters.
 * The guest wrapper supplies the current player. Its first word is a guest
 * player ID, not a local input port: network clients can have nonzero IDs. */
static inline int pc_camera_fields_in_region(uint32_t player,uint32_t fov_object,ptrdiff_t offset,
                                    float **yaw,float **pitch,float *fov,PcGuestRegion *region)
{
    uintptr_t p=(uintptr_t)player+offset,f=(uintptr_t)fov_object+offset;
    if(!player || !fov_object || !pc_guest_memory_in_span(region,p,0x108,1) ||
       !pc_guest_memory_in_span(region,f+0x384,4,0))return 0;
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
