/* Original round-limit routine, controlled score providers and end callbacks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL: %s\n",#x);exit(1); } } while(0)
static int enabled=1,ended,notices;
static int pc_network_enabled(void) { return enabled; }
/* PRODUCTION_ADAPTER */
static void network_original(void) { g_eax=MEM32(0x76b108);g_esp+=4; }
void sub_003533F0(void) { pc_network_bots_guest_score_network(network_original); }
void sub_00355EC0(void) { g_eax=MEM32(0x76b140)==1;g_esp+=4; }
void sub_00355F10(void) { g_eax=0;g_esp+=4; }
void sub_000641C0(void) { g_eax=MEM32(0x10000+MEM32(g_esp+4)*0xea0+0xa6c);g_esp+=4; }
void sub_00064160(void) {
    unsigned team=MEM32(g_esp+4);g_eax=0;
    for(unsigned i=0;i<8;i++)if(MEM32(0x10000+i*0xea0+0x84)==team)
        g_eax+=MEM32(0x10000+i*0xea0+0xa6c);
    g_esp+=4;
}
void sub_00066E90(void) { ++notices;g_esp+=4; }
void sub_00066FC0(void) { ++ended;g_esp+=4; }
/* UNREACHED_STUBS */
/* ORIGINAL_ROUTINE */
int main(void) {
    void *mem=calloc(1,0x800000);CHECK(mem);g_xbox_mem_offset=(ptrdiff_t)mem;
    MEM32(0x7356d8)=0x10000;MEM8(0x7de400)=2;MEM8(0x7de402)=6;
    MEM32(0x790950)=2;MEM32(0x7a2958)=6;MEM32(0x4e8cec)=60;
    MEM32(0x76b108)=1;MEM8(0x7de455)=11;MEM32(0x7de464)=5;
    for(unsigned slot=0;slot<8;slot++) MEM32(0x10000+slot*0xea0+0x84)=slot<2?0:1;
    /* Human eligibility is still supplied by the original controller table. */
    MEM8(0x77728c)=2;MEM8(0x77728c+0x124)=2;
    for(unsigned host=0;host<2;host++)for(unsigned team=0;team<2;team++)
    for(unsigned winner=0;winner<8;winner++)for(unsigned score=4;score<=6;score++) {
        unsigned reached=score>=5;
        for(unsigned slot=0;slot<8;slot++) MEM32(0x10000+slot*0xea0+0xa6c)=0;
        MEM32(0x10000+winner*0xea0+0xa6c)=score;
        MEM32(0x76b140)=host?1:2;MEM32(0x7de458)=team;
        MEM32(0x7de45c)=0x7800; /* Already-announced score warnings. */
        MEM32(0x6f11a4)=0;ended=notices=0;
        g_esp=0x70000;g_ebx=11;g_esi=22;g_edi=33;g_seh_ebp=44;
        sub_000674C0();
        CHECK(g_esp==0x70004 && g_ebx==11 && g_esi==22 && g_edi==33);
        CHECK(ended==(int)reached && notices==(int)reached);
        CHECK(MEM32(0x6f11a4)==reached);
        CHECK(MEM8(0x77728c+2*0x124)==0); /* Never invent bot controllers. */
    }
    free(mem);puts("PASS: original score-limit decisions for humans, bots, teams and both peers");
    return 0;
}
