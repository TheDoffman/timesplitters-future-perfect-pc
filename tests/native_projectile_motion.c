/* Execute the complete original projectile updater against a deterministic
 * collision plane. Scene queries, damage application and effects are captured
 * services: this verifies travel and impact dispatch, not enemy health or art. */
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
enum { SHOT=0x1000,TARGET=0x2000,STACK=0x30000 };
static unsigned sweeps,hits,retired,cleanups,damage_calls,damage_records;
static float record_multiplier;
static float wall,expected_step;
static int collide;
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected projectile tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected projectile callback")
/* Deterministic plane replaces scene geometry, but the complete original
 * updater computes the sweep and handles its result/impact/retirement. */
void sub_000E97A0(void)
{
    assert(g_eax==SHOT);
    uint32_t delta=MEM32(g_esp+4),normal=MEM32(g_esp+8);
    assert(fabsf(MEMF(delta)-expected_step)<.0001f);
    assert(MEMF(delta+4)==0 && MEMF(delta+8)==0);
    sweeps++;
    if(collide && MEMF(SHOT+0x1c)+MEMF(delta)>=wall) {
        assert(MEMF(SHOT+0x1c)<=wall);
        MEMF(g_ebx)=wall;MEMF(g_ebx+4)=MEMF(g_ebx+8)=0;
        MEMF(normal)=-1;MEMF(normal+4)=MEMF(normal+8)=0;
        g_eax=TARGET;hits++;
        MEM32(MEM32(g_esp+12))=1;
    } else g_eax=0;
    g_esp+=4;
}
void sub_000E9FE0(void) {
    assert(MEM32(g_esp+4)==SHOT && MEM32(g_esp+12)==TARGET);
    assert(MEMF(MEM32(g_esp+16))==wall);
    damage_calls++;g_esp+=4;
}
void sub_000EDBE0(void) { g_esp+=4; }
void sub_000E1EB0(void) { assert(g_eax==SHOT);cleanups++;g_esp+=4; }
void sub_000DC550(void) { assert(MEM32(g_esp+4)==SHOT);retired++;g_esp+=4; }
void sub_000913D0(void) { g_eax=0;g_esp+=4; }
void sub_000DC5D0(void) { g_esp+=4; }
void sub_000E1F80(void) { g_esp+=4; }
void sub_000E94E0(void) { g_esp+=4; }
void sub_000B91F0(void) { g_esp+=4; }
void sub_000B9220(void) { g_esp+=4; }
void sub_000BD0F0(void) { g_esp+=4; }
void sub_000F4920(void) { g_esp+=4; }
void sub_000E1400(void) { g_eax=0;g_esp+=4; }
void sub_000DC1D0(void) { assert(!"Unexpected call 000DC1D0"); }
void sub_0012A170(void) {
    assert(MEM32(g_esp+8)==MEM32(SHOT));
    record_multiplier=MEMF(g_esp+12);damage_records++;g_esp+=4;
}
void sub_00238510(void) { assert(!"Unexpected player statistics"); }
void sub_003C8C64(void) { assert(!"Captured projectile unexpectedly used falloff exponent"); }
void sub_000DD130(void) { assert(!"Unexpected call 000DD130"); }
void sub_000E17C0(void) { assert(!"Unexpected call 000E17C0"); }
void sub_000E9690(void) { assert(!"Unexpected call 000E9690"); }
void sub_000FA540(void) { assert(!"Unexpected call 000FA540"); }
void sub_000FF420(void) { assert(!"Unexpected call 000FF420"); }
void sub_000FF490(void) { assert(!"Unexpected call 000FF490"); }
void sub_000FFDA0(void) { assert(!"Unexpected call 000FFDA0"); }
void sub_00102320(void) { assert(!"Unexpected call 00102320"); }
void sub_0010A230(void) { assert(!"Unexpected call 0010A230"); }
void sub_002274C0(void) { assert(!"Unexpected call 002274C0"); }
void sub_00249B10(void) { assert(!"Unexpected call 00249B10"); }
void sub_0024A150(void) { assert(!"Unexpected call 0024A150"); }
/* GENERATED_ROUTINES */
static void step(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    expected_step=MEMF(SHOT+0x88)*delta/60;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=SHOT;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xfeed;g_ebp=0xabcd;
    sub_000F4A50();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_edi==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static float run(unsigned fps,int native,unsigned kind,float speed,float slow,int hit)
{
    memset((void *)((uintptr_t)SHOT+g_xbox_mem_offset),0,0x22c);
    MEM32(SHOT)=kind;MEMF(SHOT+0x64)=MEMF(SHOT+0x70)=MEMF(SHOT+0x7c)=1;
    MEMF(SHOT+0x88)=speed;MEM32(TARGET+0x20)=8;
    MEM32(SHOT+8)=kind==31?8:2;
    sweeps=hits=retired=cleanups=damage_calls=damage_records=0;record_multiplier=-1;collide=hit;wall=speed*(hit==2?1.5f:.75f);
    g_tsfp_native_timing_active=native;
    double ticks=0;unsigned previous=0;float delta=60.f/fps*slow;
    for(unsigned frame=0;frame<fps*9 && !retired;frame++) {
        ticks+=(60.0/fps)*slow;unsigned whole=(unsigned)(ticks+1e-6)-previous;previous+=whole;
        MEMF(SHOT+0x98)+=delta/60; // outer DC7B0 updates age on every callback
        unsigned before=sweeps;float x=MEMF(SHOT+0x1c);
        step(whole,delta);
        if(native && !retired)assert(sweeps==before+1);
        if(!retired && native)assert(fabsf(MEMF(SHOT+0x1c)-x-expected_step)<.0001f);
        unsigned char paused[0x22c];memcpy(paused,(void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof(paused));
        before=sweeps;step(0,0);
        assert(before==sweeps && !memcmp(paused,(void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof(paused)));
    }
    if(native && hit) assert(hits==1 && damage_calls==1 && retired==1 && cleanups==1
        && damage_records==1 && record_multiplier==1);
    if(native && !hit)assert(retired==1 && damage_calls==0 && cleanups==1);
    printf("projectile fps=%u native=%d kind=%u slow=%.2f hit=%d time=%.6f distance=%.6f sweeps=%u damage_callbacks=%u multiplier=%.6f accumulator=%.6f\n",
           fps,native,kind,slow,hit,MEMF(SHOT+0x98),MEMF(SHOT+0x1c),sweeps,damage_calls,record_multiplier,MEMF(SHOT+0xf4));
    return MEMF(SHOT+0x98);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;
    const unsigned kinds[]={28,31,45,46};const float speeds[]={180,240,480,240};
    const unsigned rates[]={60,120,144,180,240};
    /* Byte-identical native-60 arithmetic and callback counts. */
    run(60,0,31,240,1,0);
    unsigned char reference_bytes[0x22c];
    memcpy(reference_bytes,(void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof(reference_bytes));
    unsigned reference_sweeps=sweeps;
    run(60,1,31,240,1,0);
    assert(sweeps==reference_sweeps && !memcmp(reference_bytes,
        (void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof(reference_bytes)));
    for(unsigned kind=0;kind<4;kind++) {
        float reference=run(60,0,kinds[kind],speeds[kind],1,1);
        for(unsigned slow=0;slow<2;slow++)for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++) {
            unsigned fps=rates[rate];
            float hit_time=run(fps,1,kinds[kind],speeds[kind],slow?.25f:1,1);
            assert(fabsf(hit_time-reference)<.018f);
            run(fps,1,kinds[kind],speeds[kind],slow?.25f:1,0);
        }
    }
    run(144,1,31,240,1,2);assert(damage_calls==1);
    run(144,0,31,240,1,2);assert(damage_calls==0 && retired==1);
    /* Uneven fractional intervals, pauses and original multi-tick catch-up. */
    memset((void *)((uintptr_t)SHOT+g_xbox_mem_offset),0,0x22c);
    MEM32(SHOT)=31;MEMF(SHOT+0x64)=1;MEMF(SHOT+0x88)=240;
    g_tsfp_native_timing_active=1;collide=0;retired=0;
    const float intervals[]={2.f/12,8.f/12,3.f/12,10.f/12,0,5.f/12,2,6};
    float expected=0;
    for(unsigned repeat=0;repeat<5;repeat++)for(unsigned i=0;i<8;i++) {
        float delta=intervals[i];step((unsigned)delta,delta);expected+=4*delta;
        assert(fabsf(MEMF(SHOT+0x1c)-expected)<.001f);
    }
    run(144,0,31,240,1,1);assert(hits==1 && MEMF(SHOT+0x98)>1.79f);
    free(memory);puts("PASS: original projectile travel, swept impact dispatch, lifetime, pause and ABI");return 0;
}
