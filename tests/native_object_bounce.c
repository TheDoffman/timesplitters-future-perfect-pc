/* Complete original object callback, transforms and sound parameter builders.
 * Only the CRT asin service and the audio-device boundary are adapted.
 * Native free flight advances fractionally; original contact, sound and
 * retirement logic is checked against the native-off 60-Hz reference. */
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
/* ROCKING_POLICY */
enum { ENTITY=0x1000, MODEL=0x3000, MATRICES=0x4000, STACK=0x10000 };
typedef struct { unsigned tick; unsigned char parameters[48]; } Sound;
static Sound events[128];
static unsigned sounds,math_calls,tick;
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected object callback")
void sub_003C9DF8(void)
{
    double x=g_fp_stack[g_fp_top];
    assert(isfinite(x) && fabs(x)<=1);
    g_fp_stack[g_fp_top]=asin(x);
    math_calls++;g_esp+=4;
}
void sub_00045820(void)
{
    uint32_t address=MEM32(g_esp+4);
    assert(sounds<128);
    events[sounds].tick=tick;
    memcpy(events[sounds++].parameters,(void *)((uintptr_t)address+g_xbox_mem_offset),48);
    assert(MEMF(address+0x18)==MEMF(ENTITY+0x224)*MEMF(0x475cd4));
    g_eax=17;g_esp+=4;
}
/* GENERATED_ROUTINES */
void sub_000A84F0(void)
{ pc_native_rocking_update_guest(sub_000A84F0_gen,MEM32(g_esp+4)); }
static void prepare(unsigned flags,unsigned tilt)
{
    memset(pc_rocking_slot(ENTITY,1),0,sizeof(PcRockingState));
    memset((void *)((uintptr_t)ENTITY+g_xbox_mem_offset),0,0x300);
    memset(events,0,sizeof(events));sounds=math_calls=tick=0;
    MEM32(ENTITY+4)=MODEL;MEM32(MODEL+0x10)=MATRICES;
    MEM32(ENTITY+0x16c)=0xa84f0;MEM32(ENTITY+0x28)=0x04000080;
    MEM32(ENTITY+0x9c)=flags;
    MEMF(ENTITY+0x220)=.2f;MEMF(ENTITY+0x224)=-3;
    MEMF(ENTITY+0x64)=11;MEMF(ENTITY+0x68)=22;MEMF(ENTITY+0x6c)=33;
    if(tilt) {
        MEMF(ENTITY+0xd4)=.32f;MEMF(ENTITY+0xdc)=-.25f;
        MEMF(ENTITY+0xe0)=.03f;MEMF(ENTITY+0xe8)=-.02f;
    }
}
static void call(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    MEM32(0x7de318)=1000+tick;
    for(unsigned i=0;i<16;i++) MEMF(MATRICES+i*4)=i%5==0?1.f:0.f;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=ENTITY;
    g_esi=0xdead;g_ebp=0xbeef;g_ebx=0xfeed;g_edi=0xabcd;
    sub_000A84F0();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xfeed && g_edi==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    assert(MEMF(ENTITY+0x220)>=0 && isfinite(MEMF(ENTITY+0x224)));
    for(unsigned i=0;i<16;i++) assert(isfinite(MEMF(MATRICES+i*4)));
    assert(fabsf(MEMF(ENTITY+0xd4))<=.450001f);
    assert(fabsf(MEMF(ENTITY+0xdc))<=.450001f);
}
static uint32_t event_word(const Sound *s,unsigned offset)
{
    uint32_t value;memcpy(&value,s->parameters+offset,4);return value;
}
static float event_float(const Sound *s,unsigned offset)
{
    float value;memcpy(&value,s->parameters+offset,4);return value;
}
static void check_sound(unsigned flags)
{
    if(!flags) { assert(!sounds);return; }
    assert(sounds>0);
    uint32_t id=(flags&0x40)?0xcda:(flags&0x20)?0xcdb:0xcdc;
    for(unsigned i=0;i<sounds;i++) {
        assert(event_word(&events[i],0)==id);
        assert(events[i].parameters[0x28]==1);
        assert(event_float(&events[i],0x1c)==11);
        assert(event_float(&events[i],0x20)==22);
        assert(event_float(&events[i],0x24)==33);
        assert(event_float(&events[i],0x18)>.6f*.5f);
        if(i) assert(events[i].tick>events[i-1].tick);
    }
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,60,120,144},flags[]={0,0x40,0x20,0x200,0x260,0x220};
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;
    MEM32(0x72f5e8)=ENTITY;MEM32(0x72f5e4)=2;
    for(unsigned i=0;i<16;i++) MEMF(0x4e7900+i*4)=i%5==0?1.f:0.f;
    for(unsigned tilt=0;tilt<2;tilt++) for(unsigned kind=0;kind<6;kind++) {
        Sound reference[128];unsigned reference_sounds=0,reference_tick=0;
        float reference_velocity=0;
        for(unsigned slow=1;slow<=4;slow*=4) for(unsigned rate=0;rate<4;rate++) {
            unsigned fps=rates[rate],frames=0;
            prepare(flags[kind],tilt);g_tsfp_native_timing_active=rate!=0;
            while(MEM32(ENTITY+0x16c) && frames<fps*slow*20) {
                unsigned whole=(frames+1)*60/(fps*slow)-frames*60/(fps*slow);
                tick+=whole;frames++;
                call(whole,rate?60.f/(fps*slow):(float)whole);
            }
            assert(!MEM32(ENTITY+0x16c));
            assert(MEM32(ENTITY+0x28)==0x80 && MEM32(ENTITY+0x90)==1000+tick);
            assert(math_calls && fabsf(MEMF(ENTITY+0x224))<=.3f);
            check_sound(flags[kind]);
            if(slow==1 && !rate) {
                reference_sounds=sounds;reference_tick=tick;
                reference_velocity=MEMF(ENTITY+0x224);
                memcpy(reference,events,sizeof(events));
            } else {
                assert(sounds==reference_sounds);
                assert(!memcmp(reference,events,sizeof(events)));
                assert(MEMF(ENTITY+0x224)==reference_velocity);
                /* Original settling runs before the tick loop; a zero-tick
                 * callback can clear it before the next carried tick. */
                assert(tick==reference_tick || tick+1==reference_tick);
            }
            printf("bounce_audit tilt=%u flags=%x native=%u fps=%u slow=%u sounds=%u settle_tick=%u frames=%u\n",
                   tilt,flags[kind],rate!=0,fps,slow,sounds,tick,frames);
        }
    }
    /* Paused rendering still rebuilds the object's transform from the base
     * matrix. Returning at callback entry would hide its height/tilt. */
    prepare(0x40,1);g_tsfp_native_timing_active=0;
    call(0,0);
    unsigned char paused_matrix[64];
    memcpy(paused_matrix,(void *)((uintptr_t)MATRICES+g_xbox_mem_offset),64);
    unsigned char before[0x300];
    memcpy(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before));
    g_tsfp_native_timing_active=1;
    for(unsigned whole=0;whole<=6;whole++) {
        call(whole,0);
        assert(!memcmp(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before)));
        assert(!memcmp(paused_matrix,(void *)((uintptr_t)MATRICES+g_xbox_mem_offset),64));
        assert(!sounds && !math_calls);
    }
    /* Settling is before the original tick loop, so testing only a moving
     * object misses zero-time callback retirement. A paused native callback
     * must preserve the entire entity, including its lifetime flags/stamp. */
    for(unsigned whole=0;whole<=1;whole++) {
        prepare(0x40,0);g_tsfp_native_timing_active=1;
        MEMF(ENTITY+0x220)=0;MEMF(ENTITY+0x224)=.2f;
        memcpy(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before));
        call(whole,0);
        assert(!memcmp(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before)));
        assert(!sounds && !math_calls);
        /* Positive fractional time must still permit original settling. */
        call(0,.25f);
        assert(!MEM32(ENTITY+0x16c) && MEM32(ENTITY+0x28)==0x80);
        assert(MEM32(ENTITY+0x90)==1000+tick);
    }
    /* Native-off compatibility: retain original zero-tick retirement. */
    prepare(0x40,0);g_tsfp_native_timing_active=0;
    MEMF(ENTITY+0x220)=0;MEMF(ENTITY+0x224)=.2f;
    call(0,0);
    assert(!MEM32(ENTITY+0x16c) && MEM32(ENTITY+0x28)==0x80);
    g_tsfp_native_timing_active=1;
    /* Catch-up traverses the same bounce/impact response as six single
     * steps. Audio timestamps here are the containing call's tick; compare
     * the emitted payload and final physics separately. */
    prepare(0x40,1);
    for(tick=1;tick<=6;tick++) call(1,1);
    Sound catchup[128];unsigned catchup_count=sounds;
    assert(catchup_count==1);
    memcpy(catchup,events,sizeof(events));
    float state[]={MEMF(ENTITY+0x220),MEMF(ENTITY+0x224),MEMF(ENTITY+0xd4),
                   MEMF(ENTITY+0xdc),MEMF(ENTITY+0xe0),MEMF(ENTITY+0xe8)};
    prepare(0x40,1);tick=3;call(3,3);tick=6;call(3,3);
    assert(sounds==catchup_count);
    for(unsigned i=0;i<sounds;i++)
        assert(!memcmp(catchup[i].parameters,events[i].parameters,48));
    const unsigned offsets[]={0x220,0x224,0xd4,0xdc,0xe0,0xe8};
    for(unsigned i=0;i<6;i++) assert(MEMF(ENTITY+offsets[i])==state[i]);
    /* Pause during fractional floor contact, before the original bounce
     * boundary. Visible height stays nonnegative; latent penetration must
     * survive pause without an early sound or lost impact velocity. */
    prepare(0x40,1);g_tsfp_native_timing_active=0;
    for(tick=1;tick<=5;tick++)call(1,1);
    Sound first_impact=events[0];assert(sounds==1);
    for(unsigned i=0;i<6;i++)state[i]=MEMF(ENTITY+offsets[i]);
    prepare(0x40,1);g_tsfp_native_timing_active=1;
    for(unsigned frame=0;frame<11;frame++) {
        unsigned whole=(frame+1)*60/144-frame*60/144;tick+=whole;
        call(whole,60.f/144);
    }
    assert(MEMF(ENTITY+0x220)==0 && !sounds);
    PcRockingState contact=*pc_rocking_slot(ENTITY,1);
    assert(contact.hidden_height<0 && contact.phase>0);
    unsigned prior_math=math_calls;
    memcpy(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before));
    memcpy(paused_matrix,(void *)((uintptr_t)MATRICES+g_xbox_mem_offset),64);
    for(unsigned whole=0;whole<=6;whole+=6) {
        call(whole,0);
        assert(!memcmp(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before)));
        assert(!memcmp(paused_matrix,(void *)((uintptr_t)MATRICES+g_xbox_mem_offset),64));
        assert(!memcmp(&contact,pc_rocking_slot(ENTITY,1),sizeof contact));
        assert(!sounds && math_calls==prior_math);
    }
    tick++;call(1,60.f/144);
    assert(sounds==1 && !memcmp(&first_impact,&events[0],sizeof first_impact));
    for(unsigned i=0;i<6;i++) assert(MEMF(ENTITY+offsets[i])==state[i]);
    free(pc_rocking_slots);free(memory);
    puts("PASS: original bounce, sound selection, parameters, settling and pause");
    return 0;
}
