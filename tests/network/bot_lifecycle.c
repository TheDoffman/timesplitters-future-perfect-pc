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
/* HOST */
#define WORD_AT(a) MEM32(a)
#define POOL 0x10000u
#define ACTOR (POOL+2u*0xea0u)
#define BODY 0x30000u
#define HAND (ACTOR+0xa4u)
#define GUN 0x40000u
#define STACK 0x70000u
static unsigned char ram[0x800000];
static int guest_writable(uint32_t a,size_t n){return a>=0x10000u && a<sizeof ram && n<=sizeof ram-a;}
enum {PC_ONLINE_ARCADE=1};
static unsigned mode=PC_ONLINE_ARCADE,removed,attachments,visited,constructed,posed;
static int fail_construct,replace_on_remove;
static unsigned pc_online_mode(void){return mode;}
typedef struct {uint32_t a,c,d,sp,b,si,di,bp;} Saved;
static uint32_t guest_call(void (*fn)(void),unsigned count,...){
    Saved s={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp};
    uint32_t args[10];va_list ap;va_start(ap,count);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);va_end(ap);
    for(unsigned i=count;i;i--){g_esp-=4;MEM32(g_esp)=args[i-1];}
    g_esp-=4;MEM32(g_esp)=0x12345;fn();uint32_t result=g_eax;
    assert(g_esp==s.sp-count*4u);
    g_eax=s.a;g_ecx=s.c;g_edx=s.d;g_esp=s.sp;g_ebx=s.b;g_esi=s.si;g_edi=s.di;g_ebp=s.bp;
    return result;
}
void sub_0009A060(void){uint32_t b=MEM32(g_esp+4);assert(b && MEM32(b+0x20)==8);visited++;g_esp+=4;}
/* ORIGINAL_ROSTER */
void sub_000A9070(void){
    uint32_t body=MEM32(g_esp+4);assert(body==GUN);attachments++;
    MEM32(body+0x28)|=0x80000000u;g_esp+=4;
}
void sub_000DAC30(void){
    assert(MEM32(g_esp+12)==2);constructed++;
    if(fail_construct)g_eax=0;
    else {MEM32(ACTOR)=BODY;MEM32(BODY+0x20)=8;MEM32(BODY+0x28)=1;MEM32(BODY+0x7c)=ACTOR;g_eax=BODY;}
    g_esp+=4;
}
void sub_000D7570(void){assert(MEM32(g_esp+4)==ACTOR);g_esp+=4;}
void sub_001BE870(void){assert(MEM32(g_esp+4)==BODY);posed++;g_esp+=4;}
void sub_000A0860(void){assert(0);}
void sub_000A2B50(void){assert(0);}
/* PRODUCTION */
static void destroy(void){
    assert(MEM32(g_esp+4)==BODY);removed++;
    MEM32(BODY+0x28)|=0x80000000u;
    if(replace_on_remove)MEM32(ACTOR)=BODY+0x1000;
    g_eax=0xabcdef;g_esp+=4;
}
static void setup(void){
    memset(ram,0,sizeof ram);g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=STACK;
    MEM32(0x7356d8)=POOL;MEM32(0x76b108)=1;MEM32(0x76b140)=2;MEM32(0x76b1a4)=15;
    ram[0x7de400]=2;ram[0x7de402]=1;
    MEM32(ACTOR)=BODY;MEM32(BODY+0x20)=8;MEM32(BODY+0x28)=1;MEM32(BODY+0x7c)=ACTOR;
    MEM32(ACTOR+0xa6c)=19;MEM32(HAND+0x30)=7;MEM32(HAND+0xec)=ACTOR;MEM32(HAND+0xe8)=GUN;
    MEM32(GUN+0x20)=0x20;MEM32(GUN+0x7c)=HAND;MEM32(GUN+0x28)=1;
    MEM32(0x7a2d00)=BODY;MEM32(0x74c37c)=1;MEM32(0x7a3580)=ACTOR;
    MEM32(STACK)=0x12345;MEM32(STACK+4)=BODY;
}
int main(void){
    setup();pc_network_bots_remove_body(destroy);
    assert(g_esp==STACK+4 && g_eax==0xabcdef && removed==1);
    assert(!MEM32(ACTOR) && !MEM32(0x74c37c) && !MEM32(0x7a2d00));
    assert(attachments==1 && !MEM32(HAND+0xe8) && MEM32(HAND+0x30)==7 && MEM32(ACTOR+0xa6c)==19);
    /* This is the exact stock traversal from Saturday's null-body crash. */
    guest_call(sub_000D8C00,0);assert(!visited);
    fail_construct=1;assert(!pc_network_bots_revive(2));
    assert(!MEM32(0x74c37c));guest_call(sub_000D8C00,0);assert(!visited);
    fail_construct=0;assert(pc_network_bots_revive(2)==BODY && constructed==2);
    assert(MEM32(0x74c37c)==1 && MEM32(0x7a3580)==ACTOR && MEM32(0x7a2d00)==BODY);
    assert(MEM32(ACTOR+0xa6c)==19);pc_network_bots_register_body(ACTOR,BODY);assert(MEM32(0x74c37c)==1);
    guest_call(sub_000D8C00,0);assert(visited==1);
    /* Existing score rows and foreign/reused objects never belong to cleanup. */
    setup();MEM32(GUN+0x7c)=HAND+4;g_esp=STACK;pc_network_bots_remove_body(destroy);
    assert(MEM32(HAND+0xe8)==GUN && attachments==1);
    setup();MEM32(BODY+0x7c)=ACTOR+4;pc_network_bots_remove_body(destroy);assert(MEM32(ACTOR)==BODY && MEM32(0x74c37c)==1);
    setup();MEM32(BODY+0x20)=0x20;pc_network_bots_remove_body(destroy);assert(MEM32(ACTOR)==BODY && MEM32(0x74c37c)==1);
    setup();MEM32(0x76b140)=1;pc_network_bots_remove_body(destroy);assert(MEM32(ACTOR)==BODY && MEM32(0x74c37c)==1);
    setup();mode=2;pc_network_bots_remove_body(destroy);assert(MEM32(ACTOR)==BODY && MEM32(0x74c37c)==1);mode=1;
    setup();replace_on_remove=1;pc_network_bots_remove_body(destroy);assert(MEM32(ACTOR)==BODY+0x1000);replace_on_remove=0;
    /* Weapon pose must validate before either native hand routine executes. */
    setup();float health=100;memcpy(ram+BODY+0x1cc,&health,4);
    MEM32(ACTOR)=0;pc_network_bots_pose_weapons(BODY);assert(!posed);
    MEM32(ACTOR)=BODY;MEM32(BODY+0x20)=0x20;pc_network_bots_pose_weapons(BODY);assert(!posed);
    MEM32(BODY+0x20)=8;MEM32(BODY+0x28)|=0x80000000u;pc_network_bots_pose_weapons(BODY);assert(!posed);
    MEM32(BODY+0x28)=1;pc_network_bots_pose_weapons(BODY);assert(posed==2);
    puts("PASS: original roster traversal survives replica retirement, failed reconstruction and revival; owned weapons retire before reuse");return 0;
}
