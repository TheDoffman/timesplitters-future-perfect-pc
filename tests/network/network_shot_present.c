/* Execute the production presenter with isolated RAM and an effect-call spy. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifndef _WIN32
#define _WIN32 1
#endif
#include "pc_online.h"
#include "pc_network_bots_state.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t memory[0x800000/4];
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
static PcOnlineMode mode;
PcOnlineMode pc_online_mode(void){return mode;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static unsigned calls;
void sub_001BB5C0(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    va_list args;va_start(args,count);
    CHECK(fn==sub_001BB5C0 && count==4 && va_arg(args,uint32_t)==0x72000);
    CHECK(va_arg(args,uint32_t)==0 && va_arg(args,uint32_t)==0 && va_arg(args,uint32_t)==0);
    va_end(args);CHECK(WORD_AT(0x50000+2*0xea0+4)==0x74400);++calls;return 1;
}
#include "network_shot_present_generated.inc"
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x76b140)=2;WORD_AT(0x76b1a4)=15;WORD_AT(0x7356d8)=0x50000;
    const uint32_t actor=0x50000+2*0xea0,hand=actor+0xa4;
    WORD_AT(0x7207c)=actor;WORD_AT(0x50000+0xea0)=0x74400;WORD_AT(actor+4)=0x74000;
    PcNetworkBotShot shot={0};shot.serial=1;shot.target_slot=1;
    for(unsigned online=0;online<2;online++) {
        mode=online?PC_ONLINE_ARCADE:PC_ONLINE_STORY;
        const unsigned start=calls;
        const int builtins[]={41,56,61};
        for(unsigned i=0;i<3;i++) {
            shot.weapon=builtins[i];WORD_AT(hand+0x30)=builtins[i];WORD_AT(hand+0xe8)=0;
            pc_network_bots_present_shot(0x72000,&shot);
            CHECK(calls==start+i+1 && WORD_AT(actor+4)==0x74000 && !WORD_AT(hand+0xe8));
        }
        shot.weapon=7;WORD_AT(hand+0x30)=7;pc_network_bots_present_shot(0x72000,&shot);
        CHECK(calls==start+3); /* An ordinary gun still waits for its model. */
        WORD_AT(hand+0xe8)=0x180000;pc_network_bots_present_shot(0x72000,&shot);CHECK(calls==start+4);
        shot.weapon=61;pc_network_bots_present_shot(0x72000,&shot);CHECK(calls==start+4); /* stale weapon */
    }
    puts("PASS: built-in model-free shots, ordinary model readiness, ownership restoration and both Online modes");
    return 0;
}
