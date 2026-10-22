/* Complete original airborne player routines. A controlled flat floor replaces
 * world collision; fall integration, terminal speed, camera offset, landing
 * state, feedback and damage remain original. This is not scene collision QA. */
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
enum { PLAYER=0x1000, ENTITY=0x3000, BODY=0x4000, POSITION=0x6000,
       FORWARD=0x6100, SIDE=0x6110, STACK=0x10000 };
static unsigned collisions,landings;
static int floor_enabled;
static double airborne_seconds;
#define UNEXPECTED(name) void name(void) { assert(!#name); }
UNEXPECTED(sub_000644D0)
UNEXPECTED(sub_000A5A60)
UNEXPECTED(sub_000B91F0)
UNEXPECTED(sub_000BA7D0)
UNEXPECTED(sub_000BD090)
UNEXPECTED(sub_000D2CE0)
UNEXPECTED(sub_000D3F80)
UNEXPECTED(sub_0025C440)
void sub_000C5B50(void) {
    assert(MEM32(g_esp+8)==PLAYER+0xa8);
    assert(MEM32(g_esp+12)==POSITION);
    collisions++;
    g_eax=0;
    if(floor_enabled && MEMF(POSITION+4)<=0.f) {
        MEMF(POSITION+4)=0;MEMF(BODY+0xbfc)=0;g_eax=1;landings++;
    }
    g_esp+=4;
}
void sub_00238510(void) {
    assert(MEM32(g_esp+4)==0x36);
    airborne_seconds+=MEMF(g_esp+12);g_esp+=4;
}
/* GENERATED_ROUTINES */
static void setup(int mode,float height) {
    memset((void *)((uintptr_t)PLAYER+g_xbox_mem_offset),0,0x5200);
    MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER+0x14)=ENTITY;MEM32(ENTITY+0x7c)=BODY;
    MEM32(PLAYER+0x114)=mode?8:4;MEMF(PLAYER+0x90)=1.5f;
    MEMF(PLAYER+0x98)=.25f;MEMF(PLAYER+0xac)=height;
    MEMF(PLAYER+0xfa0)=.02f;MEMF(PLAYER+0xfa8)=-.01f;
    MEMF(ENTITY+0x1cc)=100;MEMF(BODY+0xb70)=height;
    MEM32(0x4e8cec)=60;MEM32(0x79094c)=101;
    /* Disable the separate four-unit fall-death rule, so the test reaches
     * the real landing branch and its independent height-based damage. */
    MEM32(0x7de458)=0x200;MEM32(0x6d612c)=0;
    MEMF(FORWARD)=1;MEMF(SIDE+4)=1;
    collisions=landings=0;airborne_seconds=0;floor_enabled=1;
}
static void step(int mode,float raw,float game) {
    MEMF(0x4e7948)=raw;MEMF(0x4e794c)=game;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=POSITION;
    MEMF(STACK+8)=.02f*game;MEM32(STACK+12)=FORWARD;
    MEM32(STACK+16)=SIDE;MEMF(STACK+20)=-.01f*game;
    g_ebx=0x123;g_ebp=0x456;g_esi=0x789;g_edi=0xabc;
    if(mode)sub_000D4610();else sub_000D4100();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(g_ebx==0x123 && g_ebp==0x456 && g_esi==0x789 && g_edi==0xabc);
    assert(MEMF(0x4e7948)==raw && MEMF(0x4e794c)==game);
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    setvbuf(stdout,NULL,_IONBF,0);
    /* ORIGINAL_CONSTANTS */
    const unsigned rates[]={60,60,120,144};
    for(int mode=0;mode<2;mode++)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned h=0;h<2;h++) {
        float height=h?18.f:3.f;
        double reference_time=0;
        for(unsigned r=0;r<4;r++) {
            unsigned fps=rates[r],frame=0;
            g_tsfp_native_timing_active=r!=0;setup(mode,height);
            while(MEM32(PLAYER+0x114)!=2 && frame<fps*slow*10) {
                step(mode,60.f/fps,60.f/(fps*slow));frame++;
                double expected=.25*pow((double)MEMF(0x475cd0),frame*60./fps);
                float actual=MEMF(PLAYER+0x98);
                if(fabs(actual-expected)>.000002) {
                    printf("offset mismatch mode=%d native=%d fps=%u slow=%u frame=%u actual=%.9f expected=%.9f\n",
                           mode,g_tsfp_native_timing_active,fps,slow,frame,actual,expected);
                    assert(!"airborne view offset settles faster at high refresh");
                }
                assert(isfinite(MEMF(PLAYER+0xb8)));
            }
            double elapsed=(double)frame/fps;
            assert(MEM32(PLAYER+0x114)==2 && landings==1 && collisions==frame);
            assert(MEMF(PLAYER+0xac)==0);
            assert(fabsf(MEMF(ENTITY+0x1cc)-(mode||!h?100.f:70.f))<.0001f);
            if(!r)reference_time=elapsed;
            assert(fabs(elapsed-reference_time)<=1./60+1./fps+.000001);
            if(!mode)assert(fabs(airborne_seconds-elapsed/slow)<.00001);
            printf("fall mode=%d native=%d fps=%u slow=%u height=%.0f frames=%u seconds=%.6f x=%.6f health=%.1f\n",
                   mode,g_tsfp_native_timing_active,fps,slow,height,frame,elapsed,
                   MEMF(PLAYER+0xa8),MEMF(ENTITY+0x1cc));
        }
    }
    /* Compare actual original-60 positions at shared sample times, including
     * terminal velocity. Fractional Euler integration has a bounded position
     * difference; this patch changes view-offset damping, not gravity. */
    for(unsigned slow=1;slow<=4;slow*=4) {
        float original_y[361],original_v[361];
        g_tsfp_native_timing_active=0;setup(0,100);floor_enabled=0;
        for(unsigned f=1;f<=90*slow;f++) {
            step(0,1,1.f/slow);
            original_y[f]=MEMF(PLAYER+0xac);original_v[f]=MEMF(BODY+0xb1c);
        }
        for(unsigned r=1;r<4;r++) {
            unsigned fps=rates[r];float max_error=0;
            g_tsfp_native_timing_active=1;setup(0,100);floor_enabled=0;
            for(unsigned f=1;f<=fps*3*slow/2;f++) {
                float previous=MEMF(PLAYER+0xac);
                step(0,60.f/fps,60.f/(fps*slow));
                assert(MEMF(PLAYER+0xac)<previous);
                if((f*60)%fps==0) {
                    unsigned index=f*60/fps;
                    float error=fabsf(MEMF(PLAYER+0xac)-original_y[index]);
                    if(error>max_error)max_error=error;
                    assert(error<.5f*fabsf(MEMF(0x47d6a4))/slow+.0002f);
                    assert(fabsf(MEMF(BODY+0xb1c)-original_v[index])<.00001f);
                }
            }
            assert(MEMF(BODY+0xb1c)==MEMF(0x47d6a4));
            printf("fall trajectory fps=%u slow=%u max_position_difference=%.9f terminal=%.6f\n",
                   fps,slow,max_error,MEMF(BODY+0xb1c));
        }
    }
    for(int mode=0;mode<2;mode++) {
        g_tsfp_native_timing_active=1;setup(mode,100);floor_enabled=0;
        step(mode,.5f,.5f);
        const unsigned fields[]={0x98,0xa8,0xac,0xb0,0xb4,0xb8,0xbc};
        float saved[7];for(unsigned i=0;i<7;i++)saved[i]=MEMF(PLAYER+fields[i]);
        float speed=MEMF(BODY+0xb1c);
        step(mode,0,0);step(mode,.5f,0);
        for(unsigned i=0;i<7;i++)assert(saved[i]==MEMF(PLAYER+fields[i]));
        assert(speed==MEMF(BODY+0xb1c));
        /* Uneven intervals preserve the original one-factor-per-raw-tick
         * response at normal and quarter speed. */
        const float deltas[]={.27f,.83f,.4f,.5f};
        for(unsigned slow=1;slow<=4;slow*=4) {
            setup(mode,100);floor_enabled=0;
            for(unsigned f=0;f<40;f++)step(mode,deltas[f%4],deltas[f%4]/slow);
            assert(fabsf(MEMF(PLAYER+0x98)-.25f*powf(MEMF(0x475cd0),20))<.000001f);
        }
        /* Original catch-up applies one damping factor even for multiple
         * ticks. Compare the entire routine output, not only the helper. */
        for(unsigned ticks=1;ticks<=3;ticks++) {
            g_tsfp_native_timing_active=0;setup(mode,100);floor_enabled=0;
            step(mode,(float)ticks,(float)ticks);
            for(unsigned i=0;i<7;i++)saved[i]=MEMF(PLAYER+fields[i]);
            speed=MEMF(BODY+0xb1c);
            g_tsfp_native_timing_active=1;setup(mode,100);floor_enabled=0;
            step(mode,(float)ticks,(float)ticks);
            for(unsigned i=0;i<7;i++)assert(saved[i]==MEMF(PLAYER+fields[i]));
            assert(speed==MEMF(BODY+0xb1c));
        }
    }
    MEMF(0x4e7948)=.5f;MEMF(0x4e794c)=.125f;
    assert(RECOMP_F32_MUL_AT(0xd41cf,.25f,.8f)==.25f*.8f);
    assert(RECOMP_F32_MUL_AT(0xd46b5,.25f,.8f)==.25f*.8f);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_MUL_AT(0xd41d0,.25f,.8f)==.25f*.8f);
    assert(RECOMP_F32_MUL_AT(0xd46b4,.25f,.8f)==.25f*.8f);
    free(memory);puts("PASS: original airborne updates, controlled landing, terminal speed, pause and timing scope");
    return 0;
}
