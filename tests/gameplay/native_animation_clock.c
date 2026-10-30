/* Execute the complete original animation timeline and event reader together.
 * Asset lookup/root sampling are controlled services, not rendered poses. */
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
enum { ENTITY=0x1000, MODEL=0x2000, TRACK=MODEL+0xd4, ASSET=0x3000,
       LOOKUP=0x4000, EVENTS=0x5000, STACK=0x10000 };
static unsigned roots, callbacks;
void sub_00056FC0(void)
{ assert(MEM32(g_esp+4)==17);g_eax=EVENTS;g_esp+=4; }
void sub_00059320(void)
{
    assert(MEM32(g_esp+4)==ASSET && MEM32(g_esp+8)==0);
    unsigned frame=MEM32(g_esp+12),out=MEM32(g_esp+16);
    assert(frame==0 || frame==60);
    /* Known displacement between endpoints, in all three axes. */
    MEMF(out)=frame*.05f;MEMF(out+4)=frame*.025f;MEMF(out+8)=-(float)frame*.1f;
    roots++;g_esp+=4;
}
void sub_000DB5E0(void)
{
    assert(MEM32(g_esp+4)==ENTITY && MEM32(g_esp+8)==17);
    callbacks++;g_esp+=4;
}
/* GENERATED_ROUTINES */
static unsigned step(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=TRACK;MEM32(STACK+8)=0;MEM32(STACK+12)=ENTITY;
    sub_0005ADC0();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    g_esp=STACK;MEM32(STACK+4)=ENTITY;MEM32(STACK+8)=0x2000;
    sub_00061570();assert(g_esp==STACK+4);
    return g_eax;
}
static void setup(int loop,float speed)
{
    memset((void *)(g_xbox_mem_offset+MODEL),0,0x200);
    MEM32(ENTITY+4)=MODEL;MEM32(ENTITY+0x20)=8;
    MEM32(TRACK)=17;MEM32(TRACK+4)=0;MEMF(TRACK+0x34)=speed;
    MEMF(TRACK+8)=.137f;
    MEM32(0x7e3954)=LOOKUP;MEM32(LOOKUP)=ASSET;
    MEM32(ASSET+0x10)=61;MEMF(ASSET+0x18)=1.f;
    MEM32(0x7de660)=loop;MEM32(0x7de664)=0x1000000;
    MEM32(0x4e8cec)=60;MEMF(0x4d1994)=60;
    MEM16(EVENTS+6)=1;MEM16(EVENTS+8)=23;MEM16(EVENTS+10)=0x2000;
    roots=callbacks=0;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    setvbuf(stdout,NULL,_IONBF,0);
    /* ORIGINAL_CONSTANTS */
    const unsigned rates[]={60,60,120,144,180,240};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++)for(unsigned slow=1;slow<=4;slow*=4)
    for(int loop=0;loop<=1;loop++)for(int reverse=0;reverse<=1;reverse++) {
        unsigned fps=rates[r],events=0,wraps=0,holds=0;
        g_tsfp_native_timing_active=r!=0;
        setup(loop,reverse?-1.f:1.f);
        unsigned frames=fps*slow*3;
        for(unsigned f=0;f<frames;f++) {
            float before=MEMF(TRACK+8);
            unsigned whole=(f+1)*60/(fps*slow)-f*60/(fps*slow);
            unsigned event=step(whole,60.f/(fps*slow));
            if(!reverse)events+=event;
            float after=MEMF(TRACK+8);
            assert(MEMF(TRACK+12)==before);
            if(after==before)holds++;
            if(before>0.f && before<1.f)assert(after!=before);
            if(!reverse && event) {
                double target=23./60.-.137+(loop?events-1:0);
                double observed=(double)(f+1)/(fps*slow);
                assert(observed>=target-.0001);
                assert(observed<=target+1./(fps*slow)+.0001);
            }
            if(loop && MEM32(TRACK+0x50))wraps++;
            double expected=.137+(reverse?-1.:1.)*(f+1)/(fps*slow);
            if(loop)expected-=floor(expected);
            else expected=fmax(0.,fmin(1.,expected));
            double difference=fabs(after-expected);
            if(loop)difference=fmin(difference,fabs(difference-1.));
            assert(difference<.00015);
            unsigned old_roots=roots,old_callbacks=callbacks;
            /* The actual updater replaces event bounds during pause, too. */
            assert(step(0,0)==0);
            assert(MEMF(TRACK+8)==after && roots==old_roots && callbacks==old_callbacks);
        }
        assert(!loop || (wraps==3 && roots==6 && holds==0));
        assert(loop || (roots==0 && callbacks==0));
        assert(!loop || callbacks==(reverse?0:3));
        if(!reverse)assert(events==(loop?3u:1u));
        if(loop) {
            float sign=reverse?1.f:-1.f;
            printf("root fps=%u reverse=%d xyz=%f,%f,%f\n",fps,reverse,MEMF(TRACK+0x1c),MEMF(TRACK+0x20),MEMF(TRACK+0x24));
            assert(fabsf(MEMF(TRACK+0x1c)-sign*9.f)<.0001f);
            assert(fabsf(MEMF(TRACK+0x20)-sign*4.5f)<.0001f);
            assert(fabsf(MEMF(TRACK+0x24)+sign*18.f)<.0001f);
        }
        printf("animation native=%d fps=%u slow=%u loop=%d reverse=%d time=%.9f events=%u wraps=%u holds=%u\n",
               g_tsfp_native_timing_active,fps,slow,loop,reverse,MEMF(TRACK+8),events,wraps,holds);
    }
    /* Catch-up crosses a loop boundary with its root adjustment and event. */
    setup(1,1);MEMF(TRACK+8)=.99f;
    step(3,3);assert(fabsf(MEMF(TRACK+8)-.04f)<.00001f);
    assert(roots==2 && callbacks==1);
    /* A zero-duration asset leaves its timeline untouched. */
    setup(1,1);MEMF(ASSET+0x18)=0;
    step(1,1);assert(MEMF(TRACK+8)==.137f && roots==0 && callbacks==0);
    free(memory);
    puts("PASS: original animation timeline, loop roots, forward events, reverse, pause and catch-up at 60/120/144");
    return 0;
}
