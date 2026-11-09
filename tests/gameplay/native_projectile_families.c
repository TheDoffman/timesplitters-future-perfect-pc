/* Original US projectile flight families. Scene, sound and particle
 * services are controlled; motion, gravity, drag and matrix helpers execute. */
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
void sub_000EDBE0(void){g_esp+=4;}
void sub_000E97A0(void){sweeps++;g_eax=0;g_esp+=4;}
void sub_000913D0(void){g_eax=0;g_esp+=4;}
void sub_000E9FE0(void){g_esp+=4;}
void sub_000F4920(void){g_esp+=4;}
void sub_000E94E0(void){g_esp+=4;}
/* Visibility query and particle allocator are scene services. */
void sub_001CDAE0(void){g_eax=1;g_esp+=4;}
void sub_000EFBD0(void){g_eax=0;g_esp+=4;}
void sub_000E33A0(void){g_esp+=4;}
void sub_000FFF70(void){g_eax=0;g_esp+=4;}
void sub_000FF420(void){g_esp+=4;}
void sub_000FF490(void){g_esp+=4;}
void sub_00153BB0(void){sub_00153B40();}
void sub_000F04B0(void){g_esp+=4;} /* Attached effect update. */
void sub_00045610(void){g_eax=0;g_esp+=4;} /* Sound handle query. */
void sub_00045460(void){g_esp+=4;}
void sub_0024A150(void){g_eax=0;g_esp+=4;} /* No scripted beam obstacle. */
void sub_00186480(void){g_esp+=4;} /* Ignition scene service. */
int pc_model_transform_entry(void){return 0;} /* Execute original matrix code. */
void sub_00101660(void){g_eax=0;g_esp+=4;} /* Particle allocation. */
void sub_0010E730(void){g_esp+=4;} /* Particle position service. */
void pc_native_projectile_smoke_choice(void){
    if(!pc_native_projectile_skip_smoke()){sub_00153BB0();return;}
    g_eax=1;g_esp+=4;
}
void sub_003C848C(void){g_eax=(int32_t)g_fp_stack[g_fp_top];g_fp_top=(g_fp_top+1)&7;g_esp+=4;}
void sub_003C8501(void){g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=floor(MEMD(g_esp+4));g_esp+=4;}
void sub_0003ED20(void){memset((void*)(g_xbox_mem_offset+0x5000),0,0x188);g_eax=0x5000;g_esp+=4;}
void sub_000873F0(void){
    memset((void*)(g_xbox_mem_offset+MEM32(g_esp+8)),0,12);
    memset((void*)(g_xbox_mem_offset+MEM32(g_esp+12)),0,12);g_esp+=4;
} /* Controlled empty scene. */
void sub_000C3E60(void){g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=0;g_esp+=4;}
/* UNREACHED_SERVICES */
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected projectile tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected projectile callback")
/* GENERATED_ROUTINES */
static void call(void (*f)(void)) {
    g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=SHOT;
    g_esi=0xabc;g_edi=0xdef;g_ebx=0x123;g_ebp=0x456;
    f();assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xabc && g_edi==0xdef && g_ebx==0x123 && g_ebp==0x456);
}
static void call_update(void){pc_native_kind35_update_guest(UPDATER,MEM32(g_esp+4));}
static void noop(void){}
typedef struct Flight {float position[3],direction[3],speed;unsigned random,smoke;} Flight;
static Flight run(unsigned kind,unsigned fps,int native,float slow) {
    memset((void*)(g_xbox_mem_offset+SHOT),0,0x22c);
    MEM32(SHOT+0x15c)=UINT32_MAX;MEM32(SHOT)=kind;MEM32(SHOT+0x10)=0x14;
    MEMF(SHOT+0x64)=.6f;MEMF(SHOT+0x68)=.2f;MEMF(SHOT+0x6c)=sqrtf(.6f);
    MEMF(SHOT+0x70)=MEMF(SHOT+0x7c)=1;
    MEMF(SHOT+0x88)=30;MEMF(SHOT+0x9c)=100;MEMF(SHOT+0x17c)=100;
    memset((void*)(g_xbox_mem_offset+0x2000),0,0x400);
    MEM32(0x2000+0x28)=0x100;
    MEM32(SHOT+0xa8)=(kind==32 || kind==33 || (kind>=12 && kind<=15))?0x2000:0;MEM32(0x2000+0x7c)=0x4000;
    random_state=12345;random_calls=smoke=sweeps=retired=0;
    g_tsfp_native_timing_active=native;
    pc_native_kind35_init_guest(noop,SHOT);
    unsigned previous=0;double ticks=0;float delta=60.f/fps*slow;
    for(unsigned i=0;i<(unsigned)(fps*.5f/slow);i++) {
        ticks+=60.0/fps*slow;unsigned whole=(unsigned)(ticks+1e-6)-previous;previous+=whole;
        if(kind!=17)MEMF(SHOT+0x98)+=delta/60; /* Kind17 stays before its short fuse. */MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
        call(call_update);
        assert(!retired && isfinite(MEMF(SHOT+0x1c)));
        if(native && kind!=43 && kind!=52 && kind!=53) {
        unsigned before_random=random_calls,before_smoke=smoke,before_sweeps=sweeps;
        unsigned char paused[0x22c];memcpy(paused,(void*)(g_xbox_mem_offset+SHOT),sizeof paused);
        MEM32(0x4e7944)=0;MEMF(0x4e794c)=0;call(call_update);
        assert(!memcmp(paused,(void*)(g_xbox_mem_offset+SHOT),sizeof paused));
        assert(random_calls==before_random && smoke==before_smoke && sweeps==before_sweeps);
        }
    }
    float x=MEMF(SHOT+0x1c);
    printf("family kind=%u fps=%u native=%d x=%.6f speed=%.6f rng=%u smoke=%u sweeps=%u\n",
        kind,fps,native,x,MEMF(SHOT+0x88),random_calls,smoke,sweeps);
    Flight f={0};for(unsigned j=0;j<3;j++){
        f.position[j]=MEMF(SHOT+0x1c+j*4);f.direction[j]=MEMF(SHOT+0x64+j*4);
    }
    f.speed=MEMF(SHOT+0x88);f.random=random_calls;f.smoke=smoke;return f;
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x737bd4)=SHOT;MEM32(0x4e8cec)=60;
    for(unsigned i=0;i<2048;i++)MEMF(0x7a38a0+i*4)=(float)sin(i*6.2831853071795864769/2048);
    const unsigned kinds[]={KINDS},rates[]={180,60,120,144,240};
    for(unsigned k=0;k<sizeof(kinds)/sizeof(kinds[0]);k++) {
        Flight reference=run(kinds[k],60,0,1);
        unsigned char original[0x22c];memcpy(original,(void*)(g_xbox_mem_offset+SHOT),sizeof original);
        if(kinds[k]!=25 && kinds[k]!=26)assert(hypotf(hypotf(reference.position[0],reference.position[1]),reference.position[2])>1);
        for(unsigned f=0;f<5;f++)for(unsigned slow=0;slow<2;slow++) {
            Flight result=run(kinds[k],rates[f],1,slow?.25f:1);
            float distance=0;
            for(unsigned j=0;j<3;j++){
                float error=result.position[j]-reference.position[j];distance+=error*error;
            }
            assert(sqrtf(distance)<(kinds[k]==44?1.01f:.6f));
            assert(fabsf(result.speed-reference.speed)<.01f);
            if(kinds[k]!=44 && kinds[k]!=49)assert(result.random==reference.random && result.smoke==reference.smoke);
            if(!slow && rates[f]==60)assert(!memcmp(original,(void*)(g_xbox_mem_offset+SHOT),sizeof original));
        }
    }
    free(memory);puts("PASS: original projectile families");return 0;
}
