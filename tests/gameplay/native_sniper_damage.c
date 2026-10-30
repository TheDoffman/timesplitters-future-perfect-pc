/* Original sniper initialization, update, impact setup and damage arithmetic.
 * Scene geometry, hit reactions, network policy and modifiers are captured.
 * Original actor health subtraction executes; this is not a live scene test. */
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
enum { SHOT=0x1000,TARGET=0x2000,ACTOR=0x4000,MODEL=0x6000,INFO=0x7000,BONES=0x8000,STACK=0x30000 };
static unsigned traces,damages,effects,retired;
static int hit;
static unsigned target_flags=0x10,accept_damage=1;
static float damage;
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected sniper tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected sniper callback")
/* Substitute one scene ray result. Original shot initialization, impact setup
 * damage calculation and actor health subtraction still execute. */
void sub_000DCB80(void) {
    assert(g_esi==SHOT);traces++;
    uint32_t ray=MEM32(g_esp+4),point=MEM32(g_esp+8),normal=MEM32(g_esp+12);
    assert(MEMF(ray)>100 && MEMF(ray+4)==0 && MEMF(ray+8)==0);
    MEMF(point)=hit?10:MEMF(ray);MEMF(point+4)=MEMF(point+8)=0;
    MEMF(normal)=-1;MEMF(normal+4)=MEMF(normal+8)=0;
    MEM32(SHOT+0x14c)=0;g_eax=hit?TARGET:0;g_esp+=4;
}
void sub_000BABA0(void) {
    uint32_t record=MEM32(g_esp+4);
    assert(MEM32(record)==TARGET && MEM32(record+0x24)==9);
    damage=MEMF(record+0x20);damages++;g_esp+=4;
}
void sub_000C6140(void) { g_eax=0;g_esp+=4; }
void sub_000B9340(void) { g_esp+=4; }
void sub_000B9430(void) { g_eax=0;g_esp+=4; }
void sub_000BB090(void) { g_esp+=4; } /* hit-reaction setup */
void sub_00350D40(void) { g_eax=0;g_esp+=4; } /* no damage modifiers */
void sub_0035C870(void) { g_eax=accept_damage;g_esp+=4; } /* offline/network acceptance boundary */
void sub_00238510(void) { g_esp+=4; } /* statistics */
void sub_000D6A80(void) { g_eax=0;g_esp+=4; }
void sub_000B93E0(void) { g_eax=1;g_esp+=4; }
void sub_00278040(void) { g_eax=1;g_esp+=4; }
void sub_000E3680(void) { g_eax=0;g_esp+=4; }
void sub_000DC550(void) { retired++;g_esp+=4; }
void sub_00104F30(void) { effects++;g_esp+=4; }
void sub_00153B40(void) { g_eax=1234;g_esp+=4; }
void sub_000DC5D0(void) { g_esp+=4; }
void sub_00046AB0(void) { g_esp+=4; }
void sub_000E9FE0(void) { g_esp+=4; }
void sub_000F4920(void) { g_esp+=4; }
void sub_000B9960(void) { assert(!"Unexpected sniper call 000B9960"); }
void sub_000DCEF0(void) { assert(!"Unexpected sniper call 000DCEF0"); }
void sub_000FC990(void) { assert(!"Unexpected sniper call 000FC990"); }
void sub_003520B0(void) { assert(!"Unexpected sniper call 003520B0"); }
void sub_003C8C64(void) { assert(!"Unexpected sniper call 003C8C64"); }
void sub_00064D10(void) { assert(!"Unexpected sniper call 00064D10"); }
void sub_000B9510(void) { assert(!"Unexpected sniper call 000B9510"); }
void sub_000BA230(void) { assert(!"Unexpected sniper call 000BA230"); }
void sub_000BA8A0(void) { g_esp+=4; } /* post-damage reaction */
void sub_000DC250(void) { assert(!"Unexpected sniper call 000DC250"); }
void sub_000DC270(void) { assert(!"Unexpected sniper call 000DC270"); }
void sub_00120220(void) { assert(!"Unexpected sniper call 00120220"); }
void sub_00160020(void) {
    uint32_t record=MEM32(g_esp+12);
    assert(MEM32(record)==TARGET && MEM32(record+0x24)==9);
    damage=MEMF(record+0x20);damages++;g_esp+=4;
}
void sub_00222340(void) { assert(!"Unexpected sniper call 00222340"); }
void sub_0025B350(void) { assert(!"Unexpected sniper call 0025B350"); }
/* GENERATED_ROUTINES */
static void call(void (*fn)(void)) {
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=SHOT;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xfeed;g_ebp=0xabcd;fn();
    if(g_esp!=STACK+4 || g_fp_top!=0) fprintf(stderr,"call %s esp=%x fp=%d\n",fn==sub_000E36D0?"init":"update",g_esp,g_fp_top);
    assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_edi==0xfeed && g_ebp==0xabcd);
}
static float run(int native,unsigned whole,float delta,int collide) {
    memset((void *)((uintptr_t)SHOT+g_xbox_mem_offset),0,0x22c);
    MEM32(SHOT)=24;MEM32(SHOT+8)=9;MEMF(SHOT+0x64)=1;
    MEM32(SHOT+0x100)=3;
    memset((void *)((uintptr_t)TARGET+g_xbox_mem_offset),0,0x300);
    memset((void *)((uintptr_t)ACTOR+g_xbox_mem_offset),0,0xd00);
    MEM32(TARGET+0x20)=8;MEM32(TARGET+0x28)=target_flags;MEMF(TARGET+0x1cc)=1;
    MEM32(TARGET+0x7c)=ACTOR;MEM32(TARGET+4)=MODEL;
    MEM32(MODEL+0x8c)=INFO;MEM32(INFO+0x5c)=BONES;
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;MEM32(0x4e8cec)=60;
    g_tsfp_native_timing_active=native;
    traces=damages=effects=retired=0;damage=0;hit=collide;
    call(sub_000E36D0);call(sub_000F5FF0);
    assert(traces==1 && effects==1 && retired==0);
    unsigned applied=hit && (native?delta>0:whole!=0) && (target_flags&0x10) && accept_damage;
    assert(damages==applied);
    assert(fabsf(MEMF(TARGET+0x1cc)-(applied?.35f:1.f))<.00001f);
    call(sub_000F5FF0);assert(traces==1 && damages==applied && effects==1 && retired==1);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    printf("sniper native=%d whole=%u delta=%.6f hit=%d damage=%.6f health=%.6f\n",native,whole,delta,hit,damage,MEMF(TARGET+0x1cc));
    return damage;
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);
    float reference=run(0,1,1,1);assert(reference>0);
    const float deltas[]={1,.5f,5.f/12,.25f,5.f/48,2,6};
    for(unsigned i=0;i<sizeof(deltas)/sizeof(deltas[0]);i++) {
        assert(run(1,(unsigned)deltas[i],deltas[i],1)==reference);
        assert(run(1,(unsigned)deltas[i],deltas[i],0)==0);
        if(deltas[i]<1) assert(run(1,1,deltas[i],1)==reference);
    }
    target_flags=0;assert(run(1,0,5.f/12,1)==0);target_flags=0x10;
    accept_damage=0;assert(run(1,0,5.f/12,1)==0);accept_damage=1;
    assert(run(0,0,5.f/12,1)==0); /* original gate loses this one-shot hit */
    assert(run(1,0,0,1)==0); /* pause remains immune to damage */
    puts("PASS: original sniper hit, damage calculation and actor health subtraction; pause and retirement preserved");
    free(memory);return 0;
}
