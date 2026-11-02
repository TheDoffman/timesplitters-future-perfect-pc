#ifndef TSFP_X64_ACCESS_CASES_H
#define TSFP_X64_ACCESS_CASES_H
#include "x64_access.h"
#include <stdio.h>
static int tsfp_x64_decoder_cases(void){
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL decoder %d: %s\n",__LINE__,#x);return 0;}}while(0)
    uint64_t r[16]={0};TsfpX64Access a;r[0]=0x100000000;r[9]=0x40;
    const unsigned char store[]={0x46,0x89,0x54,0x88,0x10};
    CHECK(tsfp_x64_access(store,sizeof store,0,r,&a)&&a.address==0x100000110&&a.reg==10&&a.bytes==4&&!a.load);
    const unsigned char high[]={0x8a,0x20};
    CHECK(tsfp_x64_access(high,sizeof high,0,r,&a)&&a.reg==0&&a.shift==8);
    CHECK(tsfp_x64_load_result(0x12345678,0xab,&a)==0x1234ab78);
    const unsigned char zx[]={0x0f,0xb6,0x30};
    CHECK(tsfp_x64_access(zx,sizeof zx,0,r,&a)&&a.reg==6&&a.shift==0&&a.result_bytes==4);
    CHECK(tsfp_x64_load_result(UINT64_MAX,0x80,&a)==0x80);
    const unsigned char sx[]={0x48,0x0f,0xbe,0x00};
    CHECK(tsfp_x64_access(sx,sizeof sx,0,r,&a)&&tsfp_x64_load_result(0,0x80,&a)==UINT64_MAX-127);
    const unsigned char imm[]={0x48,0xc7,0x05,0x10,0,0,0,0xff,0xff,0xff,0xff};
    CHECK(tsfp_x64_access(imm,sizeof imm,0x1000,r,&a)&&a.length==11&&a.address==0x101b&&a.immediate==UINT64_MAX);
    for(size_t n=0;n<sizeof imm;n++)CHECK(!tsfp_x64_access(imm,n,0x1000,r,&a));
    const unsigned char nope[]={0x90};CHECK(!tsfp_x64_access(nope,1,0,r,&a));
    return 1;
#undef CHECK
}
#endif
