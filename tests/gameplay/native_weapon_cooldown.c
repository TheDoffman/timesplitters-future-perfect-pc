/* Execute the original player weapon's deferred-action cooldown branch.
 * The completion callback is mocked. This checks elapsed console ticks and
 * event count; it does not establish complete firing/reload/damage cadence. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp;
enum { WEAPON=0x1000, STATE=0x2000, TABLE=0x3000, STACK=0x7000,
       PRIMARY=TABLE+0x68, SECONDARY=PRIMARY+0x7c };
static unsigned callbacks,expected_mode;
void sub_001C0110(void)
{
    assert(MEM32(g_esp+4)==WEAPON && MEM32(g_esp+8)==STATE);
    assert(MEM32(g_esp+12)==TABLE && MEM32(g_esp+16)==expected_mode);
    assert((int32_t)MEM32(STATE+0x110)<=0);
    callbacks++;g_esp+=4;
}
/* GENERATED_ROUTINES */
static void call(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    /* The isolated frameless slice inherits EBP through the translator's
       caller-frame slot as well as the guest register. */
    g_ebp=WEAPON;g_seh_ebp=WEAPON;g_esi=STATE;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    sub_001C320C();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(g_ebp==WEAPON && g_esi==STATE && g_fp_top==0);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144,180,240},delays[]={1,7,30,60};
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEM32(0x4f9bac)=TABLE;expected_mode=PRIMARY;
    for(int native=0;native<=1;native++) for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++)
    for(unsigned slow=1;slow<=4;slow*=4) for(unsigned d=0;d<4;d++) {
        unsigned fps=rates[rate],delay=delays[d],before=callbacks;
        MEM32(STATE+0x110)=delay;
        for(unsigned frame=0;frame<fps*slow*2;frame++) {
            unsigned ticks=(frame+1)*60/(fps*slow),previous=frame*60/(fps*slow);
            g_tsfp_native_timing_active=native;
            call(ticks-previous,60.f/(fps*slow));
            assert(callbacks==before+(ticks>=delay));
            assert(MEM32(STATE+0x110)==(ticks<delay?delay-ticks:0));
            unsigned remaining=MEM32(STATE+0x110),count=callbacks;
            call(0,0); /* Pausing between updates cannot consume a tick/event. */
            assert(MEM32(STATE+0x110)==remaining && callbacks==count);
        }
    }
    /* Catch-up must subtract the full carried tick count, not a boolean. */
    for(int native=0;native<=1;native++) {
        g_tsfp_native_timing_active=native;
        unsigned before=callbacks;
        MEM32(STATE+0x110)=5;
        call(3,3);assert(MEM32(STATE+0x110)==2 && callbacks==before);
        call(0,.5f);assert(MEM32(STATE+0x110)==2 && callbacks==before);
        call(3,2.5f);assert(MEM32(STATE+0x110)==0 && callbacks==before+1);
        call(3,3);assert(callbacks==before+1);
    }
    /* Alternate mode selection/fallback and missing callback metadata. */
    MEM32(WEAPON+0x5c)=1;expected_mode=SECONDARY;
    MEM32(STATE+0x110)=1;call(1,1);
    MEM32(SECONDARY)=UINT32_MAX;expected_mode=PRIMARY;
    MEM32(STATE+0x110)=1;call(1,1);
    unsigned before=callbacks;
    MEM32(PRIMARY+0x54)=UINT32_MAX;
    MEM32(STATE+0x110)=1;call(3,3);
    assert((int32_t)MEM32(STATE+0x110)==-2 && callbacks==before);
    call(1,1);assert(callbacks==before);
    MEM32(WEAPON+0x30)=UINT32_MAX;MEM32(STATE+0x110)=7;call(3,3);
    assert(MEM32(STATE+0x110)==7 && callbacks==before);
    free(memory);
    puts("PASS: original deferred weapon cooldown, elapsed ticks, pause, catch-up and single completion at 60/120/144");
    return 0;
}
