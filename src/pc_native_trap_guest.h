#ifndef TSFP_PC_NATIVE_TRAP_GUEST_H
#define TSFP_PC_NATIVE_TRAP_GUEST_H
#include <stddef.h>
#include <stdint.h>
#include <math.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
static inline float pc_native_trap_fraction(void)
{
    if(!g_tsfp_native_timing_active)return 0.f;
    float h=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    return h>0.f && h<1.f?h:0.f;
}
static inline float pc_native_trap_ticks(int32_t whole)
{
    float h=pc_native_trap_fraction();
    return h>0.f?h:(float)whole;
}
/* Conjugate the authored kick/drift map by a half kick. A fractional symmetric
 * step is K((h-1)/2) D(h) K((h+1)/2). At h=1 this is the original D(1) K(1).
 * Both kicks sample the original quantized sine table at their own angle.
 * No cached poses, retained state or changed whole/catch-up arithmetic. */
static inline float pc_native_trap_force(float impulse,float velocity)
{
    float h=pc_native_trap_fraction();
    return h>0.f?velocity+impulse*((h+1.f)*.5f):impulse+velocity;
}
static inline float pc_native_trap_travel(float travel,float angle,uint32_t entity)
{
    float result=travel+angle,h=pc_native_trap_fraction();
    if(h>0.f) {
        float index=result * *(const float *)((uintptr_t)0x475D0Cu+g_xbox_mem_offset);
        index*=*(const float *)((uintptr_t)0x475CE0u+g_xbox_mem_offset);
        index+=*(const float *)((uintptr_t)0x475CD4u+g_xbox_mem_offset);
        if(isfinite(index) && index>=-2147483648.f && index<2147483648.f) {
            uint32_t slot=((uint32_t)(int32_t)index)&2047u;
            float impulse=*(const float *)((uintptr_t)(0x7A38A0u+slot*4u)+g_xbox_mem_offset);
            impulse*=*(const float *)((uintptr_t)0x475D28u+g_xbox_mem_offset);
            float *velocity=(float *)((uintptr_t)(entity+0xE0u)+g_xbox_mem_offset);
            *velocity+=(h-1.f)*.5f*impulse;
        }
    }
    return result;
}
static inline void pc_native_trap_contact_store(uint32_t address)
{
    if(pc_native_trap_fraction()>0.f) {
        uint32_t *count=(uint32_t *)((uintptr_t)address+g_xbox_mem_offset);
        uint32_t whole=*(const uint32_t *)((uintptr_t)0x4E7944u+g_xbox_mem_offset);
        /* Replace only the increment just stored at 1D8BFD. Sound's first
         * increment remains original. Subframe contacts spend carried ticks
         * so they cannot exhaust the authored lifetime early. */
        *count=*count-1u+whole;
    }
}
#endif
