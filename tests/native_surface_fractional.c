/* Original dispatcher and two-pass coefficient upload, with graphics spies.
 * These establish event cadence/ABI/parameter scope, not GPU image evolution. */
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
static const char *rate="144";
#define TSFP_PC_NATIVE_RATE_H
static int pc_native_rate(void) {return atoi(rate);}
/* SURFACE_POLICY */
enum { RESOURCE=0x1000,PARAMS=0x2000,IMAGES=0x3000,STACK=0x10000 };
static unsigned events,steps,filter_steps,uploads;
static unsigned generations[4],program,pass,target;
static float expected_delta;
static int requested;
static int precision_rate=-1;
void hle_d3d8_surface_precision_rate(int high_rate){precision_rate=high_rate;}
static float bias_step=-1;
static float coefficients[3];
void hle_d3d8_surface_coefficients(float a,float b,float d){coefficients[0]=a;coefficients[1]=b;coefficients[2]=d;}
void hle_d3d8_surface_bias_step(float delta){bias_step=delta;}
static void state(void) {g_eax=7;g_esp+=4;}
void sub_00019650(void){state();} void sub_0001ED60(void){state();}
void sub_00023270(void){state();} void sub_00021450(void){state();}
void sub_000214C0(void){state();}
void sub_0001EDE0(void){target=MEM32(g_esp+4);state();}
void sub_0002A2D0(void){program=MEM32(g_esp+4);g_esp+=4;}
void sub_0002A320(void){g_esp+=4;}
void sub_00194BB0(void) {
    unsigned source=MEM32(0x6652e0);
    assert(source<4 && target<4 && source!=target);
    if(program==0x12) {
        assert(MEMF(0x665478)==1 && generations[source]==steps);
        generations[target]=generations[source];filter_steps++;
    } else {
        assert(program==0x14);
        if(!pass)assert(generations[source]==steps && target==1);
        else {
            unsigned previous=MEM32(0x6652e4);
            assert(previous<4 && previous!=target && source!=previous);
            assert(source==1 && generations[source]==steps+1 && generations[previous]==steps);
        }
        generations[target]=steps+1;
    }
    g_esp+=4;
}
void sub_0002A310(void) {
    pass=MEM32(g_esp+4);assert(pass<2);
    if(program==0x12) { assert(!pass);g_esp+=4;return; }
    assert(program==0x14);
    if(pc_surface_bias_requested() && pc_surface_step.active) {
        assert(bias_step==expected_delta);
        assert(coefficients[0]==.105f && coefficients[1]==.65f && coefficients[2]==.98f);
    }
    else assert(bias_step==-1);
    if(!pass)assert(MEMF(0x665308)==.17f && MEMF(0x665314)==.17f);
    else {
        assert(fabsf(MEMF(0x66531c)-.105f*expected_delta)<1e-7f);
        assert(fabsf(MEMF(0x665330)-.65f*expected_delta)<1e-7f);
        assert(fabsf(MEMF(0x665340)-powf(.98f,expected_delta))<1e-7f);
    }
    uploads++;g_esp+=4;
}
static void disturbance(void){assert(generations[MEM32(RESOURCE+0x24)]==steps);events++;g_esp+=4;}
void sub_001953B0(void){pc_surface_disturbance_guest(disturbance);}
void sub_001952D0(void){pc_surface_disturbance_guest(disturbance);}
void sub_001950B0_gen(void);
void sub_001950B0(void){pc_surface_smoothing_guest(sub_001950B0_gen);}
void sub_00195280(void){assert(generations[MEM32(RESOURCE+0x28)]==steps+1);steps++;g_esp+=4;}
void sub_00195110_gen(void);
void sub_00195110(void){pc_surface_feedback_guest(sub_00195110_gen);}
/* GENERATED_ROUTINES */
static void setup(void) {
    MEM32(RESOURCE+0x1c)=PARAMS;MEM32(RESOURCE+0x20)=IMAGES;
    MEM32(RESOURCE+0x24)=0;MEM32(RESOURCE+0x28)=2;
    MEM32(RESOURCE+0x2c)=1;MEM32(RESOURCE+0x30)=3;
    MEMF(PARAMS+0x10)=.17f;MEMF(PARAMS+0x14)=.105f;
    MEMF(PARAMS+0x18)=.65f;MEMF(PARAMS+0x1c)=.98f;
    MEMF(0x475c78)=1;
    for(unsigned i=0;i<4;i++)MEM32(IMAGES+i*4)=i;
    generations[0]=generations[1]=generations[3]=0xdead;generations[2]=0;
    events=steps=filter_steps=uploads=0;
}
static void step(unsigned whole,float delta) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    expected_delta=requested && g_tsfp_native_timing_active && atoi(rate)>60 && delta>0 && delta<1?delta:1;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=RESOURCE;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    pc_surface_simulation_guest(sub_001956A0_gen);
    assert(g_esp==STACK+4 && g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(!g_fp_top && MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    assert(MEMF(PARAMS+0x14)==.105f && MEMF(PARAMS+0x18)==.65f && MEMF(PARAMS+0x1c)==.98f);
    assert(MEM32(RESOURCE+0x24)+MEM32(RESOURCE+0x28)==2);
    assert(MEM32(RESOURCE+0x24)!=MEM32(RESOURCE+0x28));
    assert(generations[MEM32(RESOURCE+0x28)]==steps);
    assert(bias_step==-1);
    assert(precision_rate==(pc_surface_live_requested()?(pc_native_rate()>60):-1));
    assert(!pc_surface_step.active && uploads==steps*2 && filter_steps==events/2);
}
int main(int argc,char **argv) {
    (void)argv;requested=argc==1;
#ifdef _WIN32
    _putenv_s("TSFP_SURFACE_FRACTIONAL_TEST",requested?"1":"0");
#else
    setenv("TSFP_SURFACE_FRACTIONAL_TEST",requested?"1":"0",1);
#endif
    unsigned char *memory=calloc(1,0x900000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    for(unsigned fps_index=0;fps_index<4;fps_index++) {
        unsigned fps=fps_index<2?60:fps_index==2?120:144;
        g_tsfp_native_timing_active=fps_index!=0;rate=fps==60?"60":fps==120?"120":"144";
        setup();unsigned previous=0;
        for(unsigned frame=1;frame<=fps*8;frame++) {
            unsigned tick=frame*60/fps;step(tick-previous,60.f/fps);previous=tick;
        }
        assert(steps==(requested?fps*8:480) && events==960);
        assert(filter_steps==480);
        printf("surface_fractional fps=%u native=%d steps=%u disturbances=%u smoothing_passes=%u\n",fps,g_tsfp_native_timing_active,steps,events,filter_steps);
        unsigned before=steps;step(0,0);assert(steps==before);
        if(g_tsfp_native_timing_active){step(1,0);assert(steps==before);}
        step(3,3);assert(steps==before+1); /* original one-dispatch catch-up */
        /* Changing rate keeps whichever of the two buffers owns the newest
         * state, including after an odd number of direct fractional reads. */
        rate="144";step(0,.4f);rate="60";step(1,1);
        rate="120";step(0,.5f);step(1,.5f);rate="144";step(1,.4f);
    }
    puts("PASS: original dispatcher/uploads with fractional scope, discrete events, latest-buffer reads and separate destinations");
    free(memory);return 0;
}
