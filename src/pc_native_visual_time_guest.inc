#include "pc_native_visual_time.h"
static PcNativeVisualTime pc_native_level_visual_time;
void pc_native_visual_rebase(uint32_t value,double fraction)
{
    pc_native_level_visual_time=(PcNativeVisualTime){fraction,value,
        g_tsfp_native_timing_active && isfinite(fraction) && fraction>=0 && fraction<1};
}
uint32_t pc_native_visual_store(uint32_t site,uint32_t value)
{
    /* The six ordinary stores belong to mutually exclusive level-update
     * branches. All other direct writes (init/demo/network) are rebases.
     * Reading the live delta here respects temporary guest time scopes. */
    int increment=site==0x688BBu || site==0x68F8Eu || site==0x691F3u ||
                  site==0x69239u || site==0x69265u || site==0x69325u;
    uint32_t previous=*(const uint32_t *)((uintptr_t)0x7DE318u+g_xbox_mem_offset);
    uint32_t whole=*(const uint32_t *)((uintptr_t)0x4E7944u+g_xbox_mem_offset);
    float delta=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    pc_native_visual_time_store(&pc_native_level_visual_time,
        g_tsfp_native_timing_active,increment,previous,value,whole,delta);
    return value;
}
float pc_native_visual_sample(uint32_t value)
{
    return (float)pc_native_visual_time_sample(&pc_native_level_visual_time,value);
}
/* The US weapon dispatcher samples an integer-clock sine wave, then chooses
 * a new amplitude at an authored crossing. Only resample its final visual
 * value: changing the crossing would move shared RNG calls between frames. */
float pc_native_weapon_sway(float original)
{
    uint32_t tick=*(const uint32_t *)((uintptr_t)0x7DE318u+g_xbox_mem_offset);
    const PcNativeVisualTime *clock=&pc_native_level_visual_time;
    if(!clock->valid || clock->integer!=tick || clock->offset==0)return original;
    const float degrees=*(const float *)((uintptr_t)0x475D0Cu+g_xbox_mem_offset);
    const float scale=*(const float *)((uintptr_t)0x475CE0u+g_xbox_mem_offset);
    const float half=*(const float *)((uintptr_t)0x475CD4u+g_xbox_mem_offset);
    float phase=pc_native_visual_sample(tick)*degrees;
    phase=phase*scale+half;
    /* Preserve the original fallback if the signed conversion cannot fit. */
    if(!isfinite(phase) || phase>=2147483648.f || phase< -2147483648.f)return original;
    unsigned index=(uint32_t)(int32_t)phase&0x7ffu;
    return *(const float *)((uintptr_t)(0x7A38A0u+index*4u)+g_xbox_mem_offset)-half;
}
