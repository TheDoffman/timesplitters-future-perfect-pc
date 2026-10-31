#include "guest_io_target.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"target line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char packet[258],device[74],driver[70],old_packet[258],old_device[74],old_driver[70];
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
static int prepare(tsfp_io_target *out) {return tsfp_io_target_prepare(packet+1,256,0x2000,device+1,72,0x4000,driver+1,68,0x6000,out);}
static void save(void) {memcpy(old_packet,packet,sizeof(packet));memcpy(old_device,device,sizeof(device));memcpy(old_driver,driver,sizeof(driver));}
static void unchanged(void) {CHECK(!memcmp(old_packet,packet,sizeof(packet)) && !memcmp(old_device,device,sizeof(device)) && !memcmp(old_driver,driver,sizeof(driver)));}
int main(void) {
    unsigned major;uint32_t stack=0;tsfp_io_target target,before;
    memset(packet,0xa7,sizeof(packet));memset(device,0xb8,sizeof(device));memset(driver,0xc9,sizeof(driver));
    CHECK(tsfp_io_request_initialize(packet+1,256,0x2000,256,2,6));put(device+1+8,0x6000);
    for(major=0;major<14;++major)put(driver+1+12+4*major,0x10000000+16*major);
    for(major=0;major<14;++major) {
        packet[1+104+24]=(unsigned char)major;save();
        CHECK(prepare(&target) && target.major==major && target.routine==0x10000000+16*major);
        CHECK(target.device==0x4000 && target.request==0x2000 && target.stack==0x2080);unchanged();
        CHECK(tsfp_io_request_peek(packet+1,256,0x2000,&stack) && stack==0x2080);unchanged();
    }
    CHECK(tsfp_io_request_advance(packet+1,256,0x2000,&stack) && stack==0x2080);
    packet[1+104]=7;save();CHECK(prepare(&target) && target.major==7 && target.stack==0x2068);unchanged();
    memset(&target,0xdd,sizeof(target));before=target;packet[1+104]=14;save();CHECK(!prepare(&target));CHECK(!memcmp(&target,&before,sizeof(target)));unchanged();
    packet[1+104]=7;put(driver+1+12+4*7,0);save();CHECK(!prepare(&target));CHECK(!memcmp(&target,&before,sizeof(target)));unchanged();put(driver+1+12+4*7,0x10000070);
    CHECK(!tsfp_io_target_prepare(packet+1,256,0x2000,device+1,71,0x4000,driver+1,68,0x6000,&target));
    CHECK(!tsfp_io_target_prepare(packet+1,256,0x2000,device+1,72,0x4000,driver+1,67,0x6000,&target));
    CHECK(!tsfp_io_target_prepare(packet+1,256,0x2000,device+1,72,0x4000,driver+1,68,0x6001,&target));
    CHECK(!tsfp_io_target_prepare(packet+1,256,0x2000,device+1,72,0xfffffff0,driver+1,68,0x6000,&target));
    CHECK(!tsfp_io_target_prepare(packet+1,256,0x2000,device+1,72,0x4000,driver+1,68,0xfffffff0,&target));
    put(packet+1+0x5c,0x207f);save();CHECK(!prepare(&target));CHECK(!memcmp(&target,&before,sizeof(target)));unchanged();
    CHECK(!tsfp_io_request_peek(packet+1,103,0x2000,&stack));
    puts("PASS: read-only dispatch selection for all14 majors, exact guest offsets, slot validation and failed-output preservation");return 0;
}
