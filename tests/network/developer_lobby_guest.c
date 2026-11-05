#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pc_network.h"
static unsigned char memory[0x800000];
static uintptr_t g_xbox_mem_offset=(uintptr_t)memory;
#define WORD_AT(a) (*(uint32_t *)(memory+(a)))
static int guest_writable(uint32_t a,size_t n){return a>0 && a<sizeof memory && n<=sizeof memory-a;}
static unsigned queries;static char notice[1152];
int pc_network_enabled(void){return 1;}
int pc_developer_enabled(void){return 1;}
void pc_developer_lobby_notice(const char *names){snprintf(notice,sizeof notice,"%s",names);}
void sub_00353760(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,uint32_t slot)
{assert(fn==sub_00353760 && count==1 && slot<2);return 0x777248u+slot*0x124u;}
void pc_network_developer_lobby(const PcDeveloperPeer *roster,unsigned count,int enabled,char *names,size_t bytes)
{
    assert(count==2 && enabled && roster[0].local && !roster[1].local);
    assert(roster[0].host && !roster[1].host);
    assert(roster[0].ip==0x01020304 && roster[1].ip==0x05060708);
    assert(!strcmp(roster[0].name,"Alice") && !strcmp(roster[1].name,"Bob?"));
    snprintf(names,bytes,"Alice, Bob?");queries++;
}
#include "pc_developer_lobby_guest.inc"
int main(void)
{
    WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x774028)=0x2000;
    WORD_AT(0x77728c)=WORD_AT(0x77728c+0x124)=1;
    WORD_AT(0x777244)=0x2000;WORD_AT(0x777244+0x124)=0x2100;
    WORD_AT(0x2000)=5;WORD_AT(0x2100)=7;
    WORD_AT(0x66648c+5*16)=0x01020304;WORD_AT(0x66648c+7*16)=0x05060708;
    memcpy(memory+0x777248,"Alice",6);memcpy(memory+0x777248+0x124,"Bob\n",5);
    pc_developer_lobby_guest();assert(queries==1 && !strcmp(notice,"Alice, Bob?"));
    WORD_AT(0x76b108)=0;pc_developer_lobby_guest();assert(queries==1 && !notice[0]);
    puts("PASS: native lobby roster names, peer IP/local ownership and offline clearing");return 0;
}
