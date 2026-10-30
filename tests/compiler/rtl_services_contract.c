#include "guest_rtl_services.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"RTL services line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[512],last='A';static unsigned calls,maps;
static uint32_t fallback(void *context,uint32_t status){CHECK(context==ram);++calls;return status^0x76543210u;}
static void *resolve(void *context,uint32_t address,uint32_t bytes,int writable) {
    CHECK(context==ram && address && (bytes==1 || bytes==4) && !writable);++maps;
    if(address==UINT32_MAX && bytes==1)return &last;
    if(address>=sizeof(ram) || bytes>sizeof(ram)-address)return NULL;return ram+address;
}
int main(void) {
    static const uint32_t statuses[]={0xc0000120,0xc00000bb,0xc000009a,0xc000003a,0xc0000035,0xc0000034,0xc0000022,0xc0000021,0xc0000017,
        0xc0000011,0xc000000f,0xc000000d,0xc0000008,0xc0000002,0xc0000001,0x80000006,0x80000005,0x103,0};
    static const uint32_t errors[]={1223,50,1450,3,183,2,5,1455,8,38,2,87,6,120,31,18,234,997,0};
    tsfp_guest_vm vm={0};char output[12],before[12];unsigned i,old;
    for(i=0;i<sizeof(statuses)/sizeof(statuses[0]);++i)CHECK(tsfp_rtl_status_error(statuses[i],fallback,ram)==errors[i]);CHECK(!calls);
    for(i=1;i<10000;++i){uint32_t status=0x40000000u+i*237u;CHECK(tsfp_rtl_status_error(status,fallback,ram)==(status^0x76543210u));}
    CHECK(calls==9999 && tsfp_rtl_status_error(UINT32_MAX,NULL,NULL)==317 && tsfp_rtl_status_error(UINT32_MAX,fallback,ram)==(UINT32_MAX^0x76543210u));
    vm.host=ram;vm.resolve=resolve;memset(ram,'x',sizeof(ram));memcpy(ram+17,"hello",6);memset(output,0xa5,sizeof(output));
    CHECK(tsfp_rtl_text(&vm,17,output+2,8)==1 && !strcmp(output+2,"hello") && (unsigned char)output[1]==0xa5 && (unsigned char)output[10]==0xa5);
    CHECK(tsfp_rtl_text(&vm,23,output+2,8)==2 && !strcmp(output+2,"xxxxxx~"));
    CHECK(tsfp_rtl_text(&vm,23,output+2,2)==2 && !strcmp(output+2,"~"));
    CHECK(!tsfp_rtl_text(&vm,0,output+2,8) && !strcmp(output+2,"?"));
    CHECK(!tsfp_rtl_text(&vm,510,output+2,8) && !strcmp(output+2,"?"));
    old=maps;CHECK(!tsfp_rtl_text(&vm,UINT32_MAX,output+2,8) && !strcmp(output+2,"?") && maps==old+1);
    memcpy(before,output,sizeof(output));CHECK(!tsfp_rtl_text(&vm,17,output+2,1) && !memcmp(before,output,sizeof(output)));
    CHECK(!tsfp_rtl_text(&vm,17,NULL,8));CHECK(!tsfp_rtl_text(NULL,17,output+2,8) && !strcmp(output+2,"?"));
    puts("PASS: nineteen status overrides, raw fallback, bounded mapped diagnostic text, overflow, guards and failure");return 0;
}
