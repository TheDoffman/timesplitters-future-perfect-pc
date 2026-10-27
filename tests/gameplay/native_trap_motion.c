/* Original TRAP solver and original sine-table initialization. Sound and
 * callback removal are service spies; no rewritten motion model is used. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum { ENTITY=0x1000, STACK=0x10000, BYTES=0x1a8 };
static unsigned sounds,controls,removals;
static int retired;
float pc_native_visual_sample(uint32_t value) {
    (void)value;assert(!"unexpected visual-time read");return 0;
}
void sub_00046A00(void) { sounds++;g_eax=0x1234;g_esp+=4; }
void sub_000462B0(void) {
    assert(MEM32(g_esp+4)==4);controls++;g_esp+=4;
}
void sub_000A5CE0(void) {
    assert(MEM32(g_esp+4)==ENTITY && MEM32(g_esp+8)==3 && !MEM32(g_esp+12));
    retired=1;removals++;g_esp+=4;
}
/* GENERATED_ROUTINES */
typedef struct { unsigned char entity[BYTES];unsigned sounds,controls,removals; } State;
static void reset(void) {
    memset((void *)(g_xbox_mem_offset+ENTITY),0,BYTES);
    MEMF(ENTITY+0xc0)=MEMF(0x49dedc);sounds=controls=removals=0;retired=0;
}
static State step(unsigned whole,float delta) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=ENTITY;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    if(!retired)sub_001D8AB0();else g_esp+=4;
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    State result={0};memcpy(result.entity,(void *)(g_xbox_mem_offset+ENTITY),BYTES);
    result.sounds=sounds;result.controls=controls;result.removals=removals;return result;
}
static float field(State s,unsigned offset) {float v;memcpy(&v,s.entity+offset,4);return v;}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;g_esp=STACK;MEM32(STACK)=0;sub_00153C20();
    assert(g_esp==STACK+4 && !g_fp_top);setvbuf(stdout,NULL,_IONBF,0);
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned phase=0;phase<4;phase++) {
        unsigned den=rates[r]*slow,count=den*2,previous=0;
        State *reference=calloc(count+1,sizeof *reference);assert(reference);
        double original_sound=0,original_remove=0;
        reset();g_tsfp_native_timing_active=0;
        memcpy(reference[0].entity,(void *)(g_xbox_mem_offset+ENTITY),BYTES);
        for(unsigned f=1;f<=count;f++) {
            unsigned tick=(f*240+phase*den)/(den*4);
            reference[f]=step(tick-previous,(float)(tick-previous));previous=tick;
            if(sounds && !original_sound)original_sound=(double)f/den;
            if(removals && !original_remove)original_remove=(double)f/den;
        }
        assert(sounds==1 && controls==1 && removals==1);
        reset();g_tsfp_native_timing_active=1;previous=0;
        double sound_at=0,remove_at=0,envelope_error=0,aligned_error=0;
        unsigned fresh=0,before_sound=0;float old_angle=MEMF(ENTITY+0xc0);
        printf("trap fractional fps=%u slow=%u phase=%u\n",rates[r],slow,phase);
        for(unsigned f=1;f<=count;f++) {
            unsigned tick=(f*240+phase*den)/(den*4);
            State s=step(tick-previous,60.f/den);previous=tick;
            float angle=field(s,0xc0);assert(isfinite(angle) && isfinite(field(s,0xe0)));
            if(!sounds) {before_sound++;if(angle!=old_angle)fresh++;}
            old_angle=angle;
            if(sounds && !sound_at)sound_at=(double)f/den;
            if(removals && !remove_at)remove_at=(double)f/den;
            /* Compare against the original staircase within one authored tick,
             * including initial carried-phase uncertainty. Never compare a
             * fractional pose bit-for-bit with a held Original frame. */
            unsigned span=(den+59)/60,lo=f>span?f-span:0,hi=f+span<count?f+span:count;
            double low=field(reference[lo],0xc0),high=low;
            for(unsigned k=lo+1;k<=hi;k++) {
                double v=field(reference[k],0xc0);
                if(v<low)low=v;if(v>high)high=v;
            }
            double gap=angle<low?low-angle:angle>high?angle-high:0;
            if(gap>envelope_error)envelope_error=gap;
            if(!phase && f*60%den==0) {
                double error=fabs(angle-field(reference[f],0xc0));
                if(error>aligned_error)aligned_error=error;
            }
            if(f==den/4) {
                State paused=step(0,0),stale=step(1,0);
                assert(!memcmp(&s,&paused,sizeof s) && !memcmp(&s,&stale,sizeof s));
            }
        }
        printf("trap fractional result fresh=%u/%u envelope=%f aligned=%f sound_error=%f remove_error=%f\n",
               fresh,before_sound,envelope_error,aligned_error,sound_at-original_sound,remove_at-original_remove);
        assert(sounds==1 && controls==1 && removals==1 && fresh==before_sound);
        assert(fabs(sound_at-original_sound)<=1./60+1./den);
        assert(fabs(remove_at-original_remove)<=1./60+1./den);
        assert(envelope_error<.25 && aligned_error<.25 && MEMF(ENTITY+0xc0)==0 && MEM32(ENTITY+0x1a0)==12);
        assert(fabs(MEMF(ENTITY+0x1a4)-remove_at)<.00005);
        free(reference);
    }
    /* Live FPS changes/jitter need no retained state or velocity conversion. */
    State original[121];double original_sound=0,original_remove=0;
    reset();g_tsfp_native_timing_active=0;
    memcpy(original[0].entity,(void *)(g_xbox_mem_offset+ENTITY),BYTES);
    for(unsigned tick=1;tick<=120;tick++) {
        original[tick]=step(1,1);
        if(sounds && !original_sound)original_sound=tick/60.;
        if(removals && !original_remove)original_remove=tick/60.;
    }
    const unsigned spans12[]={3,4,6,12,5,2,7,1,11};
    for(unsigned phase12=0;phase12<12;phase12++) {
        reset();g_tsfp_native_timing_active=1;
        unsigned units=0,previous=0;double sound_at=0,remove_at=0;
        printf("trap fractional live-switch phase=%u\n",phase12);
        for(unsigned frame=0;units<1440;frame++) {
            unsigned span=spans12[frame%9];if(span>1440-units)span=1440-units;
            units+=span;unsigned tick=(units+phase12)/12;
            State s=step(tick-previous,span/12.f);previous=tick;
            if(sounds && !sound_at)sound_at=units/720.;
            if(removals && !remove_at)remove_at=units/720.;
            unsigned at=units/12,lo=at?at-1:0,hi=at+2<120?at+2:120;
            double low=field(original[lo],0xc0),high=low,angle=field(s,0xc0);
            for(unsigned k=lo+1;k<=hi;k++) {
                double v=field(original[k],0xc0);if(v<low)low=v;if(v>high)high=v;
            }
            assert(angle>=low-.25 && angle<=high+.25);
        }
        assert(sounds==1 && controls==1 && removals==1);
        assert(fabs(sound_at-original_sound)<2./60 && fabs(remove_at-original_remove)<2./60);
        assert(fabs(MEMF(ENTITY+0x1a4)-remove_at)<.00005);
    }
    reset();g_tsfp_native_timing_active=1;State paused=step(0,0);
    State stale=step(1,0);assert(!memcmp(&paused,&stale,sizeof paused));
    stale=step(0,.25f);assert(memcmp(&paused,&stale,sizeof paused));
    /* Authored catch-up step sizes retain complete original state/events. */
    State saved={0};const unsigned spans[]={1,3,2,7,30,90,180,300};
    for(unsigned native=0;native<2;native++) {
        reset();g_tsfp_native_timing_active=native;State current={0};
        for(unsigned i=0;i<sizeof spans/sizeof spans[0];i++)current=step(spans[i],(float)spans[i]);
        if(!native)saved=current;else assert(!memcmp(&saved,&current,sizeof saved));
    }
    puts("PASS: fresh TRAP motion, original event window/lifetime, pause and exact whole/catch-up fallback");
    free(memory);return 0;
}
