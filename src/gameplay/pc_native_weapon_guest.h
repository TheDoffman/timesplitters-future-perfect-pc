#ifndef TSFP_PC_NATIVE_WEAPON_GUEST_H
#define TSFP_PC_NATIVE_WEAPON_GUEST_H
#include <stddef.h>
#include <stdint.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
/* US sub_00244F30 and sub_001BB5C0 are discrete firing transactions: their
 * whole-tick gates, integer cooldowns and projectile loops retain the
 * original cadence. Event timers and nested sound helpers consume that span,
 * rather than just the fractional interval of the frame carrying the tick.
 * Scope this conversion to these audited transactions. Animation/movement and
 * other callers regain the live fractional delta on return. */
static void pc_native_weapon_fire_guest(void (*original)(void))
{
    if (!g_tsfp_native_timing_active) {
        original();
        return;
    }
    uint32_t *ticks=(uint32_t *)((uintptr_t)0x4E7944u+g_xbox_mem_offset);
    float *delta=(float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    uint32_t saved_ticks=*ticks;
    float saved_delta=*delta;
    if (!(saved_delta>0)) *ticks=0;
    *delta=(float)*ticks;
    original();
    *ticks=saved_ticks;
    *delta=saved_delta;
}
/* These two US callers consume a crossed animation marker (0x200), then
 * clear their firing flags on the same callback. It cannot be retried on
 * the next whole tick. Treat that single event as one firing transaction;
 * all held-fire callers retain their original whole-tick cadence. */
static void pc_native_npc_weapon_fire_guest(void (*original)(void),
                                            uint32_t return_address)
{
    uint32_t *ticks=(uint32_t *)((uintptr_t)0x4E7944u+g_xbox_mem_offset);
    const float *delta=(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    int event=g_tsfp_native_timing_active && *ticks==0 && *delta>0 &&
              (return_address==0x27AE66u || return_address==0x2B9BF8u);
    if (event) *ticks=1;
    pc_native_weapon_fire_guest(original);
    if (event) *ticks=0;
}
#endif
