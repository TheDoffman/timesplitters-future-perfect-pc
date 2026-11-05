#ifndef TSFP_PC_NATIVE_PROJECTILE_GUEST_H
#define TSFP_PC_NATIVE_PROJECTILE_GUEST_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
#if defined(_MSC_VER)
#define PC_PROJECTILE_TLS __declspec(thread)
#else
#define PC_PROJECTILE_TLS _Thread_local
#endif

/* US 0xE8EB0 allocates 0xB8A0 bytes: 85 complete 0x22C projectile slots.
 * Keep random sidecars outside guest objects. Allocation and the wrapped
 * initializers reset slots even while native timing is disabled. */
typedef struct PcKind35Cache {
    uint32_t random[16], ballistic_previous[3];
    float ballistic_phase;
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
    for(unsigned i=0;i<16;i++)cache->random[i]=0x80000000u;
    for(unsigned i=0;i<3;i++)cache->ballistic_previous[i]=0x80000000u;
    cache->ballistic_phase=1.f;
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
        /* The historic kind35 ABI now also scopes ballistic, grenade and
         * wandering-shot randomness. Only audited updater wrappers enter it;
         * nested unrelated projectile kinds retain their original policy. */
        uint32_t kind=cache ? *pc_kind35_word(shot) : UINT32_MAX;
        if(cache && ((kind>=8 && kind<=19) || kind==35 || kind==38 || kind==39 || kind==41 || kind==42 || kind==43 || kind==50)) {
            pc_kind35_context.cache=cache;
            float delta=pc_kind35_delta();
            if((kind==38 || kind==41) && delta>0.f && isfinite(delta))
                cache->ballistic_phase=fminf(1.f,*pc_kind35_word(0x4E7944u) ? delta : cache->ballistic_phase+delta);
        }
    }
    original();
    pc_kind35_context=saved;
}
/* Flight smoke emits once per original game tick. Its random
 * calls are visual-only and must not advance the shared RNG at render rate. */
static int pc_native_projectile_skip_smoke(void)
{ return pc_kind35_context.cache && (!(pc_kind35_delta()>0.f) || *pc_kind35_word(0x4E7944u)==0); }
uint32_t pc_native_kind35_gate(uint32_t value)
{ return pc_kind35_context.cache ? pc_kind35_delta()>0 : value; }
float pc_native_kind35_turn(float value)
{ return pc_kind35_context.cache ? value*pc_kind35_delta() : value; }

/* F1A90 derives speed from one update's travelled distance. Its continuous
 * path now advances by delta ticks, so restore units/second before the final
 * effect/transform callback. The original whole-step result stays exact. */
static void pc_native_ballistic_finish_guest(void (*original)(void),uint32_t shot)
{
    float delta=pc_kind35_delta();
    if(pc_kind35_context.cache && shot==pc_kind35_context.shot &&
       delta>0.f && isfinite(delta) && delta!=1.f) {
        float *speed=(float *)pc_kind35_word(shot+0x88u);
        *speed/=delta;
    }
    original();
}

/* The scripted spring steers at authored whole-tick boundaries, while its
 * projectile translates every fractional frame. Keep its rotation interval
 * at the same whole step without changing shared guest clocks. */
static int pc_native_scripted_projectile(void)
{ return pc_kind35_context.cache && *pc_kind35_word(pc_kind35_context.shot)==50; }
static void pc_native_scripted_steer_guest(void (*original)(void),uint32_t *esp)
{
    if(pc_native_scripted_projectile() && !*pc_kind35_word(0x4E7944u)) {*esp+=4;return;}
    original();
}
static void pc_native_scripted_rotation_guest(void (*original)(void),uint32_t esp)
{
    float *angle=(float *)pc_kind35_word(esp+12u),saved=*angle;
    float delta=pc_kind35_delta();
    if(pc_native_scripted_projectile() && delta>0.f && delta<1.f)*angle/=delta;
    original();*angle=saved;
}

/* Cache each coordinate by its call site: missing emitters must not shift
 * the random values belonging to another emitter. Fractional callbacks
 * reuse that jitter while the original code follows the fresh position. */
static void pc_native_kind35_rng_guest(void (*original)(void),uint32_t caller,
                                       uint32_t *eax,uint32_t *esp)
{
    static const uint32_t sites[]={0xF1746,0xF1775,0xF17A5,0xF17F5,
        0xF1824,0xF1854,0xF18A4,0xF18D3,0xF1903,
        0xF2677,0xF26A8,0xF26D9,0xF3404,0xF342D,0xF3456,0xECCDB};
    unsigned i=0;
    while(i<16 && sites[i]!=caller)i++;
    PcKind35Cache *cache=pc_kind35_context.cache;
    if(!cache || i==16) { original();return; }
    if(!(pc_kind35_delta()>0.f)) {*eax=cache->random[i];*esp+=4;return;}
    if(i>=9 && i<12) {
        if(*pc_kind35_word(0x4E7944u)) {
            original();cache->ballistic_previous[i-9]=cache->random[i];cache->random[i]=*eax;
        } else *esp+=4;
        if(cache->ballistic_phase==1.f)*eax=cache->random[i];
        else *eax=(uint32_t)((double)cache->ballistic_previous[i-9]+
             ((double)cache->random[i]-cache->ballistic_previous[i-9])*cache->ballistic_phase);
        return;
    }
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
