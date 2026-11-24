/* Compare co-op human classification with the original US targeting path. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
/* Execute the pinned US classifier and the reticle's real hostile predicate. */
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
static int online_mode=1;
static int story_network_session(void){return online_mode;}
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
void sub_000D6F00(void);
void sub_000DA220(void){g_eax=1;g_esp+=4;}
void sub_0009E6F0(void){g_eax=1;g_esp+=4;}
/* ORIGINAL_FRIENDLY */
/* ORIGINAL_RETICLE */
/* PRODUCTION_SCOPE */
/* PRODUCTION_FRIENDLY */
static int corrected;
void sub_000D6F00(void)
{
    if(corrected)pc_story_network_friendly(sub_000D6F00_gen);
    else sub_000D6F00_gen();
}
static uint32_t aimed(unsigned viewer,unsigned target)
{
    g_esp=0x1800;MEM32(g_esp)=0x12345678;
    MEM32(g_esp+4)=0x6000+viewer*0x1584;MEM32(g_esp+8)=0x74000+target*0x400;
    sub_000D00F0_gen();assert(g_esp==0x1804 && MEM32(0x1800)==0x12345678);return g_eax;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    MEM8(0x7de455)=10;MEM32(0x7de458)=0x80008;MEM32(0x79094c)=2;
    MEM32(0x7b0c48)=0x6000;MEM32(0x7356d8)=0x50000;
    MEM32(0x76b108)=1;MEM32(0x76b140)=1;MEM32(0x76b1a4)=15;
    MEM32(0x475cac)=0; /* zero-health cutoff */
    const unsigned kinds[5]={12,75,104,103,104};
    for(unsigned slot=0;slot<5;slot++) {
        const uint32_t actor=0x50000+slot*0xea0,context=0x6000+slot*0x1584,body=0x74000+slot*0x400;
        MEM32(actor)=body;MEM32(actor+8)=slot;MEM32(actor+0x84)=1;MEM16(actor+0x48)=kinds[slot];
        MEM32(context)=slot;MEM32(context+0x14)=body;
        MEM32(body+0x7c)=actor;MEM32(body+0x150)=slot<4?context:0;
        MEM32(body+0x20)=8;MEM32(body+0x1cc)=0x3f800000;
    }
    MEM32(0x53c508+12*20)=0x8000;MEM32(0x53c508+75*20)=0x8000;
    MEM32(0x53c508+104*20)=4;MEM32(0x53c508+103*20)=5;
    MEM32(0x790950)=4;MEM8(0x7de400)=4;
    corrected=0;
    for(unsigned viewer=0;viewer<2;viewer++) {
        assert(aimed(viewer,1)==0); /* original sidekick is friendly */
        assert(aimed(viewer,2)==1 && aimed(viewer,3)==1); /* reproduced red reticle */
    }
    corrected=1;
    const unsigned scenes[13]={2,4,18,11,1,17,6,22,5,14,9,26,81};
    for(unsigned mission=0;mission<13;mission++)for(unsigned count=2;count<=4;count++)for(unsigned role=1;role<=2;role++) {
        MEM32(0x79094c)=scenes[mission];
        MEM32(0x790950)=count;MEM8(0x7de400)=(uint8_t)count;MEM32(0x76b140)=role;
        for(unsigned viewer=0;viewer<count;viewer++)for(unsigned target=0;target<count;target++)
            assert(aimed(viewer,target)==0);
    }
    MEM32(0x790950)=4;MEM8(0x7de400)=4;
    assert(aimed(0,4)==1); /* NPC using exactly the same Private Hicks definition */
    MEM32(0x74000+4*0x400+0x150)=0x6000;assert(aimed(0,4)==1); /* NPC cannot borrow a player context */
    MEM32(0x74000+4*0x400+0x150)=0;
    MEM16(0x50000+4*0xea0+0x48)=75;assert(aimed(0,4)==0); /* authored friendly NPC */
    MEM16(0x50000+4*0xea0+0x48)=104;
    MEM32(0x74000+2*0x400+0x150)=0;assert(aimed(0,2)==1); /* incomplete human */
    MEM32(0x74000+2*0x400+0x150)=0x6000+2*0x1584;
    online_mode=0;assert(aimed(0,2)==1);online_mode=1;
    MEM8(0x7de455)=1;MEM32(0x7de458)=1;
    assert(aimed(0,2)==0);MEM32(0x50000+2*0xea0+0x84)=2;
    assert(aimed(0,2)==1); /* Arcade team rules remain native */
    assert(MEM32(0x53c508+104*20)==4 && MEM32(0x53c508+103*20)==5);
    free(memory);puts("PASS: original red-reticle reproduction; co-op human pairs friendly; NPCs and Arcade unchanged");return 0;
}
