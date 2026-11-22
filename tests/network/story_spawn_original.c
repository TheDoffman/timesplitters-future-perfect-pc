/* Compare Online starts with the actual US Splitscreen selector. */
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
recomp_func_t recomp_lookup(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t a){(void)a;assert(0);return NULL;}
void recomp_icall_fail_log(uint32_t a){(void)a;assert(0);}
void sub_00230510(void){assert(0);}
void sub_00153B40(void){assert(0);}
void sub_0006C600(void){assert(0);}
void sub_003533F0(void){g_eax=1;g_esp+=4;}
void sub_0006A850(void){assert(MEM32(g_esp+4)==2);g_eax=0x40000;g_esp+=4;}
void sub_0006B2B0(void){g_eax=1;g_esp+=4;}
void sub_0009CC40(void){g_eax=MEM32(g_esp+4);g_esp+=4;}
static unsigned checkpoint;
void sub_0025EE60(void){g_eax=checkpoint;g_esp+=4;}
void sub_0025EC30(void){g_eax=9000u+MEM32(g_esp+4);g_esp+=4;}
/* ORIGINAL_SPAWN_SELECTOR */
#define WORD_AT(a) MEM32(a)
static int enabled=1;
static int story_campaign_gameplay(void){return enabled;}
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
static uint32_t story_extra_pad_storage;
static int story_rejoin_pose_valid;
static uint32_t xbox_HeapAlloc(uint32_t n,uint32_t alignment){(void)n;(void)alignment;assert(0);return 0;}
/* PRODUCTION_SPAWN_ADAPTER */
static uint32_t select_pad(unsigned slot,int online)
{
    const uint32_t context=0x6000+slot*0x1584;
    g_esp=0x1800;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=context;
    if(online)pc_story_network_spawn_pad(sub_000CF060_gen);
    else sub_000CF060_gen();
    assert(g_esp==0x1804 && MEM32(0x1800)==0x12345678);
    return g_eax;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    MEM8(0x7de455)=10;MEM32(0x7de458)=0x80008;MEM32(0x79094c)=2;
    MEM32(0x7b0c48)=0x6000;MEM32(0x40024)=0x41000;MEM32(0x40028)=2;
    MEM32(0x41000)=1;MEM32(0x41004)=8000;
    MEM32(0x41020)=1;MEM32(0x41024)=6000;
    for(unsigned slot=0;slot<4;slot++) {
        const uint32_t context=0x6000+slot*0x1584;
        MEM32(context)=slot;MEM32(context+4)=0x55;
        MEM32(context+0x14)=0x20000+slot*0x400;
        MEM32(0x2007c+slot*0x400)=0x30000+slot*0x400;
        MEM32(context+0x1504)=0x7fffffff;
    }
    for(checkpoint=0;checkpoint<=1;checkpoint++) {
        MEM32(0x6b988c)=0;
        for(unsigned slot=0;slot<4;slot++)MEM32(0x7504+slot*0x1584)=0x7fffffff;
        const uint32_t p1=select_pad(0,0),p2=select_pad(1,0);
        assert(p1==(checkpoint?9000u:8000u) && p2==(checkpoint?9001u:6000u));
        for(unsigned count=2;count<=4;count++) for(unsigned slot=0;slot<count;slot++) {
            MEM32(0x790950)=count;MEM32(0x6b988c)=1;
            const uint32_t context=0x6000+slot*0x1584;
            MEM32(context+0x1504)=123;
            assert(select_pad(slot,1)==(slot?p2:p1));
            assert(MEM32(context)==slot && MEM32(context+4)==0x55 &&
                   MEM32(context+0x1504)==123 && MEM32(0x6b988c)==1);
        }
    }
    enabled=0;checkpoint=0;MEM32(0x6b988c)=0;MEM32(0x7504)=0x7fffffff;
    assert(select_pad(0,1)==8000); /* Non-Story path remains original. */
    free(memory);puts("PASS: Online slots match original Splitscreen starts and checkpoints; ownership retained");return 0;
}
