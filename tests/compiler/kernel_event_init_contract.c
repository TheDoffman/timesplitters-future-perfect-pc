#include "guest_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"event bridge line%d %s\n",__LINE__,#c);exit(1);}}while(0)
typedef void *HANDLE;
#define TRUE 1
#define FALSE 0
#define XBOX_LOG_WARN 1
#define XBOX_LOG_IO 1
static unsigned char bytes[32];static uint32_t args[3],g_eax;
static unsigned creates,inserts,logs,map_failure,host_failure;static int manual,initial;
#define STACK_ARG(i) args[i]
static void *bridge_object_output(void *c,uint32_t address,uint32_t size) {CHECK(!c && size==16);return !map_failure && address==0x1000?bytes+8:NULL;}
static HANDLE CreateEventW(void *security,int reset,int state,void *name) {
    CHECK(!security && !name);++creates;manual=reset;initial=state;return host_failure?NULL:(HANDLE)(uintptr_t)0x1234;
}
static void ke_shadow_insert(uint32_t address,HANDLE event) {CHECK(address==0x1000 && event==(HANDLE)(uintptr_t)0x1234);++inserts;}
static void xbox_log(int level,int category,const char *format,unsigned address) {CHECK(level==1 && category==1 && format && address==0x1000);++logs;}
#include "guest_kernel_event_init.inc"
int main(void) {
    static const unsigned char manual_header[16]={0,0,4,0,1,0,0,0,8,0x10,0,0,8,0x10,0,0};
    unsigned char before[32];
    memset(bytes,0xa5,sizeof(bytes));args[0]=0x1000;args[1]=0;args[2]=0xff;g_eax=9;bridge_KeInitializeEvent();
    CHECK(!g_eax && creates==1 && inserts==1 && manual && initial && !logs);
    CHECK(!memcmp(bytes+8,manual_header,16) && bytes[7]==0xa5 && bytes[24]==0xa5);
    args[1]=1;args[2]=0x100;bridge_KeInitializeEvent();CHECK(creates==2 && inserts==2 && !manual && !initial);
    CHECK(bytes[8]==1 && bytes[10]==4 && bytes[12]==0);
    memcpy(before,bytes,sizeof(bytes));map_failure=1;bridge_KeInitializeEvent();CHECK(creates==2 && !memcmp(before,bytes,sizeof(bytes)));map_failure=0;
    args[1]=2;bridge_KeInitializeEvent();CHECK(creates==2 && !memcmp(before,bytes,sizeof(bytes)));args[1]=0;
    args[0]=0x1001;bridge_KeInitializeEvent();CHECK(creates==2 && !memcmp(before,bytes,sizeof(bytes)));args[0]=0x1000;
    host_failure=1;bridge_KeInitializeEvent();CHECK(creates==3 && inserts==2 && logs==1 && !memcmp(before,bytes,sizeof(bytes)));
    puts("PASS: original event initialize bridge, exact guest words, host event modes, full-span rejection and host failure without guest mutation");return 0;
}
