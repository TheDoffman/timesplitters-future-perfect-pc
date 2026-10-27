#ifndef TSFP_PC_NATIVE_PROJECTILE_GUEST_H
#define TSFP_PC_NATIVE_PROJECTILE_GUEST_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
#if defined(_MSC_VER)
#define PC_PROJECTILE_TLS __declspec(thread)
#else
#define PC_PROJECTILE_TLS _Thread_local
#endif

/* US 0xE8EB0 allocates 0xB8A0 bytes: 85 complete 0x22C projectile slots.
 * Keep visual random values outside guest objects. The kind-35 initializer
 * resets its slot on every reuse, including when native timing is disabled. */
typedef struct PcKind35Cache {
    uint32_t random[9];
    unsigned ready, target_ready, first_whole;
} PcKind35Cache;
typedef struct PcKind35Context {
    PcKind35Cache *cache;
    uint32_t shot;
} PcKind35Context;
static PcKind35Cache pc_kind35_cache[85];
static uint32_t pc_kind35_pool;
static PC_PROJECTILE_TLS PcKind35Context pc_kind35_context;
static uint32_t *pc_kind35_word(uint32_t address)
{ return (uint32_t *)((uintptr_t)address+g_xbox_mem_offset); }
static float pc_kind35_delta(void)
{ return *(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset); }
static void pc_kind35_clear(PcKind35Cache *cache)
{
    memset(cache,0,sizeof *cache);
    for(unsigned i=0;i<9;i++)cache->random[i]=0x80000000u;
    cache->ready=1;
}
static PcKind35Cache *pc_kind35_slot(uint32_t shot)
{
    uint32_t base=*pc_kind35_word(0x737BD4u);
    if(!base || shot<base || (shot-base)%0x22Cu || (shot-base)/0x22Cu>=85)
        return NULL;
    if(base!=pc_kind35_pool) {
        memset(pc_kind35_cache,0,sizeof pc_kind35_cache);
        pc_kind35_pool=base;
    }
    PcKind35Cache *cache=&pc_kind35_cache[(shot-base)/0x22Cu];
    if(!cache->ready)pc_kind35_clear(cache);
    return cache;
}
static void pc_native_kind35_init_guest(void (*original)(void),uint32_t shot)
{
    PcKind35Cache *cache=pc_kind35_slot(shot);
    if(cache)pc_kind35_clear(cache);
    original();
}
static void pc_native_kind35_update_guest(void (*original)(void),uint32_t shot)
{
    PcKind35Context saved=pc_kind35_context;
    pc_kind35_context.cache=NULL;
    pc_kind35_context.shot=shot;
    if(g_tsfp_native_timing_active) {
        PcKind35Cache *cache=pc_kind35_slot(shot);
        if(cache && *pc_kind35_word(shot)==35)
            pc_kind35_context.cache=cache;
    }
    original();
    pc_kind35_context=saved;
}
uint32_t pc_native_kind35_gate(uint32_t value)
{ return pc_kind35_context.cache ? pc_kind35_delta()>0 : value; }
float pc_native_kind35_turn(float value)
{ return pc_kind35_context.cache ? value*pc_kind35_delta() : value; }

/* Cache each coordinate by its call site: missing emitters must not shift
 * the random values belonging to another emitter. Fractional callbacks
 * reuse that jitter while the original code follows the fresh position. */
static void pc_native_kind35_rng_guest(void (*original)(void),uint32_t caller,
                                       uint32_t *eax,uint32_t *esp)
{
    static const uint32_t sites[]={0xF1746,0xF1775,0xF17A5,0xF17F5,
        0xF1824,0xF1854,0xF18A4,0xF18D3,0xF1903};
    unsigned i=0;
    while(i<9 && sites[i]!=caller)i++;
    PcKind35Cache *cache=pc_kind35_context.cache;
    if(!cache || i==9) { original();return; }
    if(*pc_kind35_word(0x4E7944u)==0) {
        *eax=cache->random[i];*esp+=4;return;
    }
    original();cache->random[i]=*eax;
}
static void pc_native_kind35_target_guest(void (*original)(void),uint32_t shot)
{
    PcKind35Cache *cache=pc_kind35_context.cache;
    if(!cache || shot!=pc_kind35_context.shot) { original();return; }
    uint32_t whole=*pc_kind35_word(0x4E7944u);
    if(!cache->target_ready) {
        /* A newborn shot needs a valid target before its first fractional
         * steering step. Do not count that acquisition again at the first
         * whole boundary. Subsequent 11-tick checks retain their cadence. */
        original();cache->target_ready=1;cache->first_whole=whole==0;return;
    }
    if(whole==0 || cache->first_whole) {
        uint32_t *counter=pc_kind35_word(shot+0x1ACu),saved=*counter;
        *counter=1;original();*counter=saved;
        if(whole)cache->first_whole=0;
    } else original();
}
#endif
