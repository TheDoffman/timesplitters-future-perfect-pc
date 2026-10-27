#ifndef TSFP_PC_RELOAD_GUEST_H
#define TSFP_PC_RELOAD_GUEST_H
#include "pc_guest_span.h"
#include "pc_reload_input.h"
/* Verified US local-slot, playable-mode and weapon-state fields. This is a
 * read-only snapshot; the original guest owns reload eligibility and ammo. */
static inline void pc_reload_guest_context(PcReloadInput *r,uint32_t player,ptrdiff_t offset,
                                           uint64_t now,PcGuestRegion *region)
{
    uintptr_t p=(uintptr_t)player+offset;
    int eligible=0,reloading=0;
    if(player && pc_guest_memory_in_span(region,p,0x5a4,0)) {
        uint32_t slot=*(const uint32_t*)(p+4),mode=*(const uint32_t*)(p+0x114);
        /* Another split-screen player's update must not clear port 1's
         * pending keyboard action. Its own context still expires normally. */
        if(slot!=0)return;
        eligible=(mode&0x480e)!=0 && !(mode&0x4000);
        reloading=*(const uint32_t*)(p+0x5a0)==7 || *(const uint32_t*)(p+0x1c8)==7;
    }
    pc_reload_context(r,player,eligible,reloading,now);
}
#endif
