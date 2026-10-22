/* Complete original region-force selector, movement/state updates and falling
 * continuation. Controlled region records and a flat-floor collision service
 * replace level assets/geometry; no network or physical-input claim is made. */
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
enum { PLAYER=0x10000, SECOND=PLAYER+0x1584, ENTITY=0x20000, BODY=0x22000,
       ENTITY2=0x24000, BODY2=0x26000, POSITION=0x40000,
       FORWARD=0x40100, SIDE=0x40110, STACK=0x50000 };
static unsigned collisions,landings;
static int floor_enabled;
#define UNEXPECTED(name) void name(void) { assert(!#name); }
UNEXPECTED(sub_000644D0)
UNEXPECTED(sub_000A5A60)
UNEXPECTED(sub_000B91F0)
UNEXPECTED(sub_000BA7D0)
UNEXPECTED(sub_000BD090)
UNEXPECTED(sub_000D3F80)
UNEXPECTED(sub_0025C440)
void sub_00238510(void) { g_esp+=4; }
void sub_000C5B50(void) {
    assert(MEM32(g_esp+8)==PLAYER+0xa8 && MEM32(g_esp+12)==POSITION);
    collisions++;g_eax=0;
    if(floor_enabled && MEMF(POSITION+4)<=0) {
        MEMF(POSITION+4)=0;MEMF(BODY+0xbfc)=0;landings++;g_eax=1;
    }
    g_esp+=4;
}
/* GENERATED_ROUTINES */
static void setup(float height) {
    memset((void *)((uintptr_t)PLAYER+g_xbox_mem_offset),0,0x30200);
    MEM32(0x7b0c7c)=MEM32(0x7b0c48)=PLAYER;MEM32(0x790950)=2;
    MEM32(PLAYER+0x14)=ENTITY;MEM32(ENTITY+0x7c)=BODY;
    MEM32(SECOND+0x14)=ENTITY2;MEM32(ENTITY2+0x7c)=BODY2;
    MEM32(PLAYER+0x114)=MEM32(SECOND+0x114)=2;
    MEMF(PLAYER+0xac)=height;MEMF(PLAYER+0x98)=.25f;MEMF(PLAYER+0x90)=1.5f;
    MEMF(SECOND+0xa8)=100;MEMF(SECOND+0xac)=2;MEMF(BODY2+0xb20)=.7f;
    MEMF(ENTITY+0x1cc)=100;MEMF(BODY+0xb70)=height;
    MEM32(0x4e8cec)=60;MEM32(0x79094c)=101;MEM32(0x7de458)=0x200;
    MEM32(0x6d612c)=0;MEMF(FORWARD)=1;MEMF(SIDE+4)=1;
    for(unsigned i=0;i<4;i++) {
        unsigned region=0x50d408+i*20;
        MEMF(region)=i?1000:0;MEMF(region+4)=0;MEMF(region+8)=0;
        MEMF(region+12)=16;MEMF(region+16)=20;
    }
    collisions=landings=0;floor_enabled=0;
}
static void prepare(float raw,float game) {
    MEMF(0x4e7948)=raw;MEMF(0x4e794c)=game;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_ebx=0x123;g_ebp=0x456;g_esi=0x789;g_edi=0xabc;
}
static void check_abi(void) {
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(g_ebx==0x123 && g_ebp==0x456 && g_esi==0x789 && g_edi==0xabc);
}
static void region_step(float raw,float game) {
    prepare(raw,game);sub_00225C70();check_abi();
}
static void movement(float raw,float game,int falling,float horizontal) {
    prepare(raw,game);MEM32(STACK+4)=POSITION;MEMF(STACK+8)=horizontal*game;
    MEM32(STACK+12)=FORWARD;MEM32(STACK+16)=SIDE;MEMF(STACK+20)=0;
    if(falling)sub_000D4100();else sub_000D4820();check_abi();
    assert(MEMF(0x4e7948)==raw && MEMF(0x4e794c)==game);
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;setvbuf(stdout,NULL,_IONBF,0);
    /* ORIGINAL_CONSTANTS */
    const unsigned rates[]={60,60,120,144};
    /* Constant-force comparisons expose drag independently of the moving
     * region boundary. Then the real selector supplies the force below. */
    for(unsigned slow=1;slow<=4;slow*=4)for(unsigned force=0;force<3;force++) {
        float reference_y=0,reference_v=0;
        for(unsigned r=0;r<4;r++) {
            unsigned fps=rates[r];g_tsfp_native_timing_active=r!=0;setup(100);
            MEMF(BODY+0xb20)=force*.5f;
            for(unsigned f=0;f<fps*slow*2;f++) {
                movement(60.f/fps,60.f/(fps*slow),0,0);
                double offset=.25*pow((double)MEMF(0x475cd0),(f+1)*60./fps);
                if(fabs(MEMF(PLAYER+0x98)-offset)>.000002) {
                    printf("force offset mismatch native=%d fps=%u frame=%u actual=%.9f expected=%.9f\n",
                           g_tsfp_native_timing_active,fps,f,MEMF(PLAYER+0x98),offset);
                    assert(!"force-mode view settles at frame rate");
                }
            }
            float y=MEMF(PLAYER+0xac),v=MEMF(BODY+0xb1c);
            if(!r){reference_y=y;reference_v=v;}
            printf("force native=%d fps=%u slow=%u force=%.1f y=%.9f vy=%.9f dy=%.9f dv=%.9f\n",
                   g_tsfp_native_timing_active,fps,slow,force*.5f,y,v,y-reference_y,v-reference_v);
            assert(fabsf(y-reference_y)<.6f/slow+.001f);
            assert(fabsf(v-reference_v)<.00001f);
        }
    }
    /* Accelerated game speed still follows the original 60-Hz drag count,
     * including rates where one fractional update contains multiple loops. */
    const float speeds[]={1.5f,2.f,3.f};
    for(unsigned speed=0;speed<3;speed++)for(unsigned force=0;force<2;force++) {
        float reference_y=0,reference_v=0;
        for(unsigned r=0;r<4;r++) {
            unsigned fps=rates[r];float raw=60.f/fps;
            g_tsfp_native_timing_active=r!=0;setup(100);MEMF(BODY+0xb20)=(float)force;
            for(unsigned f=0;f<fps*2;f++)movement(raw,raw*speeds[speed],0,0);
            float y=MEMF(PLAYER+0xac),v=MEMF(BODY+0xb1c);
            if(!r){reference_y=y;reference_v=v;}
            printf("fast force fps=%u native=%d speed=%.1f force=%u dy=%.9f dv=%.9f\n",
                   fps,g_tsfp_native_timing_active,speeds[speed],force,y-reference_y,v-reference_v);
            assert(fabsf(v-reference_v)<.00001f);
            assert(fabsf(y-reference_y)<.6f*speeds[speed]+.001f);
        }
    }
    for(unsigned slow=1;slow<=4;slow*=4) {
        float reference_y[361],reference_v[361];
        for(unsigned r=0;r<4;r++) {
            unsigned fps=rates[r];g_tsfp_native_timing_active=r!=0;setup(2);
            float max_y=0,max_v=0;
            for(unsigned f=1;f<=fps*slow*6;f++) {
                region_step(60.f/fps,60.f/(fps*slow));
                assert(MEM32(PLAYER+0x114)==0x800);
                assert(MEM32(SECOND+0x114)==2 && MEMF(BODY2+0xb20)==0);
                movement(60.f/fps,60.f/(fps*slow),0,0);
                if(f*60%(fps*slow)==0) {
                    unsigned index=f*60/(fps*slow);
                    float y=MEMF(PLAYER+0xac),v=MEMF(BODY+0xb1c);
                    if(!r){reference_y[index]=y;reference_v[index]=v;}
                    float dy=fabsf(y-reference_y[index]),dv=fabsf(v-reference_v[index]);
                    if(dy>max_y)max_y=dy;if(dv>max_v)max_v=dv;
                }
            }
            printf("coupled force fps=%u native=%d slow=%u max_dy=%.9f max_dv=%.9f\n",
                   fps,g_tsfp_native_timing_active,slow,max_y,max_v);
            assert(max_y<.1f/slow+.0001f && max_v<.001f);
        }
    }
    /* Walk out of the controlled cylinder, execute the real transition back
     * to falling, then land using the original falling routine. */
    for(unsigned slow=1;slow<=4;slow*=4) {
        double reference_exit=0,reference_land=0;
        for(unsigned r=0;r<4;r++) {
            unsigned fps=rates[r],enters=0,exits=0,landed=0;
            double exit_time=0,land_time=0;
            g_tsfp_native_timing_active=r!=0;setup(2);floor_enabled=1;
            for(unsigned f=0;f<fps*slow*6;f++) {
                unsigned before=MEM32(PLAYER+0x114);
                region_step(60.f/fps,60.f/(fps*slow));
                unsigned state=MEM32(PLAYER+0x114);
                if(before!=0x800 && state==0x800)enters++;
                if(before==0x800 && state==4){exits++;exit_time=(double)f/fps;}
                assert(state==0x800 || state==4);
                movement(60.f/fps,60.f/(fps*slow),state==4,.02f);
                if(MEM32(PLAYER+0x114)==2){landed++;land_time=(double)(f+1)/fps;break;}
            }
            assert(enters==1 && exits==1 && landed==1 && landings==1);
            assert(MEMF(BODY+0xb20)==0 && MEMF(PLAYER+0xac)==0);
            assert(MEM32(SECOND+0x114)==2 && MEMF(ENTITY+0x1cc)==100);
            if(!r){reference_exit=exit_time;reference_land=land_time;}
            printf("force exit fps=%u native=%d slow=%u exit_seconds=%.6f land_seconds=%.6f\n",
                   fps,g_tsfp_native_timing_active,slow,exit_time,land_time);
            assert(fabs(exit_time-reference_exit)<1./60+1./fps+.00001);
            assert(fabs(land_time-reference_land)<2./60+1./fps+.00001);
        }
    }
    /* Fast motion with the original region-driven force feedback, sampled
     * at shared wall-clock instants rather than matching update counts. */
    for(unsigned speed=0;speed<3;speed++) {
        float reference_y[73]={0},reference_v[73]={0};
        for(unsigned r=0;r<4;r++) {
            unsigned fps=rates[r];float raw=60.f/fps,max_y=0,max_v=0;
            g_tsfp_native_timing_active=r!=0;setup(2);
            for(unsigned f=1;f<=fps*6;f++) {
                region_step(raw,raw*speeds[speed]);
                assert(MEM32(PLAYER+0x114)==0x800);
                movement(raw,raw*speeds[speed],0,0);
                if(f*12%fps==0) {
                    unsigned index=f*12/fps;
                    float y=MEMF(PLAYER+0xac),v=MEMF(BODY+0xb1c);
                    if(!r){reference_y[index]=y;reference_v[index]=v;}
                    max_y=fmaxf(max_y,fabsf(y-reference_y[index]));
                    max_v=fmaxf(max_v,fabsf(v-reference_v[index]));
                }
            }
            printf("fast coupled fps=%u native=%d speed=%.1f max_dy=%.9f max_dv=%.9f\n",
                   fps,g_tsfp_native_timing_active,speeds[speed],max_y,max_v);
            assert(max_y<.1f*speeds[speed] && max_v<.001f*speeds[speed]);
        }
    }
    /* Original region bounds/priority, two-player isolation and the death
     * state's transition guard. These run the real selector/state setter. */
    for(int native=0;native<2;native++) {
        g_tsfp_native_timing_active=native;setup(8);
        MEMF(BODY+0xb1c)=.2f;region_step(.5f,.5f);
        assert(MEM32(PLAYER+0x114)==0x800 && MEMF(BODY+0xb1c)==0);
        assert(fabsf(MEMF(BODY+0xb20)-.6f)<.000001f);
        MEMF(PLAYER+0xa8)=4;region_step(.5f,.5f);assert(MEM32(PLAYER+0x114)==0x800);
        MEMF(PLAYER+0xa8)=4.01f;region_step(.5f,.5f);
        assert(MEM32(PLAYER+0x114)==4 && MEMF(BODY+0xb20)==0);
        setup(8.01f);region_step(.5f,.5f);assert(MEM32(PLAYER+0x114)==2);
        MEM32(PLAYER+0x114)=4;MEMF(PLAYER+0xac)=20;region_step(.5f,.5f);
        assert(MEM32(PLAYER+0x114)==0x800 && MEMF(BODY+0xb20)==0);
        MEMF(PLAYER+0xac)=20.01f;region_step(.5f,.5f);assert(MEM32(PLAYER+0x114)==4);
        setup(-.01f);region_step(.5f,.5f);assert(MEM32(PLAYER+0x114)==2);
        setup(2);MEMF(SECOND+0xa8)=0;MEMF(SECOND+0xac)=4;
        MEMF(0x50d41c)=0;MEMF(0x50d42c)=40; /* Overlapping second region. */
        region_step(.5f,.5f);
        assert(fabsf(MEMF(BODY+0xb20)-.9f)<.000001f);
        assert(fabsf(MEMF(BODY2+0xb20)-.8f)<.000001f);
        assert(MEM32(SECOND+0x114)==0x800);
        setup(2);MEM32(PLAYER+0x114)=16;region_step(.5f,.5f);
        assert(MEM32(PLAYER+0x114)==16);
    }
    /* Pause and uneven sub-tick force updates. Stay above the drag threshold
     * and below clamps for an exact constant-force common-time comparison. */
    float reference[5];const unsigned fields[]={0x98,0xa8,0xac,0xb0,0xb8};
    const float steps[]={.27f,.83f,.4f,.5f};
    const float jitter[]={.18168f,.81832f,.27f,.73f};
    for(unsigned slow=1;slow<=4;slow*=4) {
        g_tsfp_native_timing_active=0;setup(100);MEMF(BODY+0xb20)=.5f;MEMF(BODY+0xb1c)=.3f;
        for(unsigned f=0;f<20;f++)movement(1,1.f/slow,0,0);
        float reference_v=MEMF(BODY+0xb1c),reference_offset=MEMF(PLAYER+0x98);
        g_tsfp_native_timing_active=1;setup(100);MEMF(BODY+0xb20)=.5f;MEMF(BODY+0xb1c)=.3f;
        for(unsigned f=0;f<40;f++) {
            movement(steps[f%4],steps[f%4]/slow,0,0);
            for(unsigned i=0;i<5;i++)reference[i]=MEMF(PLAYER+fields[i]);
            float v=MEMF(BODY+0xb1c);
            movement(0,0,0,0);movement(.5f,0,0,0);
            for(unsigned i=0;i<5;i++)assert(reference[i]==MEMF(PLAYER+fields[i]));
            assert(v==MEMF(BODY+0xb1c));
        }
        assert(fabsf(reference_v-MEMF(BODY+0xb1c))<.000002f);
        assert(fabsf(reference_offset-MEMF(PLAYER+0x98))<.000002f);
    }
    for(unsigned speed=0;speed<3;speed++) {
        g_tsfp_native_timing_active=0;setup(100);MEMF(BODY+0xb20)=.5f;MEMF(BODY+0xb1c)=.3f;
        for(unsigned f=0;f<20;f++)movement(1,speeds[speed],0,0);
        float reference_v=MEMF(BODY+0xb1c);
        g_tsfp_native_timing_active=1;setup(100);MEMF(BODY+0xb20)=.5f;MEMF(BODY+0xb1c)=.3f;
        for(unsigned f=0;f<40;f++)movement(jitter[f%4],jitter[f%4]*speeds[speed],0,0);
        printf("uneven fast force speed=%.1f dv=%.9f\n",speeds[speed],MEMF(BODY+0xb1c)-reference_v);
        assert(fabsf(reference_v-MEMF(BODY+0xb1c))<.000002f);
        for(unsigned ticks=1;ticks<=3;ticks++) {
            float state[2][6];
            for(unsigned native=0;native<2;native++) {
                g_tsfp_native_timing_active=native;setup(100);MEMF(BODY+0xb20)=1;MEMF(BODY+0xb1c)=.5f;
                movement((float)ticks,ticks*speeds[speed],0,.02f);
                for(unsigned i=0;i<5;i++)state[native][i]=MEMF(PLAYER+fields[i]);
                state[native][5]=MEMF(BODY+0xb1c);
            }
            assert(!memcmp(state[0],state[1],sizeof state[0]));
        }
    }
    for(unsigned ticks=1;ticks<=3;ticks++)for(unsigned slow=1;slow<=4;slow*=4) {
        g_tsfp_native_timing_active=0;setup(100);MEMF(BODY+0xb20)=1;MEMF(BODY+0xb1c)=.5f;
        movement((float)ticks,(float)ticks/slow,0,.02f);
        for(unsigned i=0;i<5;i++)reference[i]=MEMF(PLAYER+fields[i]);
        float reference_v=MEMF(BODY+0xb1c);
        g_tsfp_native_timing_active=1;setup(100);MEMF(BODY+0xb20)=1;MEMF(BODY+0xb1c)=.5f;
        movement((float)ticks,(float)ticks/slow,0,.02f);
        for(unsigned i=0;i<5;i++)assert(reference[i]==MEMF(PLAYER+fields[i]));
        assert(reference_v==MEMF(BODY+0xb1c));
    }
    MEMF(0x4e7948)=.5f;MEMF(0x4e794c)=.5f;
    assert(RECOMP_F32_ADD_AT(0xd4852,.005f,.2f)==.005f+.2f);
    assert(RECOMP_F32_MUL_AT(0xd48d9,.2f,.99f)==.2f*.99f);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_ADD_AT(0xd4853,.005f,.2f)==.005f+.2f);
    assert(RECOMP_F32_MUL_AT(0xd48d8,.2f,.99f)==.2f*.99f);

    free(memory);puts("PASS: force-mode original routines and region coupling");return 0;
}
