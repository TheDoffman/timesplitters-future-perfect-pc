#include "guest_virtual_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"time ABI line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[256],before[256];
static uint32_t args[2],g_eax;static unsigned reads[2],maps,lookups;
static int absent,missing;static uint32_t denied;
static uint32_t argument(unsigned i){CHECK(i<2);++reads[i];return args[i];}
#define STACK_ARG(i) argument(i)
static void *resolve(void *host,uint32_t address,uint32_t bytes,int writable) {
    CHECK(host==ram);++maps;
    CHECK(address && address<=UINT32_MAX-(bytes-1));
    CHECK((maps==1 && bytes==(absent?8u:16u) && !writable) || (maps==2 && bytes==(absent?16u:8u) && writable==1));
    if(address==denied || address>=sizeof(ram) || bytes>sizeof(ram)-address)return NULL;
    return ram+address;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    CHECK(reads[0]==1 && reads[1]==1);++lookups;memset(vm,0,sizeof(*vm));vm->host=ram;
    if(!missing)vm->resolve=resolve;
}
#include "guest_kernel_time.inc"
static void invoke(int decode,uint32_t a,uint32_t b) {
    unsigned old=lookups;absent=decode;args[0]=a;args[1]=b;reads[0]=reads[1]=maps=0;g_eax=0xdeadbeef;
    if(decode)bridge_RtlTimeToTimeFields();else bridge_RtlTimeFieldsToTime();
    CHECK(reads[0]==1 && reads[1]==1 && lookups==old+1);
}
int main(void) {
    const unsigned char fields[16]={0x41,6,1,0,1,0,0,0,0,0,0,0,0,0,99,0};unsigned i;
    memset(ram,0xa5,256);memcpy(ram+33,fields,16);invoke(0,33,65);CHECK(g_eax==1 && maps==2);
    for(i=65;i<73;++i)CHECK(!ram[i]);CHECK(ram[64]==0xa5 && ram[73]==0xa5);
    invoke(1,65,97);CHECK(g_eax==0 && maps==2 && !memcmp(ram+97,fields,14) && ram[111]==1 && !ram[112]);
    CHECK(ram[96]==0xa5 && ram[113]==0xa5);
    /* Invalid encode preserves destination; invalid mapped decode zeroes it. */
    ram[35]=2;ram[37]=30;memcpy(before,ram,256);invoke(0,33,65);CHECK(!g_eax && !memcmp(before,ram,256));
    memset(ram+65,0xff,8);invoke(1,65,97);CHECK(!g_eax);for(i=97;i<113;++i)CHECK(!ram[i]);
    /* Missing mappings/callbacks and null/wrapping spans never mutate RAM. */
    for(i=0;i<2;++i) {
        memcpy(before,ram,256);denied=33;invoke((int)i,33,65);CHECK(!g_eax && !memcmp(before,ram,256));
        denied=65;invoke((int)i,33,65);CHECK(!g_eax && !memcmp(before,ram,256));denied=0;
        invoke((int)i,253,65);CHECK(!g_eax && !memcmp(before,ram,256));
        missing=1;invoke((int)i,33,65);CHECK(!g_eax && !maps && !memcmp(before,ram,256));missing=0;
        /* Both are rejected before resolve, so no valid-side map ordering ambiguity. */
        invoke((int)i,UINT32_MAX-1,0);CHECK(!g_eax && !maps && !memcmp(before,ram,256));
    }
    memcpy(ram+33,fields,16);invoke(0,33,35);CHECK(g_eax==1);for(i=35;i<43;++i)CHECK(!ram[i]);
    memset(ram+33,0,8);invoke(1,33,35);CHECK(!g_eax && !memcmp(ram+35,fields,14) && ram[49]==1);
    puts("PASS: mapped calendar imports, spans, overlap, failure preservation and 2-word ABI");return 0;
}
