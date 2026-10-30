#include "guest_virtual_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"string ABI line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[1024],before[1024];static uint32_t args[3],g_eax,blocked,readonly;
static unsigned reads[3],allowed,lookups;static int missing;
static void *resolve(void *host,uint32_t address,uint32_t bytes,int writable) {
    CHECK(host==ram && bytes && address && address<=UINT32_MAX-(bytes-1) && (writable==0 || writable==1));
    if(address>=sizeof(ram) || bytes>sizeof(ram)-address || (blocked && address<=blocked && bytes>blocked-address) ||
       (writable && readonly && address<=readonly && bytes>readonly-address))return NULL;
    return ram+address;
}
static uint32_t argument(unsigned i){CHECK(i<allowed);++reads[i];return args[i];}
#define STACK_ARG(i) argument(i)
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    unsigned i;for(i=0;i<3;++i)CHECK(reads[i]==(i<allowed?1u:0u));++lookups;
    memset(vm,0,sizeof(*vm));vm->host=ram;if(!missing)vm->resolve=resolve;
}
#include "guest_kernel_strings.inc"
static void put(unsigned at,unsigned count,uint32_t value){unsigned i;for(i=0;i<count;++i)ram[at+i]=(unsigned char)(value>>(8*i));}
static void record(unsigned at,unsigned length,unsigned maximum,uint32_t buffer){put(at,2,length);put(at+2,2,maximum);put(at+4,4,buffer);}
static void reset(unsigned width) {
    memset(ram,0xa5,sizeof(ram));record(17,2*width,8*width,129);record(33,3*width,0,257);
    if(width==1){memcpy(ram+129,"aBx",3);memcpy(ram+257,"Abx\0",4);}
    else {unsigned i;const char *s="Abx";for(i=0;i<3;++i){put(129+2*i,2,(unsigned char)s[i]);put(257+2*i,2,(unsigned char)s[i]);}put(263,2,0);}
}
static uint32_t invoke(void (*function)(void),unsigned count,uint32_t a,uint32_t b,uint32_t c) {
    unsigned i,old=lookups;allowed=count;memset(reads,0,sizeof(reads));args[0]=a;args[1]=b;args[2]=c;g_eax=0xdeadbeef;function();
    CHECK(lookups==old+1);for(i=0;i<3;++i)CHECK(reads[i]==(i<count?1u:0u));return g_eax;
}
int main(void) {
    reset(1);CHECK(!invoke(bridge_RtlInitAnsiString,2,17,257,0));CHECK(ram[17]==3 && ram[19]==4 && ram[21]==1 && ram[22]==1);
    reset(2);CHECK(!invoke(bridge_RtlInitUnicodeString,2,17,257,0));CHECK(ram[17]==6 && ram[19]==8 && ram[21]==1 && ram[22]==1);
    reset(1);record(17,3,0,129);CHECK(invoke(bridge_RtlEqualString,3,17,33,1)==1);CHECK(!invoke(bridge_RtlEqualString,3,17,33,0));
    ram[130]=ram[258]=0;ram[131]='x';ram[259]='y';CHECK(!invoke(bridge_RtlEqualString,3,17,33,1));
    reset(2);record(17,6,0,129);CHECK(invoke(bridge_RtlEqualUnicodeString,3,17,33,1)==1);
    put(133,2,'y');CHECK(!invoke(bridge_RtlEqualUnicodeString,3,17,33,0));
    reset(1);record(17,1,1,129);record(33,1,1,257);ram[129]='z';ram[257]='[';
    CHECK(invoke(bridge_RtlCompareString,3,17,33,1)==UINT32_MAX);
    reset(2);CHECK(invoke(bridge_RtlCompareUnicodeString,3,17,33,0)==UINT32_MAX);
    reset(1);CHECK(!invoke(bridge_RtlCopyString,2,17,33,0));CHECK(ram[17]==3 && !memcmp(ram+129,"Abx",3) && ram[132]==0xa5);
    reset(2);CHECK(!invoke(bridge_RtlCopyUnicodeString,2,17,33,0));CHECK(ram[17]==6 && !memcmp(ram+129,ram+257,6) && !ram[135] && !ram[136]);
    reset(1);CHECK(!invoke(bridge_RtlUpperString,2,17,33,0));CHECK(ram[17]==3 && !memcmp(ram+129,"ABX",3));
    reset(1);CHECK(!invoke(bridge_RtlAppendStringToString,2,17,33,0));CHECK(ram[17]==5 && !memcmp(ram+129,"aBAbx",5));
    reset(2);CHECK(!invoke(bridge_RtlAppendUnicodeStringToString,2,17,33,0));CHECK(ram[17]==10 && !memcmp(ram+133,ram+257,6) && !ram[139] && !ram[140]);
    reset(2);CHECK(!invoke(bridge_RtlAppendUnicodeToString,2,17,257,0));CHECK(ram[17]==10 && !memcmp(ram+133,ram+257,6));
    reset(1);record(17,2,3,129);memcpy(before,ram,sizeof(ram));CHECK(invoke(bridge_RtlAppendStringToString,2,17,33,0)==0xc0000023u && !memcmp(before,ram,sizeof(ram)));
    reset(2);readonly=135;memcpy(before,ram,sizeof(ram));CHECK(!invoke(bridge_RtlCopyUnicodeString,2,17,33,0) && !memcmp(before,ram,sizeof(ram)));readonly=0;
    reset(1);blocked=259;memcpy(before,ram,sizeof(ram));CHECK(!invoke(bridge_RtlCopyString,2,17,33,0) && !memcmp(before,ram,sizeof(ram)));blocked=0;
    missing=1;CHECK(invoke(bridge_RtlAppendStringToString,2,17,33,0)==0xc000000du && !memcmp(before,ram,sizeof(ram)));missing=0;
    reset(1);CHECK(invoke(bridge_RtlCompareMemory,3,129,257,3)==0);ram[129]='A';CHECK(invoke(bridge_RtlCompareMemory,3,129,257,3)==1);
    put(129,4,0x87654321);put(133,4,0x87654321);CHECK(invoke(bridge_RtlCompareMemoryUlong,3,129,11,0x87654321)==8);
    puts("PASS: fourteen string/memory imports, fixed argument reads, guest-width records, full spans, failure preservation and EAX");return 0;
}
