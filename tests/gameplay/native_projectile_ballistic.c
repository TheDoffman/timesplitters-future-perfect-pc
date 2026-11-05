/* Original US flare/vehicle ballistic integrator. Only scene queries and
 * particle/sound services are controlled; initialization and motion execute. */
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
enum { SHOT=0x1000, STACK=0x30000 };
static unsigned random_state,random_calls,smoke,sweeps,retired;
void sub_00153B40(void) {
    random_state=random_state*1664525u+1013904223u;
    g_eax=random_state;random_calls++;g_esp+=4;
}
void sub_00153BD0(void) {
    sub_00153B40();g_fp_top=(g_fp_top-1)&7;
    g_fp_stack[g_fp_top]=g_eax/4294967296.0;
}
void pc_native_kind35_rng(void) {
    pc_native_kind35_rng_guest(sub_00153B40,MEM32(g_esp),&g_eax,&g_esp);
}
void pc_native_projectile_smoke_rng(void) {
    if(!pc_native_projectile_skip_smoke()){sub_00153BD0();return;}
    g_fp_top=(g_fp_top-1)&7;g_fp_stack[g_fp_top]=.5;g_esp+=4;
}
void sub_000F85C0(void){smoke++;g_esp+=4;}
void pc_native_projectile_smoke(void) {
    if(!pc_native_projectile_skip_smoke()){sub_000F85C0();return;}
    g_esp+=4;
}
void sub_000DCB80(void){assert(g_esi==SHOT);sweeps++;g_eax=0;g_esp+=4;}
void sub_000DC550(void){retired++;g_esp+=4;}
void sub_000EDBE0(void){assert(MEM32(g_esp+12)==SHOT);g_esp+=4;}
void pc_native_ballistic_finish(void){pc_native_ballistic_finish_guest(sub_000EDBE0,g_esi);}
/* The original CRT entry exchanges angle/period before FPREM and retires
 * one x87 cell. The platform libm supplies that same finite remainder. */
void sub_003C9DDE(void) {
    double period=g_fp_stack[g_fp_top],angle=g_fp_stack[(g_fp_top+1)&7];
    g_fp_top=(g_fp_top+1)&7;g_fp_stack[g_fp_top]=fmod(angle,period);g_esp+=4;
}
/* UNREACHED_SERVICES */
/* GENERATED_ROUTINES */
static void initialize(void){pc_native_kind35_init_guest(sub_000E2F90_gen,MEM32(g_esp+4));}
static void update(void){pc_native_kind35_update_guest(sub_000F1A90_gen,MEM32(g_esp+4));}
static void call(void (*f)(void)) {
    g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=SHOT;
    g_esi=0xabc;g_edi=0xdef;g_ebx=0x123;g_ebp=0x456;
    f();assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xabc && g_edi==0xdef && g_ebx==0x123 && g_ebp==0x456);
}
typedef struct Motion {float x,y,z,speed,minimum,maximum;unsigned random,smoke;} Motion;
static Motion run(unsigned kind,unsigned fps,int native,float slow,float duration) {
    memset((void*)(g_xbox_mem_offset+SHOT),0,0x22c);
    MEM32(SHOT)=kind;MEM32(SHOT+0x10)=0x14;
    MEMF(SHOT+0x64)=1;MEMF(SHOT+0x88)=30;
    MEMF(SHOT+0x1a0)=100;
    random_state=12345;random_calls=smoke=sweeps=retired=0;
    g_tsfp_native_timing_active=native;
    call(initialize);
    unsigned previous=0;double ticks=0;float delta=60.f/fps*slow;
    unsigned frames=(unsigned)(fps*duration/slow);float minimum=INFINITY,maximum=0;
    for(unsigned i=0;i<frames;i++) {
        ticks+=60.0/fps*slow;unsigned whole=(unsigned)(ticks+1e-6)-previous;previous+=whole;
        MEMF(SHOT+0x98)+=delta/60;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
        call(update);
        assert(!retired && isfinite(MEMF(SHOT+0x88)));
        minimum=fminf(minimum,MEMF(SHOT+0x88));maximum=fmaxf(maximum,MEMF(SHOT+0x88));
        unsigned char paused[0x22c];memcpy(paused,(void*)(g_xbox_mem_offset+SHOT),sizeof paused);
        unsigned rng_before=random_calls,smoke_before=smoke,sweeps_before=sweeps;
        MEM32(0x4e7944)=0;MEMF(0x4e794c)=0;call(update);
        assert(!memcmp(paused,(void*)(g_xbox_mem_offset+SHOT),sizeof paused));
        assert(random_calls==rng_before && smoke==smoke_before && sweeps==sweeps_before);
    }
    Motion m={MEMF(SHOT+0x1c),MEMF(SHOT+0x20),MEMF(SHOT+0x24),MEMF(SHOT+0x88),minimum,maximum,random_calls,smoke};
    printf("ballistic kind=%u fps=%u native=%d slow=%.2f duration=%.2f x=%.6f y=%.6f z=%.6f speed=%.6f min=%.6f max=%.6f rng=%u smoke=%u sweeps=%u\n",
        kind,fps,native,slow,duration,m.x,m.y,m.z,m.speed,m.minimum,m.maximum,m.random,m.smoke,sweeps);
    return m;
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    void *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x737bd4)=SHOT;MEM32(0x4e8cec)=60;
    for(unsigned i=0;i<2048;i++)MEMF(0x7a38a0+i*4)=(float)sin(i*6.2831853071795864769/2048);
    const unsigned kinds[]={38,41},rates[]={60,120,144,180,240};
    const float durations[]={.5f,1,1.5f};
    for(unsigned k=0;k<2;k++)for(unsigned d=0;d<3;d++)for(unsigned slow=0;slow<2;slow++) {
        Motion reference=run(kinds[k],60,0,1,durations[d]);
        unsigned char original[0x22c];memcpy(original,(void*)(g_xbox_mem_offset+SHOT),sizeof original);
        for(unsigned f=0;f<5;f++) {
            Motion m=run(kinds[k],rates[f],1,slow?.25f:1,durations[d]);
            float error=hypotf(hypotf(m.x-reference.x,m.y-reference.y),m.z-reference.z);
            assert(error<.15f && m.minimum>=reference.minimum-.05f && m.maximum<=reference.maximum+.05f);
            assert(m.random==reference.random && m.smoke==reference.smoke);
            if(!slow && rates[f]==60)assert(!memcmp(original,(void*)(g_xbox_mem_offset+SHOT),sizeof original));
        }
    }
    free(memory);puts("PASS: original ballistic initialization and native trajectory");
    return 0;
}
