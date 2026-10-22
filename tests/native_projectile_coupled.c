/* Coupled timing audit of the complete second projectile updater, kind-35
 * initializer, target sampler and steering. Scene queries miss a controlled
 * target; effect allocation/position calls are spies, not rendered effects. */
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
/* PROJECTILE_POLICY */
enum { SHOT=0x1000,TARGET=0x2000,FX=0x5000,STACK=0x30000 };
static unsigned sweeps,scans,rng,fx_updates,allocated,retired,opened,seed;
static unsigned lifecycle,hits,impacts,damage_calls,cleanups,quiet;
static float hit_plane,record_multiplier;
static unsigned damage_records,power_calls;
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected coupled projectile tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected coupled projectile callback")
#undef RECOMP_LOAD_GLOBAL_U32
static uint32_t gate(uint32_t site,uint32_t address,uint32_t value) {
    if(opened && site==0xf08d0 && g_tsfp_native_timing_active)return MEMF(0x4e794c)>0;
    return site==0xf08d0 ? pc_native_kind35_gate(value) : pc_native_global_load(site,address,value);
}
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) gate(site,address,value)
void sub_00101660(void) {
    const unsigned ids[]={83,83,84,85};
    assert(allocated<4 && MEM32(g_esp+8)==ids[allocated]);
    assert(MEM32(g_esp+4)==SHOT+0xac+4*allocated);
    assert(MEM32(g_esp+12)==0 && MEM32(g_esp+16)==SHOT+0x1c && MEM32(g_esp+20)==0xffff);
    g_eax=FX+256*allocated++;g_esp+=4;
}
void sub_000FF420(void) {
    const float sizes[]={45,40,37,35};
    uint32_t fx=MEM32(g_esp+4);
    assert(fx>=FX && fx<FX+1024 && (fx-FX)%256==0);
    assert(MEMF(g_esp+8)==sizes[(fx-FX)/256]);g_esp+=4;
}
void sub_000FF490(void) {
    uint32_t fx=MEM32(g_esp+4),point=MEM32(g_esp+8);
    assert(fx>=FX && fx<FX+1024 && (fx-FX)%256==0);
    for(unsigned i=0;i<3;i++) {
        assert(isfinite(MEMF(point+i*4)));MEMF(fx+0x20+i*4)=MEMF(point+i*4);
    }
    fx_updates++;g_esp+=4;
}
void sub_00153B40(void) { rng++;seed=seed*1664525u+1013904223u;g_eax=seed;g_esp+=4; }
void sub_000DCB80(void) {
    assert(g_esi==SHOT);uint32_t delta=MEM32(g_esp+4);
    for(unsigned i=0;i<3;i++)assert(isfinite(MEMF(delta+i*4)));
    sweeps++;g_eax=0;
    if(hit_plane && MEMF(SHOT+0x1c)+MEMF(delta)>=hit_plane) {
        uint32_t point=MEM32(g_esp+8),normal=MEM32(g_esp+12);
        MEMF(point)=hit_plane;MEMF(point+4)=MEMF(point+8)=0;
        MEMF(normal)=-1;MEMF(normal+4)=MEMF(normal+8)=0;
        hits++;g_eax=TARGET;MEM32(MEM32(g_esp+16))=1;
    }
    g_esp+=4;
}
void sub_00088C60(void) { scans++;g_eax=TARGET;g_esp+=4; }
void sub_000EDBE0(void) { g_esp+=4; }
void sub_000DC550(void) { assert(lifecycle);retired++;MEM32(SHOT)=UINT32_MAX;g_esp+=4; }
void sub_00045390(void) { assert(lifecycle);memset((void *)((uintptr_t)MEM32(g_esp+4)+g_xbox_mem_offset),0,48);g_esp+=4; }
void sub_00045820(void) { assert(lifecycle);g_esp+=4; }
void sub_000B91F0(void) { assert(lifecycle && hits==1);memset((void *)((uintptr_t)MEM32(g_esp+4)+g_xbox_mem_offset),0,0x58);g_esp+=4; }
void sub_000B9220(void) { assert(lifecycle && hits==1);g_esp+=4; }
void sub_000BD0F0(void) { assert(lifecycle);damage_calls++;g_esp+=4; }
void sub_000C3C50(void) { assert(lifecycle);g_eax=0;g_esp+=4; }
void sub_000DC2B0(void) { assert(lifecycle && MEM32(g_esp+4)==SHOT && MEM32(g_esp+8)<4);cleanups++;MEM32(SHOT+0xac+4*MEM32(g_esp+8))=0;g_esp+=4; }
void sub_000DC4A0(void) { assert(lifecycle && MEM32(g_esp+4)==23);impacts++;memset((void *)((uintptr_t)0x6000+g_xbox_mem_offset),0,0x22c);g_eax=0x6000;g_esp+=4; }
void sub_000DC5D0(void) { assert(!"Unexpected call sub_000DC5D0"); }
void sub_0012A170(void) {
    assert(lifecycle && hits==1 && MEM32(g_esp+8)==35);
    record_multiplier=MEMF(g_esp+12);damage_records++;g_esp+=4;
}
/* Original CRT adapter accepts base in ST(1), exponent in ST(0), and
 * returns one x87 value. Its adapter bytes are checked by the Python fixture. */
void sub_003C8C64(void) {
    double exponent=g_fp_stack[g_fp_top];g_fp_top=(g_fp_top+1)&7;
    g_fp_stack[g_fp_top]=pow(g_fp_stack[g_fp_top],exponent);
    power_calls++;g_esp+=4;
}
void sub_000DD170(void) { assert(lifecycle);g_eax=0;g_esp+=4; }
void sub_000E2520(void) { assert(!"Unexpected call sub_000E2520"); }
void sub_000E94E0(void) { assert(lifecycle && hits==1 && MEM32(g_esp+4)==SHOT && MEMF(MEM32(g_esp+12))==hit_plane);g_esp+=4; }
void sub_000E9FE0(void) { assert(!"Unexpected call sub_000E9FE0"); }
void sub_000EDD40(void) { assert(lifecycle && MEM32(g_esp+4)==0x6000);g_esp+=4; }
void sub_000F85C0(void) { assert(!"Unexpected call sub_000F85C0"); }
void sub_000FF3D0(void) { assert(!"Unexpected call sub_000FF3D0"); }
void sub_00100260(void) { assert(lifecycle);g_eax=0;g_esp+=4; }
void sub_00100AA0(void) { assert(!"Unexpected call sub_00100AA0"); }
void sub_00105910(void) { assert(!"Unexpected call sub_00105910"); }
void sub_00114AF0(void) { assert(!"Unexpected call sub_00114AF0"); }
void sub_00153BD0(void) { assert(!"Unexpected call sub_00153BD0"); }
void sub_001CDAE0(void) { assert(lifecycle);g_eax=1;g_esp+=4; }
void sub_00238510(void) { assert(!"Unexpected call sub_00238510"); }
void sub_000CFE40(void) { assert(!"Unexpected call sub_000CFE40"); }
extern void sub_000F08D0_gen(void);
extern void sub_000E78F0_gen(void);
extern void sub_000E1FA0(void);
void sub_000F08D0(void) {
    if(opened)sub_000F08D0_gen();
    else pc_native_kind35_update_guest(sub_000F08D0_gen,MEM32(g_esp+4));
}
void sub_000E78F0(void)
{ pc_native_kind35_init_guest(sub_000E78F0_gen,MEM32(g_esp+4)); }
void pc_native_kind35_rng(void)
{ pc_native_kind35_rng_guest(sub_00153B40,MEM32(g_esp),&g_eax,&g_esp); }
void pc_native_kind35_target(void)
{ pc_native_kind35_target_guest(sub_000E1FA0,MEM32(g_esp+4)); }
/* GENERATED_ROUTINES */
static void call(void (*fn)(void)) {
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=SHOT;
    g_esi=0xdead;g_edi=0xfeed;g_ebx=0xbeef;g_ebp=0xabcd;
    fn();
    assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xdead && g_edi==0xfeed && g_ebx==0xbeef && g_ebp==0xabcd);
}
static void run(unsigned fps,int native,float slow,unsigned homing,unsigned open) {
    memset((void *)((uintptr_t)SHOT+g_xbox_mem_offset),0,0x22c);
    memset((void *)((uintptr_t)FX+g_xbox_mem_offset),0,1024);
    MEM32(SHOT)=35;MEMF(SHOT+0x64)=1;MEMF(SHOT+0x88)=240;
    MEM32(SHOT+0xc)=homing?0x2000:0;
    MEM32(SHOT+0x19c)=homing?TARGET:0;
    MEMF(TARGET+0x64)=1000;MEMF(TARGET+0x68)=500;
    sweeps=scans=rng=fx_updates=allocated=retired=0;seed=1;
    g_tsfp_native_timing_active=native;opened=open;
    call(sub_000E78F0);assert(allocated==4);
    unsigned frames=(unsigned)(fps/slow),fresh=0;
    double ticks=0;unsigned previous=0;float delta=60.f/fps*slow;
    for(unsigned i=0;i<frames;i++) {
        ticks+=60.0/fps*slow;
        unsigned whole=(unsigned)(ticks+1e-6)-previous;previous+=whole;
        MEMF(SHOT+0x98)+=delta/60;
        MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
        float before=MEMF(SHOT+0x1c);
        call(sub_000F08D0);
        if(MEMF(SHOT+0x1c)!=before)fresh++;
        assert(!retired);
        assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
        unsigned char saved[0x22c],effects[1024];
        memcpy(saved,(void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof saved);
        memcpy(effects,(void *)((uintptr_t)FX+g_xbox_mem_offset),sizeof effects);
        unsigned prior_rng=rng,prior_scans=scans,prior_sweeps=sweeps,prior_fx=fx_updates;
        MEM32(0x4e7944)=native && !open ? 1 : 0;MEMF(0x4e794c)=0;call(sub_000F08D0);
        assert(!memcmp(saved,(void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof saved));
        assert(!memcmp(effects,(void *)((uintptr_t)FX+g_xbox_mem_offset),sizeof effects));
        assert(rng==prior_rng && scans==prior_scans && sweeps==prior_sweeps && fx_updates==prior_fx);
    }
    assert(rng==(native && !open ? 540 : 9*sweeps) && fx_updates==4*sweeps);
    if(homing)assert(scans>0);else assert(scans==0);
    if(!quiet)printf("coupled_projectile fps=%u native=%d slow=%.2f homing=%u opened=%u frames=%u fresh=%u x=%.6f y=%.6f speed=%.6f scans=%u rng=%u fx=%u\n",
        fps,native,slow,homing,opened,frames,fresh,MEMF(SHOT+0x1c),MEMF(SHOT+0x20),
        MEMF(SHOT+0x88),scans,rng,fx_updates);
}
/* Exercise original impact/expiry branches; allocator, scene collision,
 * sound and damage boundaries are controlled spies. No full damage claim. */
static float run_lifecycle(unsigned fps,float slow,unsigned hit) {
    memset((void *)((uintptr_t)SHOT+g_xbox_mem_offset),0,0x22c);
    MEM32(SHOT)=35;MEMF(SHOT+0x64)=1;MEMF(SHOT+0x88)=240;
    MEM32(TARGET+0x20)=0;MEM32(TARGET+0x28)=0;
    sweeps=scans=rng=fx_updates=allocated=retired=0;seed=1;
    MEM32(SHOT+8)=UINT32_MAX; /* captured kind-35 damage profile */
    damage_records=power_calls=0;record_multiplier=-1;
    hits=impacts=damage_calls=cleanups=0;lifecycle=1;hit_plane=hit?100.f:0;
    g_tsfp_native_timing_active=1;opened=0;call(sub_000E78F0);
    double ticks=0;unsigned previous=0;float delta=60.f/fps*slow;
    for(unsigned i=0;i<fps*20/slow && !retired;i++) {
        ticks+=60.0/fps*slow;unsigned whole=(unsigned)(ticks+1e-6)-previous;previous+=whole;
        MEMF(SHOT+0x98)+=delta/60;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
        call(sub_000F08D0);
    }
    assert(retired==1 && impacts==1 && cleanups==4 && hits==hit && damage_calls==hit);
    assert(MEM32(SHOT)==UINT32_MAX);
    assert(damage_records==hit && power_calls==0);if(hit)assert(record_multiplier==0);
    assert(!pc_kind35_context.cache);lifecycle=0;hit_plane=0;
    printf("projectile_lifecycle fps=%u slow=%.2f hit=%u age=%.6f impacts=%u damage=%u cleanup=%u\n",
           fps,slow,hit,MEMF(SHOT+0x98),impacts,damage_calls,cleanups);
    return MEMF(SHOT+0x98);
}
/* Caller contract beyond the captured profile=-1 case. Non-flat profiles
 * are deliberate controls: these combinations have not been observed live. */
static void damage_profile_controls(void) {
    memset((void *)((uintptr_t)SHOT+g_xbox_mem_offset),0,0x22c);
    MEM32(SHOT)=35;MEM32(0x7000)=1;MEM32(0x7004)=0;
    lifecycle=1;hits=1;
    const uint32_t profiles[]={UINT32_MAX,2,9,11,12};
    const float distances[]={0,16,4096};
    for(unsigned p=0;p<5;p++)for(unsigned d=0;d<3;d++) {
        uint32_t profile=profiles[p];MEM32(SHOT+8)=profile;MEMF(SHOT+0xf4)=distances[d];
        damage_records=power_calls=0;
        g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=SHOT;MEM32(STACK+8)=0x7000;
        g_esi=0xdead;g_edi=0xbeef;sub_000DCEF0();
        assert(g_esp==STACK+4 && g_fp_top==0 && g_esi==0xdead && g_edi==0xbeef);
        assert(damage_records==1);
        float expected=profile==UINT32_MAX?0:MEMF(0x4dad78+profile*60);
        if(profile!=UINT32_MAX)expected*=powf(MEMF(0x4dad4c+profile*60),sqrtf(distances[d]));
        assert(fabsf(record_multiplier-expected)<.000001f);
        assert(power_calls==((profile==11 || profile==12) && d>0));
        printf("damage_profile profile=%u accumulator=%.0f multiplier=%.9f pow=%u\n",
            profile,distances[d],record_multiplier,power_calls);
    }
    lifecycle=0;
}
static void uneven_motion(void) {
    memset((void *)((uintptr_t)SHOT+g_xbox_mem_offset),0,0x22c);
    MEM32(SHOT)=35;MEMF(SHOT+0x64)=1;MEMF(SHOT+0x88)=240;
    sweeps=scans=rng=fx_updates=allocated=retired=0;seed=1;
    g_tsfp_native_timing_active=1;opened=0;call(sub_000E78F0);
    const float schedule[]={.125f,.875f,2,.25f,.75f,6};
    double ticks=0;unsigned previous=0,random_frames=0;
    float expected_x=0,expected_speed=240;
    for(unsigned i=0;i<36;i++) {
        float delta=schedule[i%6];ticks+=delta;
        unsigned whole=(unsigned)ticks-previous;previous+=whole;
        MEMF(SHOT+0x98)+=delta/60;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
        expected_x+=delta/60*expected_speed;
        for(unsigned j=0;j<whole;j++)expected_speed*=MEMF(0x47d780);
        random_frames+=whole!=0;
        call(sub_000F08D0);
        assert(fabsf(MEMF(SHOT+0x1c)-expected_x)<.001f);
        assert(fabsf(MEMF(SHOT+0x88)-expected_speed)<.001f);
        assert(!retired && !pc_kind35_context.cache);
    }
    assert(previous==60 && rng==9*random_frames && fx_updates==4*36 && sweeps==36);
}
static void missing_emitters(void) {
    quiet=1;run(144,1,1,0,0);quiet=0;
    /* Remove the first randomized emitter after its original jitter sample.
     * Remaining emitters must keep their own jitter on fractional callbacks. */
    MEM32(SHOT+0xac)=0;
    float before[6];
    for(unsigned j=0;j<2;j++)for(unsigned k=0;k<3;k++)
        before[j*3+k]=MEMF(FX+(j+1)*256+0x20+k*4)-MEMF(SHOT+0x1c+k*4);
    unsigned old_rng=rng,old_fx=fx_updates;
    MEM32(0x4e7944)=0;MEMF(0x4e794c)=.25f;call(sub_000F08D0);
    assert(rng==old_rng && fx_updates==old_fx+3);
    for(unsigned j=0;j<2;j++)for(unsigned k=0;k<3;k++)
        assert(fabsf(before[j*3+k]-(MEMF(FX+(j+1)*256+0x20+k*4)-MEMF(SHOT+0x1c+k*4)))<.0001f);
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;MEM32(0x737bd4)=SHOT;
    setvbuf(stdout,NULL,_IONBF,0);
    for(unsigned homing=0;homing<2;homing++) {
        run(60,0,1,homing,0);
        unsigned char original[0x22c];
        memcpy(original,(void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof original);
        const unsigned rates[]={60,120,144};
        for(unsigned i=0;i<3;i++)for(unsigned slow=0;slow<2;slow++) {
            run(rates[i],1,slow?.25f:1,homing,0);
            if(i==0 && !slow)assert(!memcmp(original,
                (void *)((uintptr_t)SHOT+g_xbox_mem_offset),sizeof original));
        }
        run(144,1,1,homing,1);
    }
    for(unsigned hit=0;hit<2;hit++) {
        float base=run_lifecycle(60,1,hit);
        const unsigned rates[]={60,120,144};
        for(unsigned i=0;i<3;i++)for(unsigned slow=0;slow<2;slow++)
            assert(fabsf(run_lifecycle(rates[i],slow?.25f:1,hit)-base)<1.f/60+.0001f);
    }
    missing_emitters();
    uneven_motion();
    damage_profile_controls();
    puts("PASS: coupled projectile audit executes original initialization, motion, steering and effect positioning");
    free(memory);return 0;
}
