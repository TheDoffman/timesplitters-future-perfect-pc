/* Execute the stock minimum query, button updater and Start callback. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);exit(1);}}while(0)
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t address){(void)address;CHECK(0);return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t address){(void)address;CHECK(0);return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t address){(void)address;CHECK(0);return NULL;}
void recomp_icall_fail_log(uint32_t address){(void)address;CHECK(0);}
/* HOST */
#define BOT_LOBBY_WORD(a) MEM32(a)
static int enabled=1;
static unsigned story_limit,disabled,starts,refreshed;
static int pc_network_enabled(void){return enabled;}
static unsigned pc_story_network_player_limit(void){return story_limit;}
/* PRODUCTION_ADAPTER */
void sub_00064EA0_gen(void);
void sub_00064EA0(void){pc_network_bots_guest_minimum(sub_00064EA0_gen);}
void sub_00199CE0(void){refreshed++;g_esp+=4;}
void sub_00074910(void){disabled=MEM32(g_esp+12);g_esp+=4;}
void sub_001AF950(void){CHECK(MEM32(g_esp+8)==0x93);starts++;g_esp+=4;}
void sub_00079B70(void){starts++;g_esp+=4;}
/* ORIGINAL_ROUTINES */
int main(void)
{
    unsigned char *mem=calloc(1,0x800000);CHECK(mem);g_xbox_mem_offset=(ptrdiff_t)mem;
    /* ORIGINAL_CONSTANTS */
    for(unsigned type=0;type<21;type++) {
        MEM32(0x70000)=0x123456;MEM32(0x70004)=type;g_esp=0x70000;
        sub_00064EA0_gen();unsigned stock_minimum=g_eax;
        CHECK(g_esp==0x70004 && stock_minimum>=1 && stock_minimum<=3);
        for(unsigned mode=0;mode<=14;mode++)
        for(unsigned session=0;session<2;session++)
        for(unsigned network=0;network<2;network++)
        for(unsigned story=0;story<2;story++)
        for(unsigned players=0;players<=3;players++) {
            MEM32(0x78ad2c)=mode;MEM32(0x76b108)=session;enabled=network;story_limit=story?4:0;
            MEM8(0x78adae)=type;MEM32(0x774168)=players;
            unsigned minimum=mode>=11 && mode<=13 && session && network && !story?1:stock_minimum;
            g_esp=0x70000;g_ebx=11;g_esi=22;g_edi=33;g_ebp=44;
            disabled=99;refreshed=0;sub_00199EB0();
            CHECK(disabled==(players<minimum) && refreshed==1 && g_esp==0x70004);
            CHECK(g_ebx==11 && g_esi==22 && g_edi==33 && g_ebp==44);
            /* The ordinary launch routes retain their map-specific dispatch. */
            for(unsigned custom=0;custom<2;custom++) {
                MEM8(0x78adac)=custom?100:0;starts=0;g_esp=0x70000;
                sub_001CC2D0();CHECK(starts==(players>=minimum) && g_esp==0x70004);
            }
            CHECK(MEM32(0x774168)==players && MEM8(0x78adae)==type);
        }
    }
    free(mem);puts("PASS: original Arcade Start button/callback admit one player in every mode; empty/offline/Story guards retained");
    return 0;
}
