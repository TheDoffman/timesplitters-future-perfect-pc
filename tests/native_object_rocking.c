/* Original object rocking/bounce updater and matrix builders. Free-flight
 * characterization only: bounce, impact audio and collision response are
 * deliberately not approximated. Unexpected services fail the fixture. */
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
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected object callback")
#define UNUSED(name) void name(void) { assert(!"Unexpected " #name); }
UNUSED(sub_003C9DF8)
UNUSED(sub_00021910)
UNUSED(sub_00045390)
UNUSED(sub_00045820)
static unsigned random_calls;
void sub_00153B40(void) { random_calls++;g_eax=0x80000000u;g_esp+=4; }
/* GENERATED_ROUTINES */
static unsigned stack_offset,nest_once;
static float call_entity(unsigned entity,unsigned whole,float delta);
static void nested_original(void)
{
    unsigned parent_stack=g_esp;
    nest_once=0;stack_offset+=0x1000;
    call_entity(ENTITY+0x2e8,0,MEMF(0x4e794c));
    stack_offset-=0x1000;g_esp=parent_stack;
    sub_000A84F0_gen();
}
void sub_000A84F0(void)
{ pc_native_rocking_update_guest(nest_once?nested_original:sub_000A84F0_gen,MEM32(g_esp+4)); }
void sub_000A8AA0(void)
{ pc_native_rocking_init_guest(sub_000A8AA0_gen,MEM32(g_esp+4)); }
static void prepare_entity(unsigned entity)
{
    PcRockingState *slot=pc_rocking_slot(entity,1);
    if(slot)memset(slot,0,sizeof *slot);
    memset((void *)((uintptr_t)entity+g_xbox_mem_offset),0,0x2e8);
    MEM32(entity+4)=MODEL;MEM32(MODEL+0x10)=MATRICES;MEM32(MODEL+0x30)=2;
    MEM32(entity+0x16c)=0xa84f0;
    MEMF(entity+0x220)=10;MEMF(entity+0x224)=5;
}
static void prepare(void) { prepare_entity(ENTITY); }
static float call_entity(unsigned entity,unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    /* The caller supplies the current base transform each update. */
    unsigned matrix=MATRICES+64*MEM32(entity+0x12c);
    assert(MEM32(entity+0x12c)<4);
    for(unsigned i=0;i<16;i++) MEMF(matrix+i*4)=i%5==0?1.f:0.f;
    unsigned stack=STACK+stack_offset;
    g_esp=stack;MEM32(stack)=0x12345678;MEM32(stack+4)=entity;
    g_esi=0xdead;g_ebp=0xbeef;g_ebx=0xfeed;g_edi=0xabcd;
    sub_000A84F0();
    assert(g_esp==stack+4 && MEM32(stack)==0x12345678 && g_fp_top==0);
    /* Framed matrix helpers publish their frame in g_ebp; the translator
     * keeps the caller's actual EBP local rather than restoring this TLS. */
    assert(g_esi==0xdead && g_ebx==0xfeed && g_edi==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    assert(MEMF(entity+0x220)>.3f); /* Stay in the audited free-flight path. */
    for(unsigned i=0;i<16;i++) assert(isfinite(MEMF(matrix+i*4)));
    assert(fabsf(MEMF(matrix+0x34)-MEMF(entity+0x220))<.00001f);
    return MEMF(entity+0x220);
}
static float call(unsigned whole,float delta) { return call_entity(ENTITY,whole,delta); }
static void impulse(unsigned entity,float force)
{
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=entity;MEMF(STACK+8)=force;
    g_esi=0xdead;sub_000A8AA0();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_esi==0xdead && g_fp_top==0);
}
static unsigned reset_calls;
static void reset_original(void) { reset_calls++;g_esp+=4; }
static void lifecycle_checks(void)
{
    const unsigned other=ENTITY+0x2e8;
    const float spans[]={.125f,.875f,2,.25f,.75f,6};
    float reference[2][2];
    g_tsfp_native_timing_active=1;
    /* Separate runs and interleaved callbacks must produce the same state. */
    for(unsigned which=0;which<2;which++) {
        unsigned entity=which?other:ENTITY;
        prepare_entity(entity);MEMF(entity+0x220)+=which*3;
        for(unsigned frame=0;frame<36;frame++)call_entity(entity,0,spans[frame%6]);
        reference[which][0]=MEMF(entity+0x220);reference[which][1]=MEMF(entity+0x224);
    }
    prepare();g_tsfp_native_timing_active=0;
    for(unsigned frame=0;frame<60;frame++)call(1,1);
    assert(MEMF(ENTITY+0x220)==reference[0][0] && MEMF(ENTITY+0x224)==reference[0][1]);
    g_tsfp_native_timing_active=1;
    prepare();prepare_entity(other);MEMF(other+0x220)+=3;
    for(unsigned frame=0;frame<36;frame++) {
        call(0,spans[frame%6]);call_entity(other,0,spans[frame%6]);
    }
    for(unsigned which=0;which<2;which++) {
        unsigned entity=which?other:ENTITY;
        assert(MEMF(entity+0x220)==reference[which][0]);
        assert(MEMF(entity+0x224)==reference[which][1]);
    }
    prepare();float nested_height=call(0,.25f),nested_velocity=MEMF(ENTITY+0x224);
    prepare();prepare_entity(other);MEM32(other+0x12c)=1;
    nest_once=1;call(0,.25f);
    assert(!pc_rocking_context);
    assert(MEMF(ENTITY+0x220)==nested_height && MEMF(other+0x220)==nested_height);
    assert(MEMF(ENTITY+0x224)==nested_velocity && MEMF(other+0x224)==nested_velocity);
    /* An impulse at phase .25 contributes only .75 ticks of subsequent
     * damping/travel before the contact boundary; it cannot restart time. */
    prepare();MEM32(ENTITY+0x16c)=0;random_calls=0;
    impulse(ENTITY,2);call(1,1);
    float base_h=MEMF(ENTITY+0x220),base_v=MEMF(ENTITY+0x224);
    prepare();MEM32(ENTITY+0x16c)=0;
    impulse(ENTITY,2);call(0,.25f);
    PcRockingState no_impulse=*pc_rocking_slot(ENTITY,1);
    MEM32(MODEL+0x30)=0;impulse(ENTITY,4);MEM32(MODEL+0x30)=2;
    assert(!memcmp(&no_impulse,pc_rocking_slot(ENTITY,1),sizeof no_impulse));
    impulse(ENTITY,4);
    assert(random_calls==4 && pc_rocking_slot(ENTITY,1)->phase==.25);
    call(1,.75f);
    double r=1-MEMF(0x47d56c)/60.0,rd=pow(r,.75),s=(1-rd)/(1-r);
    assert(fabs(MEMF(ENTITY+0x224)-(base_v+2*rd))<.00001);
    assert(fabs(MEMF(ENTITY+0x220)-(base_h+2*MEMF(0x475cc4)/60.0*s))<.00001);
    /* The original initializer recursively forwards impulses to a child.
     * Fresh children draw their own two angular values; later impulses do not. */
    prepare();prepare_entity(other);
    MEM32(ENTITY+0x16c)=MEM32(other+0x16c)=0;
    MEM32(ENTITY+0x20c)=other;random_calls=0;
    impulse(ENTITY,2);assert(random_calls==4);
    assert(MEM32(ENTITY+0x16c)==0xa84f0 && MEM32(other+0x16c)==0xa84f0);
    call(0,.25f);call_entity(other,0,.5f);impulse(ENTITY,4);
    assert(random_calls==4);
    assert(pc_rocking_slot(ENTITY,1)->phase==.25 && pc_rocking_slot(other,1)->phase==.5);
    /* Reusing the same guest address must discard the old fractional state. */
    memset((void *)((uintptr_t)ENTITY+g_xbox_mem_offset),0,0x2e8);
    MEM32(ENTITY+4)=MODEL;MEMF(ENTITY+0x220)=10;
    impulse(ENTITY,2);assert(random_calls==6);
    assert(!pc_rocking_slot(ENTITY,1)->ready);
    call(1,1);assert(MEMF(ENTITY+0x220)==base_h && MEMF(ENTITY+0x224)==base_v);
    /* A global reset invalidates every slot, and still calls the original. */
    g_esp=STACK;pc_native_rocking_reset_guest(reset_original);
    assert(reset_calls==1 && g_esp==STACK+4);
    assert(!pc_rocking_slot(ENTITY,1)->ready && !pc_rocking_slot(other,1)->ready);
    /* A positive sub-microtick interval must not be rounded away. */
    call(0,1e-7f);assert(pc_rocking_slot(ENTITY,1)->phase>0);
    call(0,0);assert(pc_rocking_slot(ENTITY,1)->phase>0);
    /* Unaligned/out-of-pool entities retain the original whole-tick path. */
    prepare_entity(0x2000);g_tsfp_native_timing_active=0;
    float legacy=call_entity(0x2000,1,1);
    prepare_entity(0x2000);g_tsfp_native_timing_active=1;
    assert(call_entity(0x2000,0,.25f)==10);
    assert(call_entity(0x2000,1,.75f)==legacy);
    /* A capacity change denotes a new pool and cannot reuse old phases. */
    MEM32(0x72f5e4)=3;
    assert(!pc_rocking_slot(ENTITY,1)->ready && pc_rocking_count==3);
    MEM32(0x72f5e4)=2;
    assert(!pc_rocking_slot(other,1)->ready && pc_rocking_count==2);
    assert(!pc_rocking_slot(ENTITY-4,1) && !pc_rocking_slot(ENTITY+1,1));
    assert(!pc_rocking_slot(ENTITY+2*0x2e8,1));
    MEM32(0x72f5e8)=other;
    assert(!pc_rocking_slot(ENTITY,1));
    assert(!pc_rocking_slot(other,1)->ready && pc_rocking_base==other);
    MEM32(0x72f5e8)=ENTITY;
    assert(!pc_rocking_slot(ENTITY,1)->ready && pc_rocking_base==ENTITY);
    puts("PASS: rocking interleaving, uneven steps, original impulses/recursion, reuse, reset and pool scope");
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,60,120,144};
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;
    MEM32(0x72f5e8)=ENTITY;MEM32(0x72f5e4)=2;
    for(unsigned i=0;i<16;i++) MEMF(0x4e7900+i*4)=i%5==0?1.f:0.f;
    for(unsigned slow=1;slow<=4;slow*=4) {
        float reference_h=0,reference_v=0;
        for(unsigned rate=0;rate<4;rate++) {
            unsigned fps=rates[rate],changed=0;
            prepare();g_tsfp_native_timing_active=rate!=0;
            float previous=MEMF(ENTITY+0x220);
            for(unsigned frame=0;frame<fps;frame++) {
                unsigned whole=(frame+1)*60/(fps*slow)-frame*60/(fps*slow);
                float height=call(whole,rate?60.f/(fps*slow):(float)whole);
                changed+=height!=previous;previous=height;
            }
            if(!rate) { reference_h=previous;reference_v=MEMF(ENTITY+0x224); }
            assert(previous==reference_h && MEMF(ENTITY+0x224)==reference_v);
            printf("rocking_audit native=%u fps=%u slow=%u changed=%u frozen=%u height=%.9f velocity=%.9f\n",
                   rate!=0,fps,slow,changed,fps-changed,previous,MEMF(ENTITY+0x224));
            float paused=call(0,0);assert(paused==previous);
        }
    }
    /* Multi-tick catch-up equals the same sequence of original steps. */
    prepare();g_tsfp_native_timing_active=0;
    call(1,1);call(1,1);float reference=call(1,1);
    prepare();g_tsfp_native_timing_active=1;
    assert(call(3,3)==reference);
    lifecycle_checks();
    free(pc_rocking_slots);free(memory);
    puts("PASS: original object rocking endpoint/ABI checks; fractional progression evaluated separately");
    return 0;
}
