/* Execute original guest-owned Story awards and reward pages on isolated RAM. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t address){(void)address;assert(!"unexpected dispatch");return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t address){(void)address;assert(!"unexpected dispatch");return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t address){(void)address;assert(!"unexpected dispatch");return NULL;}
void recomp_icall_fail_log(uint32_t address){(void)address;assert(!"unexpected dispatch");}

#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
typedef struct GuestRegisters {uint32_t sp,ax,bx,cx,dx,si,di,bp;} GuestRegisters;
static void save_registers(GuestRegisters *r){*r=(GuestRegisters){g_esp,g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_ebp};}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->ax;g_ebx=r->bx;g_ecx=r->cx;g_edx=r->dx;g_esi=r->si;g_edi=r->di;g_ebp=r->bp;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...){
    const uint32_t frame=g_esp;va_list ap;va_start(ap,count);uint32_t args[10];
    assert(count<=10);for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);va_end(ap);
    for(unsigned i=count;i;i--){g_esp-=4;MEM32(g_esp)=args[i-1];}
    g_esp-=4;MEM32(g_esp)=0x12345678;fn();uint32_t result=g_eax;g_esp=frame;return result;
}
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
static int story_mode=1,story_rewards_tracking;
static unsigned story_lobby_mission,story_pending_stage=1;
static int story_network_session(void){return story_mode;}
static uint32_t xbox_HeapAlloc(uint32_t n,uint32_t align){assert(n==128 && align==4);return 0x100000;}
#define OWN_PROFILE (0x7c0050u+0x1f54u)
static unsigned panels,rows,refreshes,exits,page_awards;
static uint32_t confirm;
static void ret(uint32_t x){g_eax=x;g_esp+=4;}
void sub_003533F0(void){ret(MEM32(0x76b108)!=0);}
void sub_00239820(void){g_esp+=4;}
void sub_00064D10(void){ret(MEM8(0x7de455)==10);}
void sub_00069930(void){ret(0x130000);}
void sub_001CD040(void){ret(0);}
void sub_001CD170(void){g_esp+=4;}
void sub_00238670(void){assert(MEM32(g_esp+8)==1);g_esp+=4;}
void sub_001CD9F0(void){ret(0);}
void sub_001CD980(void){ret(0);}
void sub_001CD9D0(void){ret(0);}
void sub_001CD2A0(void){ret(0);}
void sub_001CDA20(void){ret(0);}
void sub_001CD940(void){ret(MEM32(g_esp+4)==0 && (MEM32(OWN_PROFILE+0xc00)&1));}
void sub_002CB300(void){
    for(unsigned i=0;i<page_awards;i++){MEM32(0x78aec0+4*i)=100+i;MEM32(0x78ae80+4*i)=200+i;}
    ret(page_awards);
}
void sub_002CB360(void){ret(0);}
void sub_002CB3C0(void){ret(0);}
void sub_002CB430(void){ret(0);}
void sub_002CB4F0(void){ret(0);}
void sub_0003D7B0(void){ret(0x110000);}
void sub_00075B70(void){++panels;confirm=MEM32(g_esp+16);g_esp+=4;}
void sub_00071C50(void){g_esp+=4;}
void sub_000795B0(void){g_esp+=4;}
void sub_00079D60(void){++rows;g_esp+=4;}
void sub_00079D60_gen(void){sub_00079D60();}
void sub_000771D0(void){g_esp+=4;}
void sub_000770E0(void){g_esp+=4;}
void sub_00070F50(void){g_esp+=4;}
void sub_00078980(void){
    /* Original refresh requires the callback's menu argument, including the
       final wrapped page and the original automatic timer tail call. */
    assert(MEM32(g_esp+4)==0x10000);++refreshes;g_esp+=4;
}
void sub_00079B70(void){assert(MEM32(g_esp+8)==0x5236dc);++exits;g_esp+=4;}
/* ORIGINAL_PROTOTYPES */
/* ORIGINAL_REWARD_FUNCTIONS */
static int story_online_return_menu(void (*original)(void)){(void)original;return 0;}
/* PRODUCTION_REWARD_ADAPTERS */
void sub_002CBAF0(void){pc_story_network_rewards_page(sub_002CBAF0_gen);}
static unsigned save_calls,save_owned;
static void original_save(void){
    ++save_calls;
    if(save_owned) {
        assert(MEM32(0x6f1e0c)==0x7bedc0+0x1290);
        assert(guest_call(sub_000696F0,1,0u)==0x7bedc0);
        assert(MEM32(0x7bedc0)==0x7bcc40 && MEM32(0x7bedc0+0x1290+0x918)==1);
        assert(guest_call(sub_00069790,1,0u)==0x12345678);
        MEM32(0x7bedc0+0x1290+0x918)=7;
        guest_call(sub_000697D0,3,0u,9u,0xabcdefu);
    }
    g_esp+=4;
}
static void build(void){g_esp=0x20000;MEM32(g_esp+4)=0x10000;story_rewards_build();assert(g_esp==0x20004);}
static void timer(void){MEM32(0x7613f4)=MEM32(0x4760cc);g_esp=0x20000;story_rewards_tick();assert(g_esp==0x20004);}
static uint32_t expected_descriptor;
static void original_rewards_menu(void){
    assert(MEM32(g_esp+4)==0x10000 && MEM32(g_esp+8)==expected_descriptor);
    assert(MEM32(g_esp+12)==0xffffffff);
    if(expected_descriptor==0x523900) {
        assert(story_rewards_count()==0); /* Never fabricate an unlock. */
        guest_call(story_rewards_build,1,0x10000u);
    }
    g_esp+=4;
}
static void empty_menu(uint32_t descriptor,uint32_t expected){
    g_esp=0x20000;MEM32(g_esp+4)=0x10000;MEM32(g_esp+8)=descriptor;
    MEM32(g_esp+12)=0xffffffff;expected_descriptor=expected;
    pc_story_network_rewards_menu(original_rewards_menu);
    assert(g_esp==0x20004 && MEM32(0x20008)==descriptor);
}
int main(void){
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_REWARD_CONSTANTS */
    MEM32(0x76b108)=1;MEM32(0x76b140)=2;MEM32(0x76b1a4)=15;
    MEM32(0x79094c)=2;MEM32(0x790950)=2;MEM32(0x7de458)=0x80008;MEM8(0x7de455)=10;
    MEM32(0x7b0c48)=0x40000;MEM32(0x7356d8)=0x50000;MEM32(0x5236c4)=1;
    MEM32(0x40004)=0xffffffff;MEM32(0x40000+0x1584+4)=0;
    MEM32(0x50000)=0x60000;MEM32(0x50000+0xea0)=0x61000;
    MEM32(0x77728c)=1;MEM32(0x77728c+0x124)=1;
    MEM32(0x774028)=0x30000;MEM32(0x3000c)=0x31000;MEM32(0x3103c)=1;
    MEM32(0x7bedc0+0x1f54)=1;MEM32(0x7bedc0)=0;
    MEM32(0x7c0050+0xc00)=0xabcdef00;MEM32(0x7c0050+0x918)=0x12345678;
    MEM32(0x4d1ac6)=2;MEM32(0x4e8cec)=60;float delta=1;memcpy(memory+0x4e7948,&delta,4);
    /* Identical unlocked profiles must follow the host's Online reward policy.
       Original2D2C70 skips2CB210 for BOTH network roles. A guest-only snapshot
       hides eligible rewards when this completion bit is already set. */
    MEM32(OWN_PROFILE+0xc00)=1;
    g_esp=0x20000;story_rewards_begin();assert(story_rewards_tracking && g_esp==0x20000);
    assert(MEM32(0x6f1e0c)==OWN_PROFILE && MEM32(0x75fcc0)==0);
    assert(story_rewards_count()==1);
    MEM32(0x7de45c)=0x2000000;story_rewards_record(0,2);
    assert(g_esp==0x20000 && MEM32(0x7de45c)==0x2000000 && MEM32(0x7de30c)==0);
    assert((MEM32(OWN_PROFILE+0xc00)&1) && MEM32(OWN_PROFILE+0x918)==1);
    assert(MEM32(OWN_PROFILE+0x910)==1 && MEM32(OWN_PROFILE+0x914)==1);
    assert(MEM32(0x78ad24)&0x10000); /* original dirty-profile path retained */
    assert(MEM32(0x7c0050+0xc00)==0xabcdef00 && MEM32(0x7c0050+0x918)==0x12345678);
    story_pending_stage=2;story_rewards_record(0,2);assert(MEM32(OWN_PROFILE+0x918)==1);
    story_pending_stage=3;MEM32(0x79094c)=106;MEM32(0x7de458)=0;
    assert(story_rewards_count()==1);build();assert(!story_rewards_waiting && confirm==0x2cbaf0 && rows==2);
    timer();assert(MEM32(0x75fd88)==3 && refreshes==1 && !exits);
    build();assert(story_rewards_waiting && !confirm);
    timer();g_esp=0x20000;story_rewards_page();assert(g_esp==0x20004 && !exits && refreshes==1);
    /* Multiple original three-row pages; both A and the original timer's
       direct tail call must end at waiting rather than5236DC. */
    page_awards=7;MEM32(0x75fd88)=0;build();assert(!story_rewards_waiting);
    g_esp=0x20000;story_rewards_page();assert(g_esp==0x20004 && MEM32(0x75fd88)==3 && !exits);
    build();timer();assert(MEM32(0x75fd88)==6 && !exits);
    build();timer();assert(MEM32(0x75fd88)==9 && !exits);
    build();assert(story_rewards_waiting && !confirm);timer();assert(!exits);
    /* Already-earned rewards don't become new again; original host/offline
       exhausted-page transitions remain available. */
    page_awards=0;g_esp=0x20000;sub_002CB210();assert(MEM32(0x75fcc0)==1);
    /* Actual scene entry selects loading5236DC for count0, bypassing build().
       Route that factory request to the reward descriptor, then wait with
       no A/B action or automatic advance. Keep other factory uses original. */
    story_rewards_waiting=0;
    empty_menu(0x5236dc,0x523900);assert(story_rewards_waiting && !confirm && !exits);
    timer();assert(!exits);
    empty_menu(0x5238e0,0x5238e0);
    MEM32(0x79094c)=101;empty_menu(0x5236dc,0x5236dc);
    MEM32(0x79094c)=106;MEM32(0x76b140)=1;empty_menu(0x5236dc,0x5236dc);
    MEM32(0x76b140)=2;story_mode=0;empty_menu(0x5236dc,0x5236dc);story_mode=1;
    page_awards=1;empty_menu(0x5236dc,0x5236dc);page_awards=0;
    story_pending_stage=2;empty_menu(0x5236dc,0x5236dc);story_pending_stage=3;
    MEM32(0x76b140)=1;g_esp=0x20000;story_rewards_page();assert(exits==1 && g_esp==0x20004);
    MEM32(0x76b140)=2;story_mode=0;g_esp=0x20000;story_rewards_page();assert(exits==2 && g_esp==0x20004);
    /* Original frontend save accessors enumerate persistent profile0, while
       a guest's real metadata/body moved to network slot1. The scratch body
       left in slot0 has a different cached checksum and must not be saved. */
    story_mode=1;MEM32(0x76b140)=2;MEM32(0x74e934)=1;
    MEM32(0x79094c)=101;MEM32(0x76b1a4)=0;MEM32(0x7bedc0)=0x7bcc40;
    story_rewards_profiles_capture();assert(story_rewards_profile_count==1);
    MEM32(0x76b1a4)=12;MEM32(0x7bedc0)=0x7bcac0;
    story_rewards_profiles_capture();assert(story_rewards_profiles[0]==0x7bcc40);
    MEM32(0x76b1a4)=15;MEM32(0x79094c)=106;
    MEM32(0x7bedc0)=0x7bcac0;MEM32(0x7bedc0+0x1f54)=0x7bcc40;
    MEM32(0x7bcc40+0x28)=0x12345678;MEM32(0x7bcc40+0x3c)=0x7bedc0;
    MEM32(0x7bcac0+0x28)=0;
    assert(guest_call(sub_00069790,1,0u)==0); /* reproduces wrong checksum view */
    const uint32_t remote_stat=MEM32(0x7bedc0+0x1290+0x918);
    save_owned=1;g_esp=0x20000;pc_story_network_profile_save(original_save);
    assert(g_esp==0x20004 && save_calls==1 && MEM32(0x7bedc0)==0x7bcac0);
    assert(MEM32(0x6f1e0c)==OWN_PROFILE);
    assert(MEM32(0x7bedc0+0x1f54)==0x7bcc40 && MEM32(OWN_PROFILE+0x918)==7);
    assert(MEM32(0x7bedc0+0x1290+0x918)==remote_stat && MEM32(0x7bcc40+0x28)==0xabcdef);
    save_owned=0;MEM32(0x76b140)=1;g_esp=0x20000;
    pc_story_network_profile_save(original_save);assert(save_calls==2 && g_esp==0x20004);
    puts("PASS: original guest-owned co-op awards, profile isolation, reward pages, timer tail call and waiting handoff");
    free(memory);return 0;
}
