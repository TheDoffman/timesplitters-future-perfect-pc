#ifndef TSFP_PC_NATIVE_WALK_GUEST_H
#define TSFP_PC_NATIVE_WALK_GUEST_H
#include <stdint.h>
#include <stddef.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
typedef struct PcWalkScope { int clear_slide; } PcWalkScope;
#if defined(_MSC_VER)
static __declspec(thread) PcWalkScope *pc_walk_scope;
#else
static _Thread_local PcWalkScope *pc_walk_scope;
#endif
static int pc_native_walk_fractional(void)
{
    float delta=*(const float *)((uintptr_t)0x4e794cu+g_xbox_mem_offset);
    return g_tsfp_native_timing_active && delta>0.f && delta<1.f;
}
static void pc_native_walk_guest(void (*original)(void))
{
    PcWalkScope scope={0},*saved=pc_walk_scope;
    pc_walk_scope=pc_native_walk_fractional()?&scope:NULL;
    original();pc_walk_scope=saved;
}
static void pc_native_walk_sweep_guest(void (*original)(void),uint32_t caller,uint32_t *result)
{
    original();
    /* Only the two final slide attempts in original C54A0 classify its final
     * normal comparison. Recursive sweeps and the primary hit are unrelated.
     * A later blocked attempt must replace an earlier clear result. */
    if(pc_walk_scope && (caller==0xc5781u || caller==0xc58b0u))
        pc_walk_scope->clear_slide=*result==0;
}
int pc_native_walk_accept_clear_slide(void)
{ return pc_walk_scope && pc_walk_scope->clear_slide; }
#endif
