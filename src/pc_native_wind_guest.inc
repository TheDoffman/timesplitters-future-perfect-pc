/* Shared US1DDF10 procedural wind simulation and ownership observers.
 * Register/callback wrappers run even when the oscillator's whole gate skips.
 * Unsupported local time scopes retain the authored loop count and discard
 * fractional solver history. Allocation failure leaves the original path. */
#include "pc_native_wind_owners.h"
#include "pc_native_wind_clock.h"
static PcWindOwners wind_owners;
static void pc_native_wind_register_guest(void (*original)(void))
{
#define W_WORD(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
    uint32_t owner=W_WORD(g_esp+4u),slot=W_WORD(g_esp+8u),callback=W_WORD(g_esp+12u);
    original();
    if(slot==3)pc_wind_owner_register(&wind_owners,g_xbox_mem_offset,W_WORD(0x79094Cu),owner,
        callback==0x1DDF10u?W_WORD(owner+0x7Cu):0,
        callback==0x1DDF10u?W_WORD(owner+0x2Cu):0,callback==0x1DDF10u);
#undef W_WORD
}
static void pc_native_wind_callback_guest(void (*original)(void))
{
#define W_WORD(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define W_FLOAT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
    uint32_t owner=W_WORD(g_esp+4u);float h=W_FLOAT(0x4E794Cu);
    if(h!=0.f && (!g_tsfp_native_timing_active ||
       !pc_native_wind_clock_matches(&g_tsfp_native_step,h,W_WORD(0x4E7944u)))) {
        pc_wind_owners_context(&wind_owners,g_xbox_mem_offset,W_WORD(0x79094Cu));
        size_t i=pc_wind_owner_index(&wind_owners,owner);
        if(i<wind_owners.count && wind_owners.entries[i].owner==owner)
            pc_wind_owner_invalidate(wind_owners.entries+i);
    }
    original();
#undef W_WORD
#undef W_FLOAT
}
static void pc_native_wind_oscillator_guest(void (*original)(void))
{
    uint32_t *stack=&g_esp;ptrdiff_t memory=g_xbox_mem_offset;
    const int active=g_tsfp_native_timing_active;
#define NW_WORD(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+memory))
#define NW_FLOAT(a) (*(float *)((uintptr_t)(uint32_t)(a)+memory))
    const float h=NW_FLOAT(0x4E794Cu);
    if(active && h==0.f) { *stack+=4u;return; }
    const PcNativeStep *clock=&g_tsfp_native_step;
    const int64_t wind_phase_units=clock->game_phase,wind_delta_units=clock->game_units,wind_frequency=clock->units_per_tick;
    const uint32_t wind_level_tick=NW_WORD(0x7DE318u);
    const uint32_t owner=NW_WORD(*stack+4u),data=NW_WORD(owner+0x7Cu);
    const uint32_t whole=NW_WORD(0x4E7944u);
    PcWindOwner *entry=pc_wind_owner_begin(&wind_owners,memory,NW_WORD(0x79094Cu),
        owner,data,NW_WORD(owner+0x2Cu),wind_level_tick-whole,wind_phase_units,wind_frequency);

    if(!active || !(h>0.f && h<1.f)) {
        pc_wind_owner_invalidate(entry);
        original();return;
    }
    if(!entry || !pc_native_wind_clock_matches(clock,h,whole)) {
        pc_wind_owner_invalidate(entry);if(whole)original();else *stack+=4u;return;
    }
    PcNativeWindAxis first,second;
    PcWindHistory staged[2]={entry->axes[0],entry->axes[1]};
    const float drag=NW_FLOAT(data+0xD4u),restoring=NW_FLOAT(data+0xD0u);
    const float limit=NW_FLOAT(data+0xD8u),forcing=NW_FLOAT(data+0xCCu);
    if(pc_native_wind_axis(NW_FLOAT(data+0xE8u),NW_FLOAT(data+0xF0u),
           NW_FLOAT(*stack+20u)*forcing,drag,restoring,limit,h,whole,&first,&staged[0],(double)wind_phase_units/wind_frequency,(double)wind_delta_units/wind_frequency) &&
       pc_native_wind_axis(NW_FLOAT(data+0xECu),NW_FLOAT(data+0xF4u),
           NW_FLOAT(*stack+28u)*forcing,drag,restoring,limit,h,whole,&second,&staged[1],(double)wind_phase_units/wind_frequency,(double)wind_delta_units/wind_frequency)) {
        entry->axes[0]=staged[0];entry->axes[1]=staged[1];
        pc_wind_owner_end(entry,wind_level_tick-whole,wind_phase_units,wind_delta_units,wind_frequency);
        NW_FLOAT(data+0xE8u)=first.angle;NW_FLOAT(data+0xF0u)=first.velocity;
        NW_FLOAT(data+0xECu)=second.angle;NW_FLOAT(data+0xF4u)=second.velocity;
        *stack+=4u;
    } else {
        pc_wind_owner_invalidate(entry);
        if(whole)original();else *stack+=4u;
    }
#undef NW_WORD
#undef NW_FLOAT
}
