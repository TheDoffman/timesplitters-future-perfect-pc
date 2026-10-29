/* Synthetic bridge register/guest-address environment with real Windows locks
 * and the actual project guest heap. Not a substitute production kernel header. */
#include <windows.h>
#include "guest_io_pool.h"
#include "guest_allocation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"kernel request line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static tsfp_allocations allocations;static tsfp_allocation_record records[32];
static unsigned char ram[65536],saved[256];
static volatile unsigned allowed_args;static volatile uint32_t args[3];
static uint32_t g_eax;static unsigned warnings;static int deny_mapping;
typedef struct {tsfp_allocations *allocations;} tsfp_guest_vm;
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {vm->allocations=&allocations;}
static uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) {
    CHECK(alignment==16);return tsfp_heap_allocate(&allocations,bytes,alignment);
}
static uint32_t argument(unsigned index) {CHECK(index<allowed_args);return args[index];}
#define STACK_ARG(n) argument(n)
enum {XBOX_LOG_WARN=1};
#define XBOX_LOG_IO "IO"
static void xbox_log(int level,const char *subsystem,const char *format,...) {
    CHECK(level==1 && !strcmp(subsystem,"IO"));(void)format;++warnings;
}
static void *bridge_object_output(void *context,uint32_t address,uint32_t bytes) {
    (void)context;if(deny_mapping || !address || address>=sizeof(ram) || bytes>sizeof(ram)-address)return NULL;
    return ram+address;
}
#include "guest_kernel_requests.inc"
/*ACTUAL_CLEANUP*/
static void empty(void) {
    size_t live,retained,uncertain;tsfp_io_pool_counts(&bridge_requests,&live,&retained,&uncertain);
    CHECK(!live && !retained && !uncertain && !allocations.heap.count);
}
static uint32_t allocate_request(uint32_t stacks) {
    allowed_args=1;args[0]=stacks;bridge_IoAllocateIrp();return g_eax;
}
static void free_request(uint32_t address) {
    allowed_args=1;args[0]=address;bridge_IoFreeIrp();CHECK(!g_eax);
}
static void initialize(uint32_t address,uint32_t bytes,uint32_t stacks) {
    allowed_args=3;args[0]=address;args[1]=bytes;args[2]=stacks;
    bridge_IoInitializeIrp();CHECK(!g_eax);
}
static void pending(void *ctx,tsfp_io_owner *owner) {CHECK(ctx==&bridge_requests && tsfp_io_owner_pending(owner));}
int main(void) {
    tsfp_allocation_config config;tsfp_io_owner *owner;uint32_t a,other;unsigned before;
    memset(&config,0,sizeof(config));config.ram=ram;config.ram_size=sizeof(ram);
    config.heap_begin=0x1000;config.reserve_begin=0x8000;
    CHECK(tsfp_allocations_init(&allocations,&config,records,32,NULL,0));
    CHECK(stdcall_args_for_ordinal(59)==4 && stdcall_args_for_ordinal(72)==4 && stdcall_args_for_ordinal(73)==12);
    bridge_request_init();empty();CHECK(!allocate_request(128));empty();
    a=allocate_request(2);CHECK(a==0x1000 && tsfp_allocation_remaining(&allocations,a)==152);
    CHECK(ram[a]==6 && ram[a+2]==152 && ram[a+0x18]==2 && ram[a+0x19]==3);
    CHECK(ram[a+0x5c]==0x98 && ram[a+0x5d]==0x10);
    owner=tsfp_io_registry_acquire(&bridge_requests.registry,a);CHECK(owner && owner->bytes==ram+a);
    memcpy(saved,ram+a,152);before=warnings;initialize(a,200,2);
    CHECK(warnings==before+1 && !memcmp(saved,ram+a,152));
    before=warnings;deny_mapping=1;initialize(a,152,2);deny_mapping=0;
    CHECK(warnings==before+1 && !memcmp(saved,ram+a,152));
    initialize(a,128,1);CHECK(ram[a+2]==128 && ram[a+0x18]==1 && ram[a+0x19]==2);
    CHECK(tsfp_io_owner_dispatch(owner,pending,&bridge_requests));
    memcpy(saved,ram+a,152);before=warnings;initialize(a,152,2);
    CHECK(warnings==before+1 && !memcmp(saved,ram+a,152));
    CHECK(tsfp_io_owner_release(owner));free_request(a);
    CHECK(tsfp_allocation_remaining(&allocations,a)==152);
    CHECK(!tsfp_io_registry_acquire(&bridge_requests.registry,a));
    CHECK(tsfp_io_owner_complete(owner,0,0,NULL,NULL));empty();
    a=allocate_request(1);CHECK(a);free_request(a);empty();
    other=tsfp_heap_allocate(&allocations,64,16);CHECK(other);before=warnings;
    free_request(other);CHECK(warnings==before+1 && tsfp_allocation_remaining(&allocations,other)==64);
    CHECK(tsfp_heap_release(&allocations,other));empty();
    memset(ram+0x9000,0xa7,256);initialize(0x9001,152,2);
    CHECK(ram[0x9000]==0xa7 && ram[0x9099]==0xa7 && ram[0x9001]==6 && ram[0x9003]==152);
    CHECK(ram[0x905d]==0x99 && ram[0x905e]==0x90);
    memcpy(saved,ram+0x9000,256);before=warnings;initialize(0x9001,151,2);
    CHECK(warnings==before+1 && !memcmp(saved,ram+0x9000,256));
    before=warnings;initialize(0xfff0,128,1);CHECK(warnings==before+1);
    other=tsfp_heap_allocate(&allocations,0xf000,16);CHECK(other==0x1000);
    CHECK(!allocate_request(1));CHECK(tsfp_heap_release(&allocations,other));empty();
    tsfp_io_pool_stop(&bridge_requests);CHECK(!allocate_request(1));empty();
    DeleteCriticalSection(&bridge_request_registry_lock);DeleteCriticalSection(&bridge_request_lock);
    puts("PASS: real guest request kernel allocation/free/reset adapters and caller-owned packet validation");return 0;
}
