/* US tracked driver: compensate camera-relative stick interpretation without
 * changing the camera, authored track forces, weapon ray or controller input. */
#ifndef TSFP_PC_TANK_H
#define TSFP_PC_TANK_H
#include "pc_mounted.h"
typedef struct PcTankDriverScope { uintptr_t actor_link;uint32_t player,actor,handle;ptrdiff_t offset; } PcTankDriverScope;
static inline PcTankDriverScope pc_tank_driver_input(uint32_t packet,uint32_t vehicle,uint32_t player,
                                      ptrdiff_t offset,PcGuestRegion *region)
{
    uintptr_t p=pc_mounted_read(player,offset,0x118,region);
    uintptr_t v=pc_mounted_read(vehicle,offset,0x84,region);
    uintptr_t in=pc_mounted_read(packet,offset,0x20,region);
    if(!p || !v || !in || *(uint32_t*)(p+4) || *(uint32_t*)(p+0x114)!=0x400 ||
       !pc_guest_memory_in_span(region,in,0x10,1))return (PcTankDriverScope){0};
    uintptr_t entity=pc_mounted_read(*(uint32_t*)(p+0x14),offset,0x154,region);
    if(!entity || *(uint32_t*)(entity+0x2c)!=*(uint32_t*)(in+0x1c))return (PcTankDriverScope){0};
    uintptr_t vd=pc_mounted_read(*(uint32_t*)(v+0x7c),offset,0x168,region);
    if(!vd || !(*(uint32_t*)(vd+0x58)&1))return (PcTankDriverScope){0};
    uintptr_t data=0;uint32_t gun=0;
    if(pc_vehicle_gun(p,offset,region,&data,&gun)!=2)return (PcTankDriverScope){0};
    uintptr_t extra=pc_mounted_read(*(uint32_t*)(entity+0x7c),offset,0xc0c,region);
    if(!extra || *(uint32_t*)(extra+0xc08)!=*(uint32_t*)(v+0x2c))return (PcTankDriverScope){0};
    if(*(uint32_t*)(entity+0x150)!=player ||
       !pc_guest_memory_in_span(region,entity+0x150,4,1))return (PcTankDriverScope){0};
    float x=*(float*)in,y=*(float*)(in+4);
    if(!isfinite(x) || !isfinite(y))return (PcTankDriverScope){0};
    /* The original player branch turns both tracks toward a camera-relative
     * heading, even for backwards input. Its AI branch already accepts signed
     * left/right tracks. Select that branch only during this synchronous call;
     * keep the original force, wheel, wake and callback code. */
    float magnitude=fmaxf(1,fmaxf(fabsf(y+x),fabsf(y-x)));
    /* Steering sign confirmed by first-tank playtest: D reverses this pair. */
    *(float*)(in+4)=(y+x)/magnitude;
    *(float*)(in+12)=(y-x)/magnitude;
    uintptr_t cam=pc_mounted_read(*(uint32_t*)(p+8),offset,0xfe0,region);
    if(cam && pc_guest_memory_in_span(region,cam+0xfdc,4,1) && isfinite(*(float*)(p+0x104)))
        *(float*)(cam+0xfdc)=*(float*)(p+0x104);
    *(uint32_t*)(entity+0x150)=0;
    return (PcTankDriverScope){entity+0x150,player,*(uint32_t*)(p+0x14),*(uint32_t*)(entity+0x2c),offset};
}
static inline void pc_tank_driver_restore(PcTankDriverScope scope)
{
    if(!scope.actor_link)return;
    PcGuestRegion region={0};
    uintptr_t actor=pc_mounted_read(scope.actor,scope.offset,0x154,&region);
    uintptr_t p=pc_mounted_read(scope.player,scope.offset,0x18,&region);
    if(actor && p && *(uint32_t*)(actor+0x2c)==scope.handle &&
       *(uint32_t*)(p+0x14)==scope.actor && !*(uint32_t*)(actor+0x150) &&
       pc_guest_memory_in_span(&region,actor+0x150,4,1))*(uint32_t*)(actor+0x150)=scope.player;
}
#endif
