#ifndef TSFP_PC_NATIVE_WEAPON_BOB_H
#define TSFP_PC_NATIVE_WEAPON_BOB_H
void pc_native_weapon_bob_clock(void);
float pc_native_weapon_bob_divide(float distance,float original_interval);
#endif
#if defined(PC_NATIVE_WEAPON_BOB_IMPLEMENTATION) && !defined(PC_NATIVE_WEAPON_BOB_DEFINED)
#define PC_NATIVE_WEAPON_BOB_DEFINED
/* D5EB0 measures displacement BEFORE this frame's world movement. Retain
 * the preceding update's fractional interval, independently for four players.
 * Never replace the guest's integer counters or change its simulation clock. */
static struct {
    uint32_t player,entity,actor,level,tick;
    float previous,next;
} pc_bob_clocks[4];
static unsigned pc_bob_replace;
void pc_native_weapon_bob_clock(void)
{
    uint32_t player=*(const uint32_t *)((uintptr_t)0x7B0C7Cu+g_xbox_mem_offset);
    if(!player)return;
    unsigned slot=4;
    for(unsigned i=0;i<4;i++)if(pc_bob_clocks[i].player==player){slot=i;break;}
    if(slot==4)slot=pc_bob_replace++%4;
    float delta=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    uint32_t entity=*(const uint32_t *)((uintptr_t)(player+0x14u)+g_xbox_mem_offset);
    uint32_t actor=entity?*(const uint32_t *)((uintptr_t)(entity+0x7Cu)+g_xbox_mem_offset):0;
    uint32_t level=*(const uint32_t *)((uintptr_t)0x7DE30Cu+g_xbox_mem_offset);
    uint32_t tick=*(const uint32_t *)((uintptr_t)0x7DE318u+g_xbox_mem_offset);
    int valid=g_tsfp_native_timing_active && delta>0.f && isfinite(delta);
    int same=valid && pc_bob_clocks[slot].player==player &&
        pc_bob_clocks[slot].entity==entity && pc_bob_clocks[slot].actor==actor &&
        pc_bob_clocks[slot].level==level && tick>=pc_bob_clocks[slot].tick;
    pc_bob_clocks[slot].previous=same?pc_bob_clocks[slot].next:0.f;
    pc_bob_clocks[slot].next=valid?delta:0.f;
    pc_bob_clocks[slot].player=player;pc_bob_clocks[slot].entity=entity;
    pc_bob_clocks[slot].actor=actor;pc_bob_clocks[slot].level=level;pc_bob_clocks[slot].tick=tick;
}
float pc_native_weapon_bob_divide(float distance,float original_interval)
{
    if(g_tsfp_native_timing_active) {
        uint32_t player=*(const uint32_t *)((uintptr_t)0x7B0C7Cu+g_xbox_mem_offset);
        for(unsigned i=0;i<4;i++)if(pc_bob_clocks[i].player==player) {
            float interval=pc_bob_clocks[i].previous;
            if(interval>0.f && interval<1.f)return distance/interval;
            break;
        }
    }
    return distance/original_interval;
}
#endif
