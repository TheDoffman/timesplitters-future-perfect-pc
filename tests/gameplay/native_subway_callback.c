/* Original dispatcher/reset/callback and original integer/float RNG helpers.
 * Descriptor lookup and sound lookup/playback/completion are controlled. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
static unsigned events,callbacks,polls;
static int legacy_gate;
static double elapsed,playing_until;
static uint32_t variants[3],event_seeds[3];
static double event_times[3];
static uint32_t callback_gate(uint32_t site,uint32_t address,uint32_t value)
{ return legacy_gate && site==0x21af90u?value:pc_native_global_load(site,address,value); }
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) callback_gate(site,address,value)
void sub_0021AF90(void);
static void level_callback(uint32_t target)
{ assert(target==0x21af90);++callbacks;sub_0021AF90(); }
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) level_callback(target)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) level_callback(target)
void sub_0006A850(void) {g_eax=0x5000;g_esp+=4;}
void sub_00045390(void) {g_esp+=4;}
void sub_00045540(void)
{ ++polls;assert(MEM32(g_esp+4)==100+events);g_eax=elapsed<playing_until;g_esp+=4; }
void sub_000461C0(void) {g_eax=MEM32(g_esp+4);g_esp+=4;}
void sub_00045820(void)
{
    assert(events<3);uint32_t id=MEM32(MEM32(g_esp+4));
    assert(id>=0x1b11 && id<=0x1b1c);
    if(events)assert(id!=variants[events-1]);
    variants[events]=id;event_times[events]=elapsed;
    event_seeds[events]=MEM32(0x7497cc);
    ++events;playing_until=elapsed+.3;g_eax=100+events;g_esp+=4;
}
#define UNUSED(name) void name(void) {assert(!"unexpected dispatcher path");g_esp+=4;}
UNUSED(sub_0003D7B0) UNUSED(sub_00067450) UNUSED(sub_0012D350)
UNUSED(sub_001CDAE0) UNUSED(sub_00227E40) UNUSED(sub_002282B0)
UNUSED(sub_002284D0) UNUSED(sub_00230510) UNUSED(sub_00232270)
UNUSED(sub_0033ED70)
/* ROUTINES */
static void dispatch(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=0x7000;MEM32(g_esp)=0x12345678;
    g_ebx=0x1111;g_esi=0x2222;g_edi=0x3333;
    sub_00228870();
    assert(g_esp==0x7004 && MEM32(0x7000)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0x1111 && g_esi==0x2222 && g_edi==0x3333);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void reset(void)
{
    MEM32(0x7497cc)=0x12345678;g_fp_top=0;events=callbacks=polls=0;
    elapsed=playing_until=0;g_esp=0x7000;sub_0021AF50();
    assert(g_esp==0x7004 && g_fp_top==0 && MEM32(0x6000)==0xffffffff);
    assert(MEMF(0x6008)>=10 && MEMF(0x6008)<=60);
}
int main(int argc,char **argv)
{
    (void)argv;legacy_gate=argc>1;setbuf(stdout,NULL);
    void *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    MEM32(0x5110)=0x21af90;MEM32(0x75c0f0)=0x6000;
    MEM32(0x4e8cec)=60;MEM32(0x7de45c)=0x8000;
    const unsigned rates[]={60,60,120,144,180,240};
    double reference_times[3]={0};uint32_t reference_variants[3]={0},reference_seeds[3]={0};
    for(unsigned r=0;r<6;r++)for(unsigned slow=1;slow<=4;slow*=4) {
        unsigned fps=rates[r],denominator=fps*slow;
        g_tsfp_native_timing_active=r!=0;reset();float initial=MEMF(0x6008);
        uint32_t reset_seed=MEM32(0x7497cc);float delta=60.f/denominator;
        for(unsigned frame=0;frame<denominator;frame++) {
            unsigned whole=(frame+1)*60/denominator-frame*60/denominator;
            dispatch(whole,g_tsfp_native_timing_active?delta:(whole?1.f:0.f));
        }
        printf("subway_callback fps=%u native=%u slow=%u consumed=%.8f\n",fps,r!=0,slow,initial-MEMF(0x6008));
        assert(fabsf(initial-MEMF(0x6008)-1.f)<.003f && !events && !polls);
        assert(MEM32(0x7497cc)==reset_seed);
        reset();
        for(unsigned frame=0;events<3 && frame<denominator*200;frame++) {
            unsigned whole=(frame+1)*60/denominator-frame*60/denominator;
            float game=g_tsfp_native_timing_active?delta:(whole?1.f:0.f);
            elapsed+=game/60.;dispatch(whole,game);
            if(g_tsfp_native_timing_active && frame%71==0) {
                uint32_t seed=MEM32(0x7497cc);unsigned e=events,p=polls,c=callbacks;
                float timer=MEMF(0x6008);dispatch(1,0);
                assert(MEM32(0x7497cc)==seed && events==e && polls==p && callbacks==c && MEMF(0x6008)==timer);
            }
        }
        assert(events==3 && polls>0);
        if(r==0 && slow==1) {
            memcpy(reference_times,event_times,sizeof event_times);
            memcpy(reference_variants,variants,sizeof variants);memcpy(reference_seeds,event_seeds,sizeof event_seeds);
        } else {
            assert(!memcmp(reference_variants,variants,sizeof variants));
            assert(!memcmp(reference_seeds,event_seeds,sizeof event_seeds));
            for(unsigned i=0;i<3;i++)assert(fabs(event_times[i]-reference_times[i])<.15);
        }
        uint32_t expected=0x12345678;
        for(unsigned i=0;i<7;i++)expected=expected*0x343fdu+0x269ec3u;
        assert(MEM32(0x7497cc)==expected); /* reset + variant/repeat draw per event */
    }
    free(memory);puts("PASS: Subway callback elapsed time, real RNG sequence, nonrepeat variants, completion, pause and ABI");return 0;
}
