/* Preserve the original US Story cast preload and add online player assets. */
#include <assert.h>
#include <stdint.h>
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
recomp_func_t recomp_lookup(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t a){(void)a;assert(0);return NULL;}
void recomp_icall_fail_log(uint32_t a){(void)a;assert(0);}

static unsigned requested[128],enabled=1,mounts;
void sub_000622D0(void){}
void sub_000389B0(void){g_esp+=4;}
void sub_0006BC00(void){g_eax=2;g_esp+=4;}
void sub_0006BC50(void){unsigned model=MEM32(g_esp+4)?75:12;MEM32(MEM32(g_esp+8))=model;MEM32(MEM32(g_esp+12))=model;g_esp+=4;}
void sub_000D7780(void){unsigned kind=MEM32(g_esp+4);assert(kind<128 && MEM32(g_esp+8)==kind);requested[kind]++;g_eax=0xa5;g_esp+=4;}
/* ORIGINAL_ASSET_PRELOAD */
#define WORD_AT(a) MEM32(a)
typedef struct {uint32_t sp,ax;} GuestRegisters;
static void save_registers(GuestRegisters *s){s->sp=g_esp;s->ax=g_eax;}
static void restore_registers(const GuestRegisters *s){g_esp=s->sp;g_eax=s->ax;}
static int story_campaign_scene_active(void){return enabled && MEM32(0x7de458)==0x80008;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    va_list ap;va_start(ap,count);unsigned first=va_arg(ap,unsigned),second=va_arg(ap,unsigned);
    if(fn==sub_000622D0){assert(count==3 && first==0x478784 && !second && va_arg(ap,unsigned)==1);++mounts;va_end(ap);return 1;}
    va_end(ap);assert(count==2 && mounts);uint32_t frame=g_esp;g_esp-=12;
    MEM32(g_esp+4)=first;MEM32(g_esp+8)=second;fn();uint32_t value=g_eax;g_esp=frame;return value;
}
/* PRODUCTION_ASSET_ADAPTER */
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    const unsigned kinds[4]={12,75,104,103};MEM8(0x7de455)=10;MEM32(0x79094c)=2;
    for(unsigned i=0;i<4;i++)MEM8(0x7de414+i)=kinds[i];
    for(unsigned count=2;count<=4;count++) {
        MEM8(0x7de400)=count;MEM32(0x7de458)=0x80008;
        memset(requested,0,sizeof requested);g_esp=0x20000;sub_00158BC0_gen();
        assert(g_esp==0x20004 && requested[12] && requested[75] && !requested[104] && !requested[103]);
        memset(requested,0,sizeof requested);g_esp=0x20000;pc_story_network_player_assets(sub_00158BC0_gen);
        assert(g_esp==0x20004);
        assert(mounts==count-1);
        for(unsigned i=0;i<count;i++)assert(requested[kinds[i]]);
    }
    enabled=0;memset(requested,0,sizeof requested);g_esp=0x20000;pc_story_network_player_assets(sub_00158BC0_gen);
    assert(requested[12] && requested[75] && !requested[104] && !requested[103]);
    enabled=1;MEM32(0x7de458)=0x20000000;memset(requested,0,sizeof requested);g_esp=0x20000;
    pc_story_network_player_assets(sub_00158BC0_gen);assert(!requested[12] && !requested[103]);
    free(memory);puts("PASS: original Story cast preserved and configured online models preloaded; movie/local unchanged");return 0;
}
