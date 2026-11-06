/* Exercise the original signed-save workflow with distinct peer profiles. */
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

#define SAVE_PROFILE_BYTES 0x1f54u
#define PROFILE_TABLE 0x7bedc0u
#define LOCAL_METADATA 0x7bc9c0u
#define REMOTE_METADATA 0x7bcac0u
static unsigned multiplayer_destination,multiplayer_profile_count;
static uint32_t multiplayer_profiles[16];
static int arcade=1;
static int multiplayer_arcade(void){return arcade;}
static unsigned dialogs,writes,lobbies,write_failure;
static uint8_t disk[SAVE_PROFILE_BYTES];
static uint32_t disk_word(unsigned offset){uint32_t value;memcpy(&value,disk+offset,4);return value;}
static void ret(uint32_t value){g_eax=value;g_esp+=4;}
void sub_003C9D90(void){g_esp-=g_eax;g_esp+=4;}
void sub_00025980(void){ret(0);}
void sub_00026320(void){ret(2);}
void sub_00025280(void){
    assert(MEM32(g_esp+20)==SAVE_PROFILE_BYTES);
    memcpy((void *)(g_xbox_mem_offset+MEM32(g_esp+16)),disk,SAVE_PROFILE_BYTES);
    ret(1);
}
void sub_000263E0(void){
    if(write_failure){ret(3);return;}
    assert(MEM32(g_esp+20)==SAVE_PROFILE_BYTES);
    memcpy(disk,(const void *)(g_xbox_mem_offset+MEM32(g_esp+16)),SAVE_PROFILE_BYTES);
    ++writes;ret(1);
}
void sub_00025BB0(void){ret(0x1234);}
void sub_00079B70(void){assert(MEM32(g_esp+8)==0x4e8d40);++dialogs;ret(0);}
void sub_00079860(void){assert(MEM32(g_esp+8)==0x4e99d4);++lobbies;ret(0);}
/* ORIGINAL_SAVE_PROTOTYPES */
/* ORIGINAL_SAVE_FUNCTIONS */
/* PRODUCTION_NETWORK_PROFILE_SAVE */
static uint32_t checksum(uint32_t profile){return guest_call(sub_001504F0,2,profile,SAVE_PROFILE_BYTES);}
static void setup(void){
    memset((void *)g_xbox_mem_offset,0,0x800000);g_esp=0x20000;
    /* ORIGINAL_CRC_TABLE */
    dialogs=writes=lobbies=write_failure=0;arcade=1;multiplayer_destination=0;
    MEM32(0x74e934)=1;MEM32(0x76b108)=1;MEM32(0x76b140)=2;
    MEM32(PROFILE_TABLE)=LOCAL_METADATA;
    network_profiles_capture(multiplayer_profiles,&multiplayer_profile_count);
    assert(multiplayer_profile_count==1);
    /* Two valid profiles with the same visible name but different contents.
       The remote identity must never validate or replace this local disk file. */
    memset(disk,0x11,sizeof disk);memset(disk,0,4);
    memcpy((void *)(g_xbox_mem_offset+PROFILE_TABLE),disk,SAVE_PROFILE_BYTES);
    MEM32(LOCAL_METADATA+0x28)=checksum(PROFILE_TABLE);
    MEM32(LOCAL_METADATA+0x24)=2;
    memcpy((void *)(g_xbox_mem_offset+PROFILE_TABLE+SAVE_PROFILE_BYTES),disk,SAVE_PROFILE_BYTES);
    MEM32(PROFILE_TABLE+SAVE_PROFILE_BYTES)=LOCAL_METADATA;
    MEM32(PROFILE_TABLE)=0;
    MEM32(PROFILE_TABLE+100)=0x99999999;
    MEM32(REMOTE_METADATA+0x28)=checksum(PROFILE_TABLE);
    MEM32(REMOTE_METADATA+0x24)=2;
    MEM32(PROFILE_TABLE)=REMOTE_METADATA;
    MEM32(0x6f1e0c)=PROFILE_TABLE+SAVE_PROFILE_BYTES+0x1290;
    /* Guest gameplay updates its own profile while keeping the on-disk CRC. */
    MEM32(PROFILE_TABLE+SAVE_PROFILE_BYTES+100)=0x12345678;
}
static void save(void){
    MEM32(0x78ad24)=0x10000;
    g_esp=0x20000;MEM32(g_esp)=0x12345678;
    MEM32(g_esp+4)=0x10000;MEM32(g_esp+8)=0x4e99d4;MEM32(g_esp+12)=UINT32_MAX;
    if(!multiplayer_profile_save(sub_001922D0))sub_001922D0();
    assert(g_esp==0x20004);
    assert(MEM32(PROFILE_TABLE)==REMOTE_METADATA);
    assert(MEM32(PROFILE_TABLE+SAVE_PROFILE_BYTES)==LOCAL_METADATA);
    assert(MEM32(0x6f1e0c)==PROFILE_TABLE+SAVE_PROFILE_BYTES+0x1290);
}
int main(void){
    g_xbox_mem_offset=(ptrdiff_t)calloc(1,0x800000);assert(g_xbox_mem_offset);
    setup();
#ifdef REPRODUCE_SHORT_SCOPE
    save();assert(dialogs==1 && writes==0 && MEM32(0x74f9d8)==7);
#else
    save();assert(!dialogs && writes==1 && lobbies==1);
    assert(disk_word(100)==0x12345678);
    assert(MEM32(PROFILE_TABLE+100)==0x99999999);
    /* A second dirty save checks the metadata CRC updated by the first save. */
    MEM32(PROFILE_TABLE+SAVE_PROFILE_BYTES+100)=0x87654321;
    save();assert(!dialogs && writes==2 && lobbies==2);
    assert(disk_word(100)==0x87654321);
    setup();disk[100]^=1;save();
    assert(dialogs==1 && !writes && !lobbies && MEM32(0x74f9d8)==7);
    setup();write_failure=1;save();
    assert(dialogs==1 && !writes && !lobbies && MEM32(0x74f9d8)==3);
#endif
    free((void *)g_xbox_mem_offset);
    puts("PASS: persistent save identity, repeated saves, genuine CRC and write errors");
    return 0;
}
