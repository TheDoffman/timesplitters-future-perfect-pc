#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
static unsigned char ram[0x800000];
static ptrdiff_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)(ram+(uint32_t)(a)))
static int guest_writable(uint32_t a,size_t n){return a && a<sizeof ram && n<=sizeof ram-a;}
static unsigned mode=1,removed;
enum {PC_ONLINE_ARCADE=1};
static unsigned pc_online_mode(void){return mode;}
static void sub_000A9070(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,uint32_t entity)
{assert(fn==sub_000A9070 && count==1);assert(!(WORD_AT(entity+0x28)&0x80000000u));WORD_AT(entity+0x28)|=0x80000000u;removed++;return 0;}
/* PRODUCTION */
#define POOL 0x10000u
static uint32_t hand(unsigned slot,unsigned index){return POOL+slot*0xea0+0xa4+index*0x130;}
static void attach(unsigned slot,unsigned index,uint32_t gun)
{
 uint32_t record=hand(slot,index),actor=POOL+slot*0xea0;
 WORD_AT(record+0x30)=7;WORD_AT(record+0x34)=19;
 WORD_AT(record+0xe8)=gun;WORD_AT(record+0xec)=actor;
 WORD_AT(gun+0x20)=0x20;WORD_AT(gun+0x7c)=record;WORD_AT(gun+0x28)=1;
}
int main(void)
{
 (void)guest_call;(void)guest_writable;(void)pc_online_mode;
 g_xbox_mem_offset=(ptrdiff_t)ram;
 WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=15;WORD_AT(0x7356d8)=POOL;
 ram[0x7de400]=2;ram[0x7de402]=2;
 attach(0,0,0x30000); /* Human attachment must survive. */
 attach(2,0,0x31000);attach(2,1,0x32000);
 attach(3,0,0x33000);WORD_AT(0x33028)|=0x80000000u; /* Already queued. */
 attach(3,1,0x34000);WORD_AT(0x3407c)=0x1234; /* Unrelated world object. */
 WORD_AT(POOL+2*0xea0+0xa6c)=123;
 mode=2;pc_network_bots_promote();assert(removed==0);
 mode=1;WORD_AT(0x76b140)=2;pc_network_bots_promote();assert(removed==0);
 WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=11;pc_network_bots_promote();assert(removed==0);
 WORD_AT(0x76b1a4)=15;pc_network_bots_promote();
#ifdef EXPECT_FALLBACK
 assert(removed==0 && WORD_AT(hand(2,0)+0xe8)==0x31000);
#else
 assert(removed==2 && !WORD_AT(hand(2,0)+0xe8) && !WORD_AT(hand(2,1)+0xe8));
 assert(!WORD_AT(hand(3,0)+0xe8));
 assert(WORD_AT(hand(0,0)+0xe8)==0x30000 && WORD_AT(hand(3,1)+0xe8)==0x34000);
 assert(WORD_AT(0x30028)==1 && WORD_AT(0x34028)==1);
 assert(WORD_AT(hand(2,0)+0x30)==7 && WORD_AT(hand(2,0)+0x34)==19);
 assert(WORD_AT(POOL+2*0xea0+0xa6c)==123);
 pc_network_bots_promote();assert(removed==2); /* Idempotent; no double deletion. */
#endif
 puts("PASS: promotion retires only owned bot attachments, preserves humans, world objects, inventory and score");return 0;
}
