#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "pc_network.h"
static unsigned char memory[0x800000];
static uintptr_t g_xbox_mem_offset=(uintptr_t)memory;
#define WORD_AT(a) (*(uint32_t *)(memory+(a)))
static int guest_writable(uint32_t a,size_t n){return a>0 && a<sizeof memory && n<=sizeof memory-a;}
static uint32_t g_esp=0x7000,g_ebx,g_eax;
static unsigned queries,icon_rectangles,original_calls;static char notice[1152];
static uint32_t bits(float value){uint32_t word;memcpy(&word,&value,4);return word;}
uint32_t xbox_HeapAlloc(uint32_t n,uint32_t alignment){(void)n;(void)alignment;return 0x6000;}
void sub_0003FCF0(void){}
void sub_000814B0(void){}
int pc_network_enabled(void){return 1;}
int pc_developer_enabled(void){return 1;}
void pc_developer_lobby_notice(const char *names){snprintf(notice,sizeof notice,"%s",names);}
void sub_00353760(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t args[9];va_list ap;va_start(ap,count);
    assert(count<=9);for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);va_end(ap);
    if(fn==sub_00353760){assert(count==1&&args[0]<2);return 0x777248u+args[0]*0x124u;}
    if(fn==sub_0003FCF0){assert(count==4);WORD_AT(args[0])=bits(64.f);WORD_AT(args[1])=bits(14.f);return 0;}
    assert(fn==sub_000814B0&&count==9);assert(args[1]>=167&&args[2]>=50&&args[3]>0&&args[4]>0);icon_rectangles++;return 0;
}
unsigned pc_network_developer_lobby(const PcDeveloperPeer *roster,unsigned count,int enabled,char *names,size_t bytes)
{
    assert(count==2 && enabled && roster[0].local && !roster[1].local);
    assert(roster[0].host && !roster[1].host);
    assert(roster[0].ip==0x01020304 && roster[1].ip==0x05060708);
    assert(!strcmp(roster[0].name,"Alice") && !strcmp(roster[1].name,"Bob?"));
    snprintf(names,bytes,"Alice, Bob?");queries++;return 3;
}
#include "pc_developer_lobby_guest.inc"
static void original_name(void){g_eax=0x777248;g_esp+=4;}
static void original_text(void){original_calls++;g_esp+=4;}
int main(void)
{
    WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x774028)=0x2000;
    WORD_AT(0x77728c)=WORD_AT(0x77728c+0x124)=1;
    WORD_AT(0x777244)=0x2000;WORD_AT(0x777244+0x124)=0x2100;
    WORD_AT(0x2000)=5;WORD_AT(0x2100)=7;
    WORD_AT(0x66648c+5*16)=0x01020304;WORD_AT(0x66648c+7*16)=0x05060708;
    memcpy(memory+0x777248,"Alice",6);memcpy(memory+0x777248+0x124,"Bob\n",5);
    pc_developer_lobby_guest();assert(queries==1 && !strcmp(notice,"Alice, Bob?"));
    WORD_AT(g_esp)=0x198fd6;WORD_AT(g_esp+4)=0;
    pc_developer_lobby_name(original_name);assert(g_esp==0x7004&&g_eax==0x777248);
    g_esp=0x7000;WORD_AT(g_esp)=0x199583;WORD_AT(g_esp+4)=bits(100.f);WORD_AT(g_esp+8)=bits(50.f);WORD_AT(g_esp+12)=0x777248;WORD_AT(g_esp+16)=0xffffff7f;
    WORD_AT(0x76b1a4)=12;pc_developer_lobby_text(original_text);assert(g_esp==0x7004&&icon_rectangles>10&&original_calls==1);
    unsigned drawn=icon_rectangles;
    /* Display names can be decorated copies; measure the actual outlined label. */
    memcpy(memory+0x5000,"Alice (2)",10);g_esp=0x7000;WORD_AT(g_esp+12)=0x5000;
    pc_developer_lobby_text(original_text);assert(icon_rectangles>drawn);drawn=icon_rectangles;
    g_ebx=1;g_esp=0x7000;pc_developer_lobby_text(original_text);assert(icon_rectangles==drawn);
    g_ebx=0;assert(original_calls==3);
    developer_lobby_slots=0;g_esp=0x7000;
    pc_developer_lobby_text(original_text);assert(icon_rectangles==drawn&&original_calls==4);
    assert(!strcmp((char *)(memory+0x777248),"Alice"));
    WORD_AT(0x76b108)=0;pc_developer_lobby_guest();assert(queries==1 && !notice[0]);
    puts("PASS: native lobby roster names, peer IP/local ownership and offline clearing");return 0;
}
