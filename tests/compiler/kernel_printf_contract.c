#include "guest_printf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"printf ABI line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[8192];static uint32_t fixed[4],g_esp=256,g_eax;static unsigned allowed,lookups,frames,high,short_frame;
static void put(unsigned at,unsigned bytes,uint64_t value){unsigned i;for(i=0;i<bytes;++i)ram[at+i]=(unsigned char)(value>>(8*i));}
static void *resolve(void *host,uint32_t address,uint32_t bytes,int writable) {
    CHECK(host==ram && address && bytes && address<=UINT32_MAX-(bytes-1) && (writable==0 || writable==1));
    if(address==g_esp && !writable) {CHECK(bytes==allowed*4);++frames;if(short_frame)return NULL;}
    if(address&0x80000000u){++high;address&=0x7fffffffu;}
    if(address>=sizeof(ram) || bytes>sizeof(ram)-address)return NULL;return ram+address;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    ++lookups;memset(vm,0,sizeof(*vm));vm->host=ram;vm->resolve=resolve;
}
#include "guest_kernel_printf.inc"
static void prepare(const char *format,unsigned count) {
    memset(ram,0xa5,sizeof(ram));memcpy(ram+65,format,strlen(format)+1);allowed=count;g_eax=0xdeadbeef;g_esp=256;frames=high=short_frame=0;
    fixed[0]=4097;fixed[1]=128;fixed[2]=65;fixed[3]=524;
}
static uint32_t invoke(void (*function)(void),int frame) {
    unsigned old=lookups,i;uint32_t stack=g_esp;
    if(stack<sizeof(ram) && allowed*4<=sizeof(ram)-stack)for(i=0;i<allowed;++i)put(stack+i*4,4,fixed[i]);
    function();CHECK(lookups==old+1 && g_esp==stack && frames==(unsigned)frame);return g_eax;
}
int main(void) {
    prepare("%.2f:%I64d:%u",3);put(268,8,UINT64_C(0x400a000000000000));put(276,8,UINT64_C(0xffffffffffffffd6));put(284,4,17);
    CHECK(invoke(bridge_RtlSnprintf,1)==strlen("3.25:-42:17") && !strcmp((char *)ram+4097,"3.25:-42:17"));
    prepare("%x:%I64u",2);fixed[1]=65;put(264,4,0x1234);put(268,8,UINT64_MAX);
    CHECK(invoke(bridge_RtlSprintf,1)==strlen("1234:18446744073709551615") && !strcmp((char *)ram+4097,"1234:18446744073709551615"));
    prepare("%.2f:%u",4);put(524,8,UINT64_C(0x400a000000000000));put(532,4,42);
    CHECK(invoke(bridge_RtlVsnprintf,1)==strlen("3.25:42") && !strcmp((char *)ram+4097,"3.25:42"));
    prepare("%s:%ld",3);fixed[1]=65;fixed[2]=524;put(524,4,1025);put(528,4,UINT32_C(0xffffffff));memcpy(ram+1025,"guest",6);
    CHECK(invoke(bridge_RtlVsprintf,1)==strlen("guest:-1") && !strcmp((char *)ram+4097,"guest:-1"));
    prepare("hello",3);g_esp=UINT32_MAX-4;CHECK(!invoke(bridge_RtlSnprintf,0) && ram[4097]==0xa5);
    prepare("hello",2);fixed[1]=65;g_esp=UINT32_MAX-4;CHECK(!invoke(bridge_RtlSprintf,0) && ram[4097]==0xa5);
    prepare("hello",4);fixed[3]=0;CHECK(!invoke(bridge_RtlVsnprintf,1) && ram[4097]==0xa5);
    prepare("hello",3);fixed[1]=65;fixed[2]=0;CHECK(!invoke(bridge_RtlVsprintf,1) && ram[4097]==0xa5);
    prepare("hello",3);short_frame=1;CHECK(!invoke(bridge_RtlSnprintf,1) && ram[4097]==0xa5);
    prepare("%u",4);fixed[0]|=0x80000000u;fixed[2]|=0x80000000u;fixed[3]|=0x80000000u;put(524,4,42);
    CHECK(invoke(bridge_RtlVsnprintf,1)==2 && !strcmp((char *)ram+4097,"42") && high>=3);
    puts("PASS: four cdecl printf imports, fixed argument snapshots, direct/list cursor placement, 32-bit long and stack-wrap/NULL-list preservation");return 0;
}
