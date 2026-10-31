#include "guest_io_build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"build line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned char memory[3202],before[3202];
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void rejection(tsfp_io_build_plan *plan,uint32_t address,uint32_t bytes,unsigned stacks,size_t span) {
    memset(memory,0xa7,sizeof(memory));memcpy(before,memory,sizeof(memory));
    CHECK(!tsfp_io_build_packet(memory+1,span,address,bytes,stacks,plan));
    CHECK(!memcmp(memory,before,sizeof(memory)));
}
int main(void) {
    static const unsigned char slot[24]={2,0x11,0,1,0x80,0,0,0,0x10,0,0,0,
        0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0,0x20,0x34,0x12};
    tsfp_io_build_plan plan={0},saved;uint32_t next;unsigned i;
    unsigned char expected[152]={0};
    plan.transfer.major=2;plan.transfer.minor=0x11;plan.transfer.control=1;
    plan.transfer.length=0x80;plan.transfer.buffer_word=0x10;
    plan.transfer.byte_offset=UINT64_C(0x1122334455667788);plan.transfer.device=0x12342000;
    plan.flags=0x14;plan.user_buffer=0x4000;plan.user_iosb=0x5000;plan.user_event=0x6000;plan.thread=0x7000;memcpy(&saved,&plan,sizeof(plan));
    expected[0]=6;expected[2]=152;expected[4]=0x14;expected[0x18]=2;expected[0x19]=3;
    expected[0x1d]=0x50;expected[0x21]=0x60;expected[0x31]=0x40;expected[0x51]=0x70;
    expected[0x5c]=0x98;expected[0x5d]=0x10;memcpy(expected+128,slot,24);
    memset(memory,0xa7,sizeof(memory));
    CHECK(tsfp_io_build_packet(memory+1,152,0x1000,152,2,&plan));
    CHECK(!memcmp(memory+1,expected,sizeof(expected)) && memory[0]==0xa7 && memory[153]==0xa7);
    CHECK(!memcmp(&plan,&saved,sizeof(plan)));
    CHECK(tsfp_io_request_advance(memory+1,152,0x1000,&next) && next==0x1080);
    CHECK(memory[1+0x19]==2 && word(memory+1+0x5c)==0x1080 && !memcmp(memory+1+128,slot,24));
    for(i=1;i<=TSFP_IO_MAX_STACKS;++i) {
        unsigned bytes=104+24*i,offset=104+24*(i-1);memset(memory,0xa7,sizeof(memory));
        CHECK(tsfp_io_build_packet(memory+1,bytes,0x1000,bytes,i,&plan));
        CHECK(!memcmp(memory+1+offset,slot,24) && memory[bytes+1]==0xa7);
        CHECK(word(memory+1+0x5c)==0x1000+bytes && memory[1+0x19]==i+1);
        CHECK(tsfp_io_request_peek(memory+1,bytes,0x1000,&next) && next==0x1000+offset);
    }
    rejection(&plan,0x1000,152,0,152);rejection(&plan,0x1000,152,128,152);
    rejection(&plan,0,152,2,152);rejection(&plan,0xffffff80,152,2,152);
    rejection(&plan,0x1000,151,2,152);rejection(&plan,0x1000,152,2,151);
    rejection(&plan,0x1000,65536,2,3200);rejection(NULL,0x1000,152,2,152);
    plan.transfer.major=4;rejection(&plan,0x1000,152,2,152);plan=saved;
    plan.transfer.device=0;rejection(&plan,0x1000,152,2,152);plan=saved;
    plan.transfer.device=0xfffffff0;rejection(&plan,0x1000,152,2,152);plan=saved;
    plan.user_iosb=0xfffffffc;rejection(&plan,0x1000,152,2,152);plan=saved;
    plan.user_event=0xfffffff8;rejection(&plan,0x1000,152,2,152);plan=saved;
    plan.user_buffer=0xfffffff0;rejection(&plan,0x1000,152,2,152);plan=saved;
    plan.transfer.flags=0x80;plan.transfer.buffer_word=0xfffffff0;rejection(&plan,0x1000,152,2,152);plan=saved;
    plan.transfer.flags=0x80;plan.transfer.buffer_word=0x8000;plan.transfer.major=3;plan.flags=0x24;
    plan.user_event=plan.user_iosb=plan.thread=0;
    CHECK(tsfp_io_build_packet(memory+1,152,0x1000,152,2,&plan));
    CHECK(memory[129]==3 && word(memory+1+4)==0x24 && memory[131]==0x80);
    CHECK(word(memory+1+0x1c)==0 && word(memory+1+0x20)==0 && word(memory+1+0x50)==0);
    CHECK(word(memory+1+128+8)==0x8000);
    puts("PASS: original transfer packet construction, exact literal IRP/slot bytes, all127 stack counts and transactional failures");return 0;
}
