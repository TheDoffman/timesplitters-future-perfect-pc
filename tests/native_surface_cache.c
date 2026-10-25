/* Original surface-motion/cache caller and complete procedural-texture
 * dispatcher. Graphics/pipeline passes are spies, not a GPU simulation.
 * Two material entries share one procedural resource and one active view. */
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
enum { MATERIALS=0x1000,PARAMS=0x2000,TEXTURE=0x3000,RESOURCE=0x4000,
       PLAYER=0x5000,STACK=0x10000 };
static unsigned passes,updates,state_calls,initializing,clears,bindings;
static uint32_t handle_a,handle_b;
static int unpatched;
static uint32_t load(uint32_t site,uint32_t address,uint32_t value) {
    if(unpatched && (((site==0x12764c || site==0x127694) && address==0x7497a8) ||
                     (site==0x1956a0 && address==0x4e7944)))
        return value;
    return pc_native_global_load(site,address,value);
}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) load(site,address,value)
#undef RECOMP_ITAIL
void sub_0001EDE0(void);
#define RECOMP_ITAIL(target) do { assert((target)==0x1ede0); sub_0001EDE0(); return; } while(0)
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected surface callback")
static void state(void) { state_calls++;g_eax=7;g_esp+=4; }
void sub_00019650(void) { state(); }
void sub_0001ED60(void) { state(); }
void sub_00023270(void) { state(); }
void sub_00021450(void) { state(); }
void sub_000214C0(void) { state(); }
void sub_0001EDE0(void) {
    if(initializing) {
        unsigned phase=bindings%3;
        uint32_t target=phase==2?0xfffffffau:MEM32(MEM32(g_esi+0x20)+(phase?8:0));
        assert(MEM32(g_esp+4)==target && MEM32(g_esp+8)==0xffffffff);
        bindings++;g_esp+=4;
    }
    else state();
}
void sub_0001A0D0(void) {
    assert(initializing && MEM32(g_esp+4)==15);
    assert(MEM32(g_esp+8)==0 && MEM32(g_esp+12)==0);
    assert(MEM32(g_esp+16)==128 && MEM32(g_esp+20)==128);
    assert(MEM32(g_esp+24)==0x40404040);
    clears++;g_esp+=4;
}
static void pass(unsigned index) {
    assert(passes%5==index && g_esi==RESOURCE && g_edi==RESOURCE);
    assert(MEM32(RESOURCE+0x24)==(index<3?handle_b:handle_a));
    assert(MEM32(RESOURCE+0x28)==(index<3?handle_a:handle_b));
    passes++;if(index==4)updates++;g_esp+=4;
}
void sub_001953B0(void) { pass(0); }
void sub_001952D0(void) { pass(1); }
void sub_001950B0(void) { pass(2); }
void sub_00195110(void) { pass(3); }
void sub_00195280(void) { pass(4); }
/* GENERATED_ROUTINES */
static void setup(unsigned view) {
    memset((void *)(g_xbox_mem_offset+MATERIALS),0,0x6000);
    MEM32(0x74227c)=2;MEM32(0x7ac6e4)=MATERIALS;MEM32(0x7b0cc0)=PLAYER;
    MEM32(PLAYER+0x214)=view;MEM32(0x4e8cec)=60;
    MEMF(TEXTURE+0x10)=MEMF(TEXTURE+0x14)=64;
    MEM32(RESOURCE+0x24)=handle_a=0x1111;MEM32(RESOURCE+0x28)=handle_b=0x2222;
    for(unsigned i=0;i<4;i++)MEM32(RESOURCE+4+i*4)=0xffffffffu;
    /* Constant velocity and zero forcing/drag isolate the cache defect.
     * The U/V and phase integrations still run through original code. */
    MEMF(PARAMS+0x10)=.1f;
    for(unsigned i=0;i<2;i++) {
        uint32_t p=MATERIALS+i*0xc0;
        MEM32(p)=PARAMS;MEM32(p+4)=TEXTURE;MEM32(p+0x24)=0x10;
        MEMF(p+0xa8)=2;MEMF(p+0xac)=-1;
        MEMF(p+0xb0)=.1f;MEMF(p+0xb4)=.4f;MEM32(p+0xbc)=RESOURCE;
    }
    passes=updates=state_calls=0;
}
static void step(unsigned frame,unsigned vblank,unsigned tick,unsigned whole,float delta) {
    MEM32(0x7497b8)=frame;MEM32(0x7497a8)=vblank;MEM32(0x7de318)=tick;
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    sub_00127500();
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x7497b8)==frame && MEM32(0x7497a8)==vblank);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta && MEM32(0x7de318)==tick);
    assert(passes==updates*5 && state_calls==updates*13);
    assert(MEM32(RESOURCE+0x24)==handle_a && MEM32(RESOURCE+0x28)==handle_b);
}
static uint32_t acquire(unsigned kind) {
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=kind;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    initializing=1;sub_00127400();initializing=0;
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    return g_eax;
}
static void lifecycle(void) {
    setup(0);g_tsfp_native_timing_active=1;
    MEM32(0x742350)=RESOURCE;MEM32(0x7421a0)=0;
    /* Five slots, poisoned stamps: execute the actual allocator and resource
     * initializer. GPU target binding/clearing remain spies. */
    memset((void *)(g_xbox_mem_offset+RESOURCE),0xa5,5*0xfc);
    for(unsigned i=0;i<5;i++)MEM32(RESOURCE+i*0xfc)=0xffffffff;
    bindings=clears=0;
    for(unsigned i=0;i<20;i++)MEM32(0x74fa20+i*4)=100+i;
    for(unsigned i=0;i<5;i++) {
        uint32_t p=acquire(i);
        assert(p==RESOURCE+i*0xfc && MEM32(p)==i);
        for(unsigned view=0;view<6;view++)assert(MEM32(p+4+view*4)==0xffffffff);
        assert(MEM32(p+0x1c)==0x4e8d98+i*32);
        assert(MEM32(p+0x20)==0x74fa20+i*16);
        assert(MEM32(p+0x24)==0 && MEM32(p+0x28)==2);
        assert(MEM32(p+0x2c)==1 && MEM32(p+0x30)==3 && MEM32(p+0xf4)==0);
    }
    assert(clears==10 && bindings==15 && MEM32(0x7421a0)==5);
    assert(acquire(9)==0 && clears==10); /* exhausted pool */
    handle_a=0;handle_b=2;
    for(unsigned view=0;view<4;view++) {
        MEM32(PLAYER+0x214)=view;
        step(100,200,1,1,1);assert(updates==view+1);
        assert(acquire(0)==RESOURCE && clears==10);
        assert(MEM32(RESOURCE+4+view*4)==100);
        step(100,200,1,1,1);assert(updates==view+1);
    }
    /* Recycle one slot at the same native update ID: initialization must
     * invalidate its old view stamps so the new resource receives work. */
    MEM32(RESOURCE)=0xffffffff;
    assert(acquire(9)==RESOURCE && clears==12 && bindings==18);
    for(unsigned view=0;view<6;view++)assert(MEM32(RESOURCE+4+view*4)==0xffffffff);
    step(100,200,1,1,1);assert(updates==5);
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);
    unsigned losses=0;const unsigned rates[]={60,120,144,180,240};
    for(unpatched=1;unpatched>=0;unpatched--)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned r=0;r<sizeof(rates)/sizeof(rates[0]);r++)
    for(unsigned phase=0;phase<4;phase++)for(unsigned view=0;view<4;view+=3) {
        setup(view);g_tsfp_native_timing_active=1;
        unsigned previous=0;
        for(unsigned f=1;f<=rates[r]*slow*8;f++) {
            unsigned tick=f*60/(rates[r]*slow),whole=tick-previous;
            unsigned vblank=100+(f*60*4+phase*rates[r])/(rates[r]*4);
            unsigned before=updates;
            float prior_u=MEMF(MATERIALS+0xb0),prior_v=MEMF(MATERIALS+0xb4);
            float prior_phase=MEMF(MATERIALS+0xb8);
            step(f,vblank,tick,whole,60.f/(rates[r]*slow));
            /* Every fractional frame must advance the original material's
             * scrolling and phase, even when its discrete GPU image holds.
             * Compare the independent constant-velocity solution each step. */
            double seconds=(double)f/(rates[r]*slow);
            for(unsigned material=0;material<2;material++) {
                uint32_t p=MATERIALS+material*0xc0;
                assert(fabs(MEMF(p+0xb0)-(.1+2*seconds/64))<.0001);
                assert(fabs(MEMF(p+0xb4)-(.4-seconds/64))<.0001);
                assert(fabs(MEMF(p+0xb8)-.1*seconds)<.0001);
            }
            assert(MEMF(MATERIALS+0xb0)>prior_u && MEMF(MATERIALS+0xb4)<prior_v);
            assert(MEMF(MATERIALS+0xb8)>prior_phase);
            assert(updates-before<=1 && (whole || updates==before));
            if(!unpatched)assert(updates-before==(whole?1u:0u));
            previous=tick;
        }
        printf("surface_cache original=%d fps=%u slow=%u vblank_phase=%u view=%u updates=%u expected=480\n",
               unpatched,rates[r],slow,phase,view,updates);
        if(unpatched && updates<480)losses++;
        if(!unpatched || rates[r]==60)assert(updates==480);
        for(unsigned i=0;i<2;i++) {
            uint32_t p=MATERIALS+i*0xc0;
            assert(fabsf(MEMF(p+0xb0)-.35f)<.0001f);
            assert(fabsf(MEMF(p+0xb4)-.275f)<.0001f);
            assert(fabsf(MEMF(p+0xb8)-.8f)<.0001f);
        }
    }
    assert(losses>0);unpatched=0;
    /* Original inactive path still uses its vblank key, even when the native
     * frame id changes. Native pause/resume must not consume a real update. */
    for(unsigned native=0;native<2;native++) {
        setup(0);g_tsfp_native_timing_active=native;
        step(1,100,1,1,1);assert(updates==1);
        step(2,100,1,0,0);assert(updates==1);
        step(3,100,2,1,1);assert(updates==(native?2u:1u));
        step(4,101,3,1,1);assert(updates==(native?3u:2u));
        assert(pc_native_global_load(0x12764d,0x7497a8,999)==999);
    }
    unpatched=1;setup(0);g_tsfp_native_timing_active=1;
    step(1,100,1,1,0);assert(updates==1); /* original paused-image counterexample */
    unpatched=0;setup(0);
    step(1,100,1,1,1);assert(updates==1);
    /* A temporary zero-float pause must not evolve the procedural image
     * from a stale positive whole count, even on a new display tick. */
    step(2,101,1,1,0);assert(updates==1);
    step(3,101,2,1,1);assert(updates==2);
    step(4,102,5,3,2.75f);assert(updates==3); /* original one-dispatch catch-up */
    /* Each view owns its stamp; aliases within a view must still deduplicate. */
    MEM32(PLAYER+0x214)=1;
    step(4,102,5,3,2.75f);assert(updates==4);
    /* Inactive-native retains the original stale-whole behavior as well. */
    setup(0);g_tsfp_native_timing_active=0;
    step(1,100,1,1,0);assert(updates==1);
    lifecycle();
    puts("PASS: procedural texture cadence, original UV motion, resource allocation/reuse and per-view cache lifetime");
    free(memory);return 0;
}
