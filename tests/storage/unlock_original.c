/* Actual US eligibility routines with only profile selection supplied by the
 * fixture. Tables are loaded from the verified local XBE, never checked in. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "pc_unlock_content.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_esp,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint16_t g_fp_cc,g_fp_control_word;
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t address){(void)address;abort();}
recomp_func_t recomp_lookup_manual(uint32_t address){(void)address;abort();}
recomp_func_t recomp_lookup_kernel(uint32_t address){(void)address;abort();}
void recomp_icall_fail_log(uint32_t address){(void)address;abort();}
static unsigned selected;
#define WORD_AT(a) MEM32(a)
#define PC_DISPLAY_MENU_CALLBACK 0x12008u
static int guest_writable(uint32_t a,size_t n){return a && a<0x900000 && n<=0x900000-a;}
/* GUEST_CALL_HELPERS */
void sub_00356870(void){g_eax=selected;g_esp+=4;}
/* DECLARATIONS */
/* UNLOCK_HOOKS */
void sub_001CD3E0(void){pc_unlock_requirement(sub_001CD3E0_gen);}
void sub_001CDA70(void){pc_unlock_event(sub_001CDA70_gen);}
void sub_001CD560(void){pc_unlock_movie(sub_001CD560_gen);}
/* ORIGINAL_ROUTINES */
static uint32_t profile(void){return 0x7bedc0+selected*PC_UNLOCK_PROFILE_SIZE;}
static void select_profile(unsigned index)
{
    selected=index;MEM32(0x6f1e0c)=profile()+0x1290;
}
static void toggle(unsigned category,int enabled)
{assert(pc_unlock_set((void *)(g_xbox_mem_offset+profile()),category,enabled));}
static unsigned queries;
static uint32_t query(void (*fn)(void),uint32_t a,uint32_t b,uint32_t c)
{
    g_esp=0x800000;MEM32(g_esp)=0xfeedface;MEM32(g_esp+4)=a;MEM32(g_esp+8)=b;MEM32(g_esp+12)=c;
    g_ebx=0x1234;g_esi=0x2345;g_edi=0x3456;g_ebp=0x4567;g_seh_ebp=g_ebp;
    fn();assert(g_esp==0x800004 && g_ebx==0x1234 && g_esi==0x2345 && g_edi==0x3456);
    queries++;return g_eax;
}
static void table(unsigned category,void (*fn)(void),unsigned count,uint32_t base,unsigned stride)
{
    for(unsigned i=0;i<count;i++) {
        uint32_t baseline=query(fn,i,0,0);
        toggle(category,1);
        uint32_t on=query(fn,i,0,0);
        assert(on==(MEM32(base+i*stride)?1u:baseline));
        select_profile(1);assert(query(fn,i,0,0)==baseline);select_profile(0);
        toggle(category,0);assert(query(fn,i,0,0)==baseline);
    }
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x900000);assert(g_xbox_mem_offset);
    /* LOAD_XBE */
    MEM32(0x7bedc0)=0x1000;MEM32(0x7bedc0+PC_UNLOCK_PROFILE_SIZE)=0x2000;
    select_profile(0);
    unsigned char before[0xc4c];memcpy(before,(void *)(g_xbox_mem_offset+profile()+0x1290),sizeof before);
    table(PC_UNLOCK_MAPS,sub_001CD9F0,16,0x4d23ac,0x3c);
    table(PC_UNLOCK_WEAPONS,sub_001CD9D0,70,0x4fa144,0x3c);
    table(PC_UNLOCK_MODES,sub_001CD900,20,0x4d21f8,20);
    table(PC_UNLOCK_CHEATS,sub_001CD940,13,0x4fcc88,0x1c);
    table(PC_UNLOCK_CHEATS,sub_001CD920,5,0x4fce08,20);
    assert(query(sub_001CD120,0,0,0)==0);
    toggle(PC_UNLOCK_CHEATS,1);assert(query(sub_001CD120,0,0,0)==10);toggle(PC_UNLOCK_CHEATS,0);
    for(unsigned i=0;i<150;i++) {
        uint32_t p=0x521a18+i*0x30,id=MEM32(p+4),variant=(uint32_t)(int32_t)(int8_t)MEM8(p+1);
        uint32_t baseline=query(sub_001CD980,id,variant,0);
        toggle(PC_UNLOCK_CHARACTERS,1);
        assert(query(sub_001CD980,id,variant,0)==(MEM32(p+0x14)?1u:baseline));
        toggle(PC_UNLOCK_CHARACTERS,0);assert(query(sub_001CD980,id,variant,0)==baseline);
    }
    for(unsigned level=0;level<13;level++)for(unsigned difficulty=0;difficulty<3;difficulty++)for(unsigned coop=0;coop<2;coop++) {
        uint32_t baseline=query(sub_001CD890,level,difficulty,coop);
        toggle(PC_UNLOCK_STORY,1);assert(query(sub_001CD890,level,difficulty,coop)==1);
        toggle(PC_UNLOCK_STORY,0);assert(query(sub_001CD890,level,difficulty,coop)==baseline);
    }
    for(unsigned group=0;group<16;group++) {
        unsigned category=group<7?PC_UNLOCK_CHALLENGES:PC_UNLOCK_LEAGUE;
        uint32_t baseline=query(sub_001CDA20,group,0,0);
        toggle(category,1);assert(query(sub_001CDA20,group,0,0)==1);
        toggle(category,0);assert(query(sub_001CDA20,group,0,0)==baseline);
    }
    for(unsigned event=0;event<48;event++) {
        unsigned category=event<21?PC_UNLOCK_CHALLENGES:PC_UNLOCK_LEAGUE,group=event/3;
        uint32_t baseline=query(sub_001CDA70,event,group,0);
        toggle(category,1);assert(query(sub_001CDA70,event,group,0)==1);
        toggle(category,0);assert(query(sub_001CDA70,event,group,0)==baseline);
    }
    /* Fresh real progress retains every byte after all the queries/toggles. */
    assert(!memcmp(before,(void *)(g_xbox_mem_offset+profile()+0x1290),sizeof before));
    /* Complete a story level during early access, then disable it. Only the
     * real completed level remains eligible through the untouched predicate. */
    toggle(PC_UNLOCK_STORY,1);MEM32(profile()+0x1290+0xc00)=1;
    toggle(PC_UNLOCK_STORY,0);assert(query(sub_001CD890,1,0,0)==1);
    assert(query(sub_001CD890,2,0,0)==0);
    printf("PASS: %u original US access/ABI checks, 150 characters, 48 events, profile isolation and earned progress\n",queries);
    free((void *)(uintptr_t)g_xbox_mem_offset);
}
