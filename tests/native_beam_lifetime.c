/* Execute the original effect manager with its original type/switch tables.
 * Type-32 visual geometry and unrelated subsystem services are replaced by
 * spies. This checks scheduling/age/expiry, not rendered appearance. */
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

enum { FX=0x1000,STACK=0x30000,HEAD=0x73bb88 };
static unsigned updates,retired;
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected effect tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected effect callback")
void sub_001053D0(void) { assert(MEM32(g_esp+4)==FX);updates++;g_esp+=4; }
void sub_000FF200(void) {
    assert(MEM32(g_esp+4)==FX && MEM32(HEAD)==FX);
    retired++;MEM32(HEAD)=0;g_esp+=4;
}
void sub_000C6140(void) { g_eax=0;g_esp+=4; }
void sub_00111E40(void) { g_esp+=4; }
void sub_0012B5D0(void) { g_esp+=4; }
void sub_000389B0(void) { g_esp+=4; }
void sub_00344A90(void) { g_esp+=4; }
void sub_001881E0(void) { g_esp+=4; }
void sub_00153BD0(void) { assert(!"Unexpected effect type: 00153BD0"); }
void sub_000FFE50(void) { assert(!"Unexpected effect type: 000FFE50"); }
void sub_001067E0(void) { assert(!"Unexpected effect type: 001067E0"); }
void sub_001064B0(void) { assert(!"Unexpected effect type: 001064B0"); }
void sub_00106AD0(void) { assert(!"Unexpected effect type: 00106AD0"); }
void sub_00106BD0(void) { assert(!"Unexpected effect type: 00106BD0"); }
void sub_001070E0(void) { assert(!"Unexpected effect type: 001070E0"); }
void sub_00107240(void) { assert(!"Unexpected effect type: 00107240"); }
void sub_00107500(void) { assert(!"Unexpected effect type: 00107500"); }
void sub_00101700(void) { assert(!"Unexpected effect type: 00101700"); }
void sub_00102650(void) { assert(!"Unexpected effect type: 00102650"); }
void sub_00105750(void) { assert(!"Unexpected effect type: 00105750"); }
void sub_00108780(void) { assert(!"Unexpected effect type: 00108780"); }
void sub_001078A0(void) { assert(!"Unexpected effect type: 001078A0"); }
/* GENERATED_ROUTINES */
static void tick(float delta,unsigned whole) {
    MEMF(0x4e794c)=delta;MEM32(0x4e7944)=whole;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xfeed;g_ebp=0xabcd;
    sub_0010A760();
    assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_edi==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void run(int fps,float speed,int native,float delay) {
    memset((void *)((uintptr_t)FX+g_xbox_mem_offset),0,0x100);
    MEM32(HEAD)=FX;MEM32(FX)=32;
    MEMF(FX+0x10)=MEMF(0x478674); /* original constructor lifetime */
    MEMF(FX+0x38)=delay;
    MEM32(0x4e8cec)=60;g_tsfp_native_timing_active=native;
    updates=retired=0;
    double elapsed=0,carry=0;float step=60.f/fps*speed;
    unsigned frames=0,first=0;
    while(!retired && frames<fps*20) {
        float before=MEMF(FX+0xc);unsigned n=updates;
        tick(0,0);assert(MEMF(FX+0xc)==before && !retired);
        /* The manager may visit the type updater at pause; time stays frozen. */
        updates=n;
        carry+=step;unsigned whole=(unsigned)carry;carry-=whole;
        tick(step,whole);frames++;elapsed+=(double)step/60;
        if(updates && !first)first=frames;
    }
    float lifetime=MEMF(FX+0x10),dt=step/60;
    assert(retired==1);
    assert(fabs(elapsed-(lifetime+delay))<=2*dt+.00001);
    if(delay==0)assert(first==1);
    else assert(first && fabs(first*dt-delay)<=dt+.00001);
    printf("beam native=%d fps=%d speed=%.2f delay=%.3f first=%.6f expiry=%.6f updates=%u\n",
           native,fps,speed,delay,first*dt,elapsed,updates);
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    assert(MEM32(0x4e11bc+32*16)!=0);
    setvbuf(stdout,NULL,_IONBF,0);
    run(60,1,0,0);
    const int rates[]={60,120,144,180,240};
    for(unsigned i=0;i<sizeof rates/sizeof rates[0];i++)for(unsigned slow=0;slow<2;slow++) {
        run(rates[i],slow?.25f:1,1,0);
        run(rates[i],slow?.25f:1,1,.3f);
    }
    puts("PASS: original beam manager first update, lifetime, deferred start and pause at 60/120/144/180/240 FPS");
    free(memory);return 0;
}
