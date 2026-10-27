#ifndef TSFP_PC_NATIVE_PHYSICS_GUEST_H
#define TSFP_PC_NATIVE_PHYSICS_GUEST_H
#include <stdint.h>
#include <stddef.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
#ifndef PC_NATIVE_PHYSICS_OBSERVE
#define PC_NATIVE_PHYSICS_OBSERVE() ((void)0)
#endif
#if defined(_MSC_VER)
#define PC_PHYSICS_TLS __declspec(thread)
#else
#define PC_PHYSICS_TLS _Thread_local
#endif

typedef struct PcPhysicsRandom {
    uint32_t low,high,count;
    int ready;
} PcPhysicsRandom;
typedef struct PcPhysicsScope {
    uint32_t whole,step;
    PcPhysicsRandom *random;
} PcPhysicsScope;
static PC_PHYSICS_TLS PcPhysicsScope *pc_physics_scope;
static uint32_t *pc_physics_word(uint32_t address)
{ return (uint32_t *)((uintptr_t)address+g_xbox_mem_offset); }

/* US 171FB0 partitions each live interval into ceil(delta) primary steps.
 * Keep only the carried whole-tick count of those steps on the shared RNG.
 * A stack scope also isolates recursive updates and leaves no saved state
 * across levels, pause, or changes between native and original timing. */
static void pc_native_physics_guest(void (*original)(void))
{
    PcPhysicsScope *saved=pc_physics_scope;
    uint32_t whole=*pc_physics_word(0x4E7944u);
    PcPhysicsScope scope={ (int32_t)whole>0 ? whole : 0,0,NULL };
    pc_physics_scope=g_tsfp_native_timing_active ? &scope : NULL;
    original();
    pc_physics_scope=saved;
}

/* Exact call 172114 -> 171560. Each fractional primary step still performs
 * every secondary solve and shuffle, using an advancing private RNG sequence.
 * Authored primary steps retain their original shared random side effects. */
static void pc_native_physics_solver_guest(void (*original)(void))
{
    PC_NATIVE_PHYSICS_OBSERVE();
    PcPhysicsScope *scope=pc_physics_scope;
    if(!g_tsfp_native_timing_active || !scope) {original();return;}
    PcPhysicsRandom random={0,0,0,0},*saved=scope->random;
    scope->random=scope->step++>=scope->whole ? &random : NULL;
    original();
    scope->random=saved;
}

/* Only shuffle sites 1715A0 and 170E51 call this helper. Other gameplay RNG
 * consumers remain original, even when called from inside the solver.
 * Run the original generator (153B40), preserving its return/register/stack
 * behavior. Restore its two seed words and diagnostic counter afterwards. */
static void pc_native_physics_random_guest(void (*original)(void))
{
    PcPhysicsRandom *random=pc_physics_scope ? pc_physics_scope->random : NULL;
    if(!g_tsfp_native_timing_active || !random) {original();return;}
    uint32_t *low=pc_physics_word(0x4E7950u),*high=pc_physics_word(0x4E7954u);
    uint32_t *count=pc_physics_word(0x7497C8u);
    uint32_t saved_low=*low,saved_high=*high,saved_count=*count;
    if(!random->ready) {
        random->low=saved_low;random->high=saved_high;
        random->count=saved_count;random->ready=1;
    }
    *low=random->low;*high=random->high;*count=random->count;
    original();
    random->low=*low;random->high=*high;random->count=*count;
    *low=saved_low;*high=saved_high;*count=saved_count;
}
#endif
