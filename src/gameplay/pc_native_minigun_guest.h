#ifndef TSFP_PC_NATIVE_MINIGUN_GUEST_H
#define TSFP_PC_NATIVE_MINIGUN_GUEST_H
#include "pc_guest_span.h"
#include <math.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
typedef struct {uint32_t hand,saved;ptrdiff_t offset;} PcMinigunIntent;
static uintptr_t pc_minigun_read(uint32_t address,size_t bytes,ptrdiff_t offset,PcGuestRegion *region)
{
    uintptr_t p=(uintptr_t)address+offset;
    return address && pc_guest_memory_in_span(region,p,bytes,0)?p:0;
}
/* US 42760 controller snapshot: held bits minus inhibited buttons. */
static int pc_minigun_buttons(uint32_t id,ptrdiff_t offset,PcGuestRegion *r,uint32_t *buttons)
{
    uintptr_t inhibit=pc_minigun_read(0x6b9888,4,offset,r),mode=pc_minigun_read(0x76b0fc,4,offset,r);
    if(!inhibit || !mode || id>=4)return 0;
    if(*(uint32_t*)inhibit){*buttons=0;return 1;}
    uint32_t index=id,base=0x6b94c8;
    if(!*(uint32_t*)mode) {
        uintptr_t map=pc_minigun_read(0x6b93c0+id*4,4,offset,r);
        if(!map)return 0;
        index=*(uint32_t*)map;base=0x6b93d0;
    }
    if(index>=4)return 0;
    uintptr_t pad=pc_minigun_read(base+index*0x3c,0x3c,offset,r);
    if(!pad)return 0;
    *buttons=*(uint32_t*)(pad+0xc)&~*(uint32_t*)(pad+0x38);
    return 1;
}
/* The minigun motor consumes spin intent every native frame, but the original
 * readiness callback only refreshes +64 on authored firing ticks. Supply held
 * primary input for this motor call; keep actual shot/cooldown transactions
 * on their existing whole ticks. No cached intent survives release/reboarding. */
static PcMinigunIntent pc_native_minigun_prepare(uint32_t frame,ptrdiff_t offset)
{
    PcGuestRegion r={0};PcMinigunIntent none={0};
    if(!g_tsfp_native_timing_active)return none;
    uintptr_t f=pc_minigun_read(frame,16,offset,&r),clock=pc_minigun_read(0x4e794c,4,offset,&r);
    uintptr_t current=pc_minigun_read(0x7b0c7c,4,offset,&r);
    if(!f || *(uint32_t*)(f+4)!=1 || !clock || !current)return none;
    float delta=*(float*)clock;if(!isfinite(delta) || delta<=0)return none;
    uint32_t player=*(uint32_t*)current,hand=*(uint32_t*)(f+8),state=*(uint32_t*)(f+12);
    uintptr_t p=pc_minigun_read(player,0x1c,offset,&r);
    if(!p || *(uint32_t*)(p+4)!=0 || player>UINT32_MAX-0x55c ||
       (hand!=player+0x184 && hand!=player+0x55c))return none;
    uintptr_t h=pc_minigun_read(hand,0x3d0,offset,&r),s=pc_minigun_read(state,0x34,offset,&r);
    if(!h || !s || *(uint32_t*)(h+0x30)!=7 || *(uint32_t*)(s+0x30)!=7 ||
       *(uint32_t*)(h+0x3cc)!=state || *(uint32_t*)(h+0x44)!=1 ||
       !pc_guest_memory_in_span(&r,h+0x64,4,1))return none;
    uint32_t buttons;if(!pc_minigun_buttons(*(uint32_t*)p,offset,&r,&buttons))return none;
    PcMinigunIntent scope={hand,*(uint32_t*)(h+0x64),offset};
    *(uint32_t*)(h+0x64)=!!(buttons&*(uint32_t*)(p+0x18));
    return scope;
}
static void pc_native_minigun_restore(PcMinigunIntent scope)
{
    if(!scope.hand)return;
    PcGuestRegion r={0};uintptr_t h=pc_minigun_read(scope.hand,0x68,scope.offset,&r);
    if(h && *(uint32_t*)(h+0x30)==7 && pc_guest_memory_in_span(&r,h+0x64,4,1))
        *(uint32_t*)(h+0x64)=scope.saved;
}
#endif
