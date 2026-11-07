#ifndef TSFP_PC_NATIVE_UPLINK_H
#define TSFP_PC_NATIVE_UPLINK_H
/* The audited callers have already dereferenced this hand and linked state.
 * Check their identities before changing the gate; this is not a host pointer
 * discovery API. Keep host window headers out of generated numeric policy.
 * Admit only Uplink physics mode's continuous function-1 hold.
 * Neither the shared clock nor any integer shot/cooldown consumer is changed. */
static inline uint32_t pc_native_uplink_gate(uint32_t hand,uint32_t value)
{
    if(!g_tsfp_native_timing_active || value)return value;
    float delta=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    if(!(delta>0.f) || !isfinite(delta))return value;
    if(!hand || hand>UINT32_MAX-0x3D0u)return value;
    uintptr_t h=(uintptr_t)hand+g_xbox_mem_offset;
    if(*(const uint32_t *)(h+0x30)!=36 ||
       *(const uint32_t *)(h+0x38)!=1 ||
       *(const uint32_t *)(h+0x5C)!=1)return value;
    uint32_t state=*(const uint32_t *)(h+0x3CC);
    uintptr_t s=(uintptr_t)state+g_xbox_mem_offset;
    return state && state<=UINT32_MAX-0x34u &&
           *(const uint32_t *)(s+0x30)==36 ? 1u : value;
}
/* US350320 releases a single force through 351AA0/16C8F0. The body solver
 * integrates and clears it on the next native interval. Preserve its authored
 * one-60-Hz-frame impulse, including original slow-motion scaling; continuous
 * pull forces and unrelated rigid-body forces must not receive this multiplier. */
static inline float pc_native_uplink_throw(float strength,float scale)
{
    if(g_tsfp_native_timing_active) {
        float raw=*(const float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
        float game=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if(raw>0.f && raw<1.f && game>0.f && isfinite(game))
            return (strength*scale)/raw;
    }
    return strength*scale;
}
#endif
