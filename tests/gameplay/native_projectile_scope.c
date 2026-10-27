/* Sidecar isolation: interleaved/recycled projectiles must not borrow jitter
 * or leak timing policy into nested unrelated guest updates. */
#include <assert.h>
#include <stdlib.h>
#include "../src/gameplay/pc_native_projectile_guest.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
enum { FIRST=0x1000, SECOND=FIRST+0x22c, OTHER=SECOND+0x22c };
static uint32_t result,sp,draws,caller=0xF1746;
static void rng(void) { result=++draws;sp+=4; }
static void sample(void)
{
    sp=100;pc_native_kind35_rng_guest(rng,caller,&result,&sp);
    assert(sp==104);
}
static void noop(void) {}
static void original_policy(void)
{
    assert(pc_native_kind35_gate(0)==0 && pc_native_kind35_gate(6)==6);
    assert(pc_native_kind35_turn(8)==8);
    sample();assert(result==draws);
}
static void nested(void)
{
    assert(pc_native_kind35_gate(0)==1 && pc_native_kind35_turn(8)==2);
    pc_native_kind35_update_guest(original_policy,OTHER);
    assert(pc_native_kind35_gate(0)==1 && pc_native_kind35_turn(8)==2);
    sample();assert(result==1); /* first slot's cached value restored */
}
int main(void)
{
    void *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    *pc_kind35_word(0x737BD4)=FIRST;
    *pc_kind35_word(FIRST)=*pc_kind35_word(SECOND)=35;
    *pc_kind35_word(OTHER)=28;
    *(float *)pc_kind35_word(0x4E794C)=.25f;
    g_tsfp_native_timing_active=1;
    *pc_kind35_word(0x4E7944)=1;
    pc_native_kind35_update_guest(sample,FIRST);assert(result==1);
    pc_native_kind35_update_guest(sample,SECOND);assert(result==2);
    *pc_kind35_word(0x4E7944)=0;
    pc_native_kind35_update_guest(sample,FIRST);assert(result==1 && draws==2);
    pc_native_kind35_update_guest(sample,SECOND);assert(result==2 && draws==2);
    pc_native_kind35_update_guest(nested,FIRST);assert(draws==3);
    original_policy();assert(draws==4); /* outer scope also restored */
    caller=0xF0A29; /* impact RNG remains an ordinary one-shot draw */
    pc_native_kind35_update_guest(sample,FIRST);assert(result==5 && draws==5);
    caller=0xF1746;
    pc_native_kind35_init_guest(noop,FIRST);
    pc_native_kind35_update_guest(sample,FIRST);
    assert(result==0x80000000u && draws==5);
    pc_native_kind35_update_guest(sample,SECOND);assert(result==2 && draws==5);
    g_tsfp_native_timing_active=0;
    pc_native_kind35_update_guest(original_policy,FIRST);assert(draws==6);
    /* Reinitialization while disabled must still invalidate old sidecar data. */
    pc_native_kind35_init_guest(noop,SECOND);
    g_tsfp_native_timing_active=1;
    pc_native_kind35_update_guest(sample,SECOND);
    assert(result==0x80000000u && draws==6);
    *pc_kind35_word(0x737BD4)=0;
    pc_native_kind35_update_guest(original_policy,FIRST);assert(draws==7);
    *pc_kind35_word(0x737BD4)=FIRST;
    *pc_kind35_word(FIRST+85*0x22c)=35; /* outside complete allocated slots */
    pc_native_kind35_update_guest(original_policy,FIRST+85*0x22c);
    assert(draws==8);
    *pc_kind35_word(0x737BD4)=SECOND; /* new pool address clears old values */
    pc_native_kind35_update_guest(sample,SECOND);
    assert(result==0x80000000u && draws==8);
    free(memory);
    return 0;
}
