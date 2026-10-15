#ifndef TSFP_PC_CAMERA_H
#define TSFP_PC_CAMERA_H
#include "pc_guest_memory.h"
#include <math.h>
/* Verified US Xbox player/FOV fields, shared by host input adapters. */
static inline int pc_camera_fields(uint32_t player,uint32_t fov_object,ptrdiff_t offset,
                                    float **yaw,float **pitch,float *fov)
{
    uintptr_t p=(uintptr_t)player+offset,f=(uintptr_t)fov_object+offset;
    if(!player || !fov_object || !pc_guest_memory(p,0x108,1) ||
       !pc_guest_memory(f+0x384,4,0) || *(const uint32_t*)p!=0)return 0;
    *yaw=(float*)(p+0x100);*pitch=(float*)(p+0x104);*fov=*(const float*)(f+0x384);
    return isfinite(*fov) && *fov>=1 && *fov<=120;
}
#endif
