/* Original TRAP initializer/callback -> entity post-update -> cached Euler
 * matrix -> affine point transform. Collision/sector services and registration
 * are controlled spies; no renderer or normal-play activation is simulated. */
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
enum { ENTITY=0x1000, MATRIX=0x4000, POINT=0x5000, STACK=0x10000 };
static unsigned registered;
static int original_point;
#undef RECOMP_FAST_ENTRY
#define RECOMP_FAST_ENTRY(site) ((site)==0x151E80u && !original_point && pc_model_transform_entry())
void missing_matrix_copy(void) {g_esp+=4;}
void sub_0023FF20(void)
{ assert(MEM32(g_esp+4)==0x54524150);g_eax=ENTITY;g_esp+=4; }
void sub_000A5CE0(void)
{
    assert(MEM32(g_esp+4)==ENTITY && MEM32(g_esp+8)==3);
    assert(MEM32(g_esp+12)==0x1d8ab0);registered++;g_esp+=4;
}
#define UNEXPECTED(name) void name(void) {assert(!"Unexpected " #name);}
UNEXPECTED(sub_00046A00) UNEXPECTED(sub_000462B0)
UNEXPECTED(sub_00057040) UNEXPECTED(sub_000A5F40) UNEXPECTED(sub_000A6290)
UNEXPECTED(sub_00129920) UNEXPECTED(sub_000A64D0) UNEXPECTED(sub_0012C1F0)
UNEXPECTED(sub_00238510)
static unsigned sector_queries,bounds_queries;
void sub_000A6530(void) {assert(MEM32(g_esp+4)==ENTITY);g_esp+=4;}
void sub_0009FBE0(void) {assert(MEM32(g_esp+4)==ENTITY);sector_queries++;g_esp+=4;}
void sub_0009FD60(void) {assert(MEM32(g_esp+4)==ENTITY);bounds_queries++;g_esp+=4;}
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected matrix reflection target")
/* GENERATED_ROUTINES */
static void stack(void)
{
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=g_seh_ebp=0;
}
static void returned(void)
{
    assert(g_esp==STACK+4 && !g_fp_top && MEM32(STACK)==0x12345678);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed);
}
static void point_transform(void) {sub_00151E80();}
static void transform(int changed)
{
    stack();g_eax=ENTITY;sub_000A7360();returned();
    assert(!!(MEM32(ENTITY+0x28)&0x1000)==changed);
    stack();MEM32(STACK+4)=MATRIX;MEM32(STACK+8)=ENTITY;
    MEM32(STACK+12)=1;MEM32(STACK+16)=0;
    sub_000A14B0();returned();
    assert(!(MEM32(ENTITY+0x28)&0x1000));
    assert(MEMF(ENTITY+0x104)==MEMF(ENTITY+0xc0));
    float radians=MEMF(ENTITY+0xc0)*MEMF(0x475d0c);
    assert(fabsf(MEMF(MATRIX+0x14)-cosf(radians))<.000001f);
    assert(fabsf(MEMF(MATRIX+0x18)-sinf(radians))<.000001f);
    for(unsigned i=0;i<16;i++)assert(isfinite(MEMF(MATRIX+i*4)));
    MEMF(POINT)=0;MEMF(POINT+4)=1;MEMF(POINT+8)=0;
    stack();MEM32(STACK+4)=MATRIX;MEM32(STACK+8)=POINT;
    point_transform();returned();
    float accelerated[3];memcpy(accelerated,(void *)(g_xbox_mem_offset+POINT),sizeof accelerated);
    original_point=1;
    MEMF(POINT)=0;MEMF(POINT+4)=1;MEMF(POINT+8)=0;
    stack();MEM32(STACK+4)=MATRIX;MEM32(STACK+8)=POINT;
    point_transform();returned();original_point=0;
    assert(!memcmp(accelerated,(void *)(g_xbox_mem_offset+POINT),sizeof accelerated));
    assert(MEMF(POINT)==3.f);
    assert(fabsf(MEMF(POINT+4)-(4.f+cosf(radians)))<.000001f);
    assert(fabsf(MEMF(POINT+8)-(5.f+sinf(radians)))<.000001f);
}
static void prepare(void)
{
    memset((void *)(g_xbox_mem_offset+ENTITY),0,0x2e8);
    MEM32(ENTITY+0x20)=1;registered=sector_queries=bounds_queries=0;
    stack();sub_001D8C40();returned();
    assert(registered==1 && MEM8(ENTITY+0x32)==2);
    for(unsigned i=0;i<3;i++)MEMF(ENTITY+0x64+i*4)=MEMF(ENTITY+0xec+i*4)=3.f+i;
    MEM32(0x4e7944)=1;MEMF(0x4e794c)=1;
    transform(1);
}
int main(void)
{
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;
    for(unsigned i=0;i<16;i++)MEMF(0x4e7900+i*4)=i%5==0?1.f:0.f;
    stack();sub_00153C20();returned();
    setvbuf(stdout,NULL,_IONBF,0);
    puts("trap transform propagation audit");
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned phase=0;phase<4;phase++)for(unsigned native=0;native<2;native++) {
        g_tsfp_native_timing_active=native;prepare();
        unsigned den=rates[r]*slow,count=den*3/10,previous=0,fresh=0,updates=0;
        float previous_point[3];memcpy(previous_point,(void *)(g_xbox_mem_offset+POINT),sizeof previous_point);
        printf("trap transform fps=%u slow=%u phase=%u native=%u\n",rates[r],slow,phase,native);
        for(unsigned f=1;f<=count;f++) {
            unsigned tick=(f*240+phase*den)/(den*4),whole=tick-previous;previous=tick;
            MEM32(0x4e7944)=whole;MEMF(0x4e794c)=60.f/den;
            float before=MEMF(ENTITY+0xc0);
            stack();MEM32(STACK+4)=ENTITY;sub_001D8AB0();returned();
            int changed=MEMF(ENTITY+0xc0)!=before;
            if(changed)updates++;else assert(!native);
            transform(changed);
            if(memcmp(previous_point,(void *)(g_xbox_mem_offset+POINT),sizeof previous_point))fresh++;
            memcpy(previous_point,(void *)(g_xbox_mem_offset+POINT),sizeof previous_point);
            assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==60.f/den);
        }
        printf("trap transform result fresh=%u updates=%u frames=%u\n",fresh,updates,count);
        assert(fresh==updates && updates==(native?count:previous));
        assert(sector_queries==count+1 && bounds_queries==count+1);
        unsigned char snapshot[0x2e8];memcpy(snapshot,(void *)(g_xbox_mem_offset+ENTITY),sizeof snapshot);
        MEMF(0x4e794c)=0;MEM32(0x4e7944)=native?1:0;
        stack();MEM32(STACK+4)=ENTITY;sub_001D8AB0();returned();
        assert(!memcmp(snapshot,(void *)(g_xbox_mem_offset+ENTITY),sizeof snapshot));
        transform(0);
        assert(!memcmp(previous_point,(void *)(g_xbox_mem_offset+POINT),sizeof previous_point));
    }
    puts("PASS: original TRAP initialization, post-update invalidation, Euler cache and transformed vertices through240");
    free(memory);return 0;
}
