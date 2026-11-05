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
#endif
