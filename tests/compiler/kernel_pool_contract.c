#include "guest_virtual_memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pool line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static tsfp_allocations state;
static tsfp_allocation_record heap[8],physical[8];
static unsigned char ram[32768],device[8192],before[32768];
static uint32_t args[2],g_eax;static unsigned reads[2],allowed,lookups;
static uint32_t argument(unsigned i) {CHECK(i<allowed);++reads[i];return args[i];}
#define STACK_ARG(i) argument(i)
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {memset(vm,0,sizeof(*vm));vm->allocations=&state;++lookups;}
#include "guest_kernel_pool.inc"
static void init(size_t count) {
    tsfp_allocation_config config={0};memset(&state,0,sizeof(state));memset(ram,0xa5,sizeof(ram));memset(device,0xb6,sizeof(device));
    config.ram=ram;config.ram_size=sizeof(ram);config.heap_begin=0x1004;config.reserve_begin=0x4000;
    config.physical=device;config.physical_base=0x81000000;config.physical_size=sizeof(device);config.physical_begin=0x81000000;config.physical_end=0x81002000;
    CHECK(tsfp_allocations_init(&state,&config,heap,count,physical,8));
}
static void prepare(uint32_t a,uint32_t b,unsigned count) {args[0]=a;args[1]=b;reads[0]=reads[1]=0;allowed=count;g_eax=0xdeadbeef;}
static uint32_t allocate(uint32_t bytes,uint32_t tag,int tagged) {
    unsigned old=lookups;prepare(bytes,tag,tagged?2:1);
    if(tagged)bridge_ExAllocatePoolWithTag();else bridge_ExAllocatePool();
    CHECK(reads[0]==1 && reads[1]==(tagged?1u:0u) && lookups==old+1);return g_eax;
}
static uint32_t query(uint32_t address) {prepare(address,0,1);bridge_ExQueryPoolBlockSize();CHECK(reads[0]==1 && !reads[1]);return g_eax;}
static void release(uint32_t address) {prepare(address,0,1);bridge_ExFreePool();CHECK(!g_eax && reads[0]==1 && !reads[1]);}
int main(void) {
    uint32_t a,b,reserve,p;unsigned i;
    CHECK(!allocate(16,0,0));init(8);
    a=allocate(0,0,0);CHECK(a==0x1010 && query(a)==16 && query(a+15)==1 && !query(a+16));
    for(i=0;i<16;++i)CHECK(!ram[a+i]);CHECK(ram[a-1]==0xa5 && ram[a+16]==0xa5);
    ram[a]=0x6b;release(a+1);CHECK(query(a)==16 && ram[a]==0x6b);release(0xf0000000);CHECK(query(a)==16);
    release(a);CHECK(!query(a) && ram[a]==0x6b);release(a);release(0);
    b=allocate(33,0xfedcba98,1);CHECK(b==a && query(b)==33 && query(b+32)==1 && !query(b+33));
    memcpy(before,ram,sizeof(ram));CHECK(!allocate(UINT32_MAX,0,0) && !memcmp(before,ram,sizeof(ram)));
    CHECK(!allocate(0x10010,0,0) && !memcmp(before,ram,sizeof(ram)));
    reserve=tsfp_reserve_allocate(&state,32,16);CHECK(reserve && !query(reserve));release(reserve);CHECK(tsfp_virtual_release(&state,reserve));
    p=tsfp_physical_allocate(&state,32,4096);CHECK(p==0x81000000 && query(p+5)==27);release(p);CHECK(tsfp_physical_release(&state,p));
    release(b);init(2);a=allocate(16,7,1);b=allocate(16,8,1);CHECK(a && b && a!=b);
    memcpy(before,ram,sizeof(ram));CHECK(!allocate(16,9,1) && !memcmp(before,ram,sizeof(ram)));
    release(a);CHECK(allocate(1,UINT32_MAX,1)==a && query(a)==16);
    tsfp_allocations_stop(&state);CHECK(!allocate(16,0,0) && !allocate(16,0,1) && !query(a));release(a);
    puts("PASS: original pool ABI, real guest ownership/zeroing/query, tag data, exhaustion, foreign-kind rejection and inactive lifetime");return 0;
}
