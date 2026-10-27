/* Exercise the production menu wrapper using disposable guest memory. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
#define FP_MEM32(a) MEM32(a)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(1); } } while(0)
static unsigned draws,calls;
static uint32_t observed;
void sub_002E5750_gen(void);
/* PRODUCTION_WRAPPER */
#ifdef ORIGINAL_MENU
void sub_000774B0(void) { g_eax=0x4000; g_esp+=4; }
void sub_000746E0(void) { g_eax=0; g_esp+=4; }
void sub_00074BC0(void) { g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=0;g_esp+=4; }
void sub_00055D80(void) { g_esp+=4; }
void sub_00056080(void) { g_esp+=4; }
void sub_0001A010(void) { g_esp+=4; }
void sub_002E4670(void) { g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=0;g_esp+=4; }
void sub_0013AB60(void) {
    ++draws;
    CHECK(MEM32(g_esp+4)==0x5000); /* Original health/shield data. */
    CHECK(MEMF(g_esp+16)>0); /* Visible scale. */
    CHECK(MEM32(g_esp+20)==127 && MEM32(g_esp+32)==127);
    g_esp+=4;
}
void sub_002E5480(void) { ++calls;observed=MEM32(0x3000);g_esp+=4; }
void sub_00019650(void) { g_esp+=4; }
/* ORIGINAL_ROUTINE */
#else
void sub_002E5750_gen(void) {
    ++calls;observed=MEM32(0x3000);
    if (!(observed&0x100)) MEM32(0x3000)&=~0x80u;
    MEM32(0x3000)|=0x400; /* Unrelated changes by the draw must survive. */
    g_eax=0x12345678;g_esp+=4;
}
#endif
static void run(unsigned flags,unsigned mode,unsigned index,unsigned expected) {
    MEM32(0x3000)=flags;MEM32(0x7DE454)=mode<<8;
    MEM32(0x2004)=index;g_esp=0x10000;MEM32(g_esp)=0xDEADBEEF;
    calls=draws=observed=0;g_fp_top=0;
    g_ebx=11;g_esi=22;g_edi=33;g_ebp=g_seh_ebp=44;
    sub_002E5750();
    CHECK(calls==1 && g_esp==0x10004);
    CHECK(observed==expected);
    CHECK(g_ebx==11 && g_esi==22 && g_edi==33 && g_ebp==44);
#ifdef ORIGINAL_MENU
    CHECK(draws==(unsigned)((expected&0x101)==1 && MEM32(0x761C64) && !(MEM32(0x4040)&0x80000)));
    CHECK(g_fp_top==0);
    CHECK(MEM32(0x3000)==((flags&0x100)?flags:flags&~0x80u));
#else
    CHECK(MEM32(0x3000)==(((flags&0x100)?flags:flags&~0x80u)|0x400));
    CHECK(g_eax==0x12345678);
#endif
}
int main(void) {
    void *mem=calloc(1,0x800000);CHECK(mem);g_xbox_mem_offset=(ptrdiff_t)mem;
    MEM32(0x7B0C7C)=0x2000;MEM32(0x761C48)=0x3000;
    MEM32(0x2008)=0x6000;MEM32(0x621C)=1;MEM32(0x2014)=0x7000;MEM32(0x7268)=0x5000;
    MEM32(0x761C64)=1;MEM32(0x7E71C8)=0x8000;MEMF(0x4210)=1;MEMF(0x4C0460)=1;
    /* ORIGINAL_CONSTANTS */
    run(0x191,10,0,0x91); /* Reproduced Tab screen. */
    run(0x111,10,0,0x11); /* Quick view without the transient flag. */
    run(0x81,10,0,0x81); /* Full pause view retains its own behavior. */
    run(0x100,10,0,0x100); /* Inactive menu. */
    run(0x191,1,0,0x191); /* Other game modes. */
#ifndef ORIGINAL_MENU
    run(0x191,10,4,0x191);run(0x191,10,0xFFFFFFFF,0x191);
    MEM32(0x7B0C7C)=0;run(0x191,10,0,0x191);MEM32(0x7B0C7C)=0x2000;
#else
    MEM32(0x761C64)=0;run(0x191,10,0,0x91);MEM32(0x761C64)=1;
    MEM32(0x4040)=0x80000;run(0x191,10,0,0x91);MEM32(0x4040)=0;
    MEM32(0x761C54)=0x3000;run(0x191,10,3,0x91); /* Fourth local viewport. */
#endif
    free(mem);puts("PASS: Objectives health/shield visibility and menu state");return 0;
}
