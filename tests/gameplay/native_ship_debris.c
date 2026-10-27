/* Run the original ship-debris initializer. Allocation and the final effect
 * service are captured; a repeatable RNG exercises all original arithmetic.
 * This tests inherited motion, not assets, rendering or audio output. */
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
enum { ENTITY=0x1000, DEBRIS=0x10000, STACK=0x30000, COUNT=30, STRIDE=0x300 };
static unsigned allocations,effects,random_calls,fail_every;
static uint32_t random_state;
typedef struct {
    uint32_t model,live;
    float position[3],direction[3];
    unsigned char entity[STRIDE];
} Fragment;
typedef struct {
    Fragment fragments[COUNT];
    unsigned random_calls;
    uint32_t effect_words[8];
    float effect_vectors[6];
} Result;
static Result result;
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected debris callback")
void sub_00153B40(void)
{
    random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;
    g_eax=random_state;random_calls++;g_esp+=4;
}
void sub_000A6EC0(void)
{
    assert(allocations<COUNT && MEM32(g_esp+4)==1);
    assert(MEM32(g_esp+20)==0 && MEM32(g_esp+24)==0);
    unsigned index=allocations++;
    Fragment *f=&result.fragments[index];
    f->model=MEM32(g_esp+8);
    uint32_t direction=MEM32(g_esp+12),position=MEM32(g_esp+16);
    for(unsigned i=0;i<3;i++) {
        f->position[i]=MEMF(position+i*4);
        f->direction[i]=MEMF(direction+i*4);
    }
    f->live=!(fail_every && allocations%fail_every==0);
    g_eax=f->live?DEBRIS+index*STRIDE:0;g_esp+=4;
}
void sub_000EFBD0(void)
{
    effects++;
    for(unsigned i=0;i<8;i++)result.effect_words[i]=MEM32(g_esp+4+i*4);
    for(unsigned vector=0;vector<2;vector++) {
        uint32_t address=result.effect_words[2+vector];
        for(unsigned i=0;i<3;i++)result.effect_vectors[vector*3+i]=MEMF(address+i*4);
        result.effect_words[2+vector]=0; /* stack addresses are not effect content */
    }
    g_eax=1;g_esp+=4;
}
/* GENERATED_ROUTINES */
static Result run(unsigned whole,float delta,int native,unsigned model,unsigned inherit,unsigned failure)
{
    memset(&result,0,sizeof(result));allocations=effects=random_calls=0;
    random_state=0x12345678;fail_every=failure;g_tsfp_native_timing_active=native;
    memset((void *)((uintptr_t)ENTITY+g_xbox_mem_offset),0,0x300);
    memset((void *)((uintptr_t)DEBRIS+g_xbox_mem_offset),0,COUNT*STRIDE);
    const float velocity[]={12,-24,36};
    MEM16(ENTITY+0x94)=(uint16_t)model;
    for(unsigned i=0;i<3;i++) {
        MEMF(ENTITY+0xb4+i*4)=velocity[i]*delta/60;
        MEMF(ENTITY+0x64+i*4)=10.f*(i+1);
        MEMF(ENTITY+0x1a8+i*4)=.25f*(i+1);
    }
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;MEM32(0x4e8cec)=60;
    unsigned char parent[0x300];memcpy(parent,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(parent));
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=ENTITY;MEM32(STACK+8)=inherit;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xfeed;g_ebp=0xabcd;
    sub_002168C0();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_edi==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    assert(!memcmp(parent,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(parent)));
    assert(allocations==COUNT && effects==1);
    result.random_calls=random_calls;
    for(unsigned i=0;i<COUNT;i++)
        memcpy(result.fragments[i].entity,(void *)((uintptr_t)(DEBRIS+i*STRIDE)+g_xbox_mem_offset),STRIDE);
    return result;
}
static float word_float(const unsigned char *bytes,unsigned offset)
{ float value;memcpy(&value,bytes+offset,4);return value; }
static void equivalent(Result actual,Result reference)
{
    for(unsigned f=0;f<COUNT;f++)for(unsigned axis=0;axis<3;axis++) {
        unsigned offset=0xb4+axis*4;
        float a=word_float(actual.fragments[f].entity,offset);
        float b=word_float(reference.fragments[f].entity,offset);
        assert(isfinite(a) && fabsf(a-b)<.0001f);
        /* Compare every other output byte, including RNG-derived rotation,
         * effect arguments, material selection and allocator failures. */
        memcpy(actual.fragments[f].entity+offset,reference.fragments[f].entity+offset,4);
    }
    assert(!memcmp(&actual,&reference,sizeof(actual)));
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    unsigned cases=0;
    for(unsigned kind=0;kind<2;kind++)for(unsigned inherit=0;inherit<2;inherit++)
    for(unsigned failure=0;failure<=2;failure++) {
        unsigned model=kind?0x51e:0x539;
        Result reference=run(1,1,0,model,inherit,failure);
        Result native60=run(1,1,1,model,inherit,failure);
        assert(!memcmp(&native60,&reference,sizeof(reference)));
        const float deltas[]={.5f,5.f/12,.25f,5.f/48,2.f,6.f};
        for(unsigned i=0;i<sizeof(deltas)/sizeof(deltas[0]);i++) {
            float delta=deltas[i];
            equivalent(run((unsigned)delta,delta,1,model,inherit,failure),reference);
            if(delta<1)equivalent(run(1,delta,1,model,inherit,failure),reference);
            cases++;
        }
        /* This is a one-shot initializer, not a periodic update: zero time
         * still emits debris, but must avoid dividing by zero. */
        equivalent(run(0,0,1,model,inherit,failure),run(0,0,0,model,inherit,failure));
    }
    Result broken=run(0,5.f/12,0,0x539,1,0);
    Result reference=run(1,1,0,0x539,1,0);
    assert(fabsf(word_float(broken.fragments[0].entity,0xb4)-
                 word_float(reference.fragments[0].entity,0xb4))>6.9f);
    printf("debris_counterfactual velocity_error=%.6f,%.6f,%.6f\n",
           word_float(broken.fragments[0].entity,0xb4)-word_float(reference.fragments[0].entity,0xb4),
           word_float(broken.fragments[0].entity,0xb8)-word_float(reference.fragments[0].entity,0xb8),
           word_float(broken.fragments[0].entity,0xbc)-word_float(reference.fragments[0].entity,0xbc));
    printf("PASS: ship debris motion matches original across %u schedules; both models, inheritance, allocation failure, zero time and ABI checked\n",cases);
    free(memory);return 0;
}
