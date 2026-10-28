#include "guest_virtual_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 0x80000u
#define HEAP 0x10000u
#define ARG UINT32_C(0xc000000d)
#define ACCESS UINT32_C(0xc0000005)
static unsigned char memory[SIZE], saved[SIZE];
static tsfp_allocations allocator;
static tsfp_allocation_record records[64];
static tsfp_guest_vm vm;
static unsigned calls;
static int fail_protect, inaccessible;
static uint32_t denied, protect_at, protect_bytes, protect_flags;

static void *resolve(void *host, uint32_t a, uint32_t n, int access) {
    assert(host == memory);
    if (a < 4096 || (uint64_t)a + n > SIZE) return NULL;
    if (denied && access == 1 && a <= denied && (uint64_t)a + n > denied) return NULL;
    if (inaccessible && access != -1 && a >= 0x40000) return NULL;
    return memory + a;
}
static int protect(void *host, uint32_t a, uint32_t n, uint32_t flags, uint32_t *old) {
    assert(host == memory);
    ++calls; protect_at = a; protect_bytes = n; protect_flags = flags;
    if (fail_protect) return 0;
    *old = 0x20; return 1;
}
static void put(uint32_t a, uint32_t n) {
    for (unsigned i=0;i<4;++i) memory[a+i]=(unsigned char)(n>>(i*8));
}
static uint32_t get(uint32_t a) {
    uint32_t result=0;
    for (unsigned i=0;i<4;++i) result|=(uint32_t)memory[a+i]<<(i*8);
    return result;
}
static void setup(void) {
    tsfp_allocation_config c={0};
    memset(memory,0xa5,sizeof(memory));
    c.ram=memory; c.ram_size=SIZE; c.heap_begin=HEAP; c.reserve_begin=0x50000;
    assert(tsfp_allocations_init(&allocator,&c,records,64,NULL,0));
    memset(&vm,0,sizeof(vm));
    vm.allocations=&allocator; vm.host=memory; vm.resolve=resolve; vm.protect=protect;
    vm.code_begin=0x12000; vm.ram_size=0x60000; vm.host_page_size=4096;
    calls=0; fail_protect=0; inaccessible=0; denied=0;
}
static void allocation_checks(void) {
    setup(); put(0x2001,0); put(0x2013,19);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,0,0x2013,0x3000,4)==0);
    assert(get(0x2001)==HEAP && get(0x2013)==19);
    for(unsigned i=0;i<19;++i) assert(memory[HEAP+i]==0);
    assert(memory[HEAP+19]==0xa5 && allocator.heap.count==1);
    /* A commit on existing guest storage does not allocate or republish. */
    put(0x2013,9); memcpy(saved,memory,SIZE);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,27,0x2013,0x1000,1)==0);
    assert(allocator.heap.count==1 && !memcmp(saved,memory,SIZE));
    setup(); put(0x2001,0); put(0x2013,0x100000);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,0,0x2013,0x2000,4)==0);
    assert(get(0x2001)==HEAP && get(0x2013)==0x40000);
    setup(); put(0x2001,0); put(0x2013,0x100000); memcpy(saved,memory,SIZE);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,0,0x2013,0x3000,4)==0xc0000017u);
    assert(!allocator.heap.count && !memcmp(saved,memory,SIZE));
    /* One output aliases the other: all inputs precede publication. */
    setup(); put(0x2001,32);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,0,0x2001,0x3000,4)==0);
    assert(get(0x2001)==32 && records[0].bytes==32);
    /* Pointer words in a currently free interval survive allocation zeroing. */
    setup(); put(HEAP+1,0); put(HEAP+7,32);
    assert(tsfp_guest_vm_allocate(&vm,HEAP+1,0,HEAP+7,0x3000,4)==0);
    assert(get(HEAP+1)==HEAP && get(HEAP+7)==32);
}
static void invalid_checks(void) {
    const uint32_t bad[]={0,1,4095,SIZE-3,SIZE,UINT32_MAX-2,UINT32_MAX};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        setup(); put(0x2001,0); put(0x2013,32); memcpy(saved,memory,SIZE);
        assert(tsfp_guest_vm_allocate(&vm,bad[i],0,0x2013,0x3000,4)==ARG);
        assert(tsfp_guest_vm_allocate(&vm,0x2001,0,bad[i],0x3000,4)==ARG);
        assert(!allocator.heap.count && !memcmp(saved,memory,SIZE));
    }
    setup(); put(0x2001,0); put(0x2013,32); denied=0x2016;
    memcpy(saved,memory,SIZE);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,0,0x2013,0x3000,4)==ARG);
    assert(!allocator.heap.count && !memcmp(saved,memory,SIZE));
    denied=0; put(0x2013,0);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,0,0x2013,0x3000,4)==0xc0000045u);
    assert(tsfp_guest_vm_allocate(NULL,0x2001,0,0x2013,0x3000,4)==ARG);
    put(0x2013,32); tsfp_allocations_stop(&allocator); memcpy(saved,memory,SIZE);
    assert(tsfp_guest_vm_allocate(&vm,0x2001,0,0x2013,0x3000,4)==0xc0000017u);
    assert(!memcmp(saved,memory,SIZE));
}
static void free_checks(void) {
    uint32_t address;
    setup(); address=tsfp_heap_allocate(&allocator,31,4096); assert(address==HEAP);
    put(0x2001,address); put(0x2013,31); memcpy(saved,memory,SIZE);
    assert(tsfp_guest_vm_free(&vm,0x2001,0x2013,0x4000,0)==0);
    assert(tsfp_guest_vm_free(&vm,0x2001,0x2013,0x8000,1)==0);
    assert(allocator.heap.count==1 && !memcmp(saved,memory,SIZE));
    denied=0x2016;
    assert(tsfp_guest_vm_free(&vm,0x2001,0x2013,0x8000,0)==ARG);
    assert(allocator.heap.count==1 && !memcmp(saved,memory,SIZE));
    denied=0;
    assert(tsfp_guest_vm_free(&vm,0x2001,0x2013,0x8000,0)==0);
    assert(!allocator.heap.count && get(0x2001)==0 && get(0x2013)==0);
    assert(tsfp_guest_vm_free(&vm,0x2001,0,0x8000,0)==ARG);
    address=tsfp_reserve_allocate(&allocator,4096,4096); assert(address==0x50000);
    assert(!tsfp_heap_release(&allocator,address));
    assert(!tsfp_virtual_release(&allocator,address+1));
    put(0x2001,address);
    assert(tsfp_guest_vm_free(&vm,0x2001,0,0x8000,0)==0);
    assert(!allocator.heap.count && tsfp_reserve_allocate(&allocator,4096,4096)==address);
    assert(!tsfp_virtual_release(&allocator,0x80010000));
    /* Unowned release compatibility cannot free a different owner. */
    put(0x2001,address+1); put(0x2013,77);
    assert(tsfp_guest_vm_free(&vm,0x2001,0x2013,0x8000,0)==0);
    assert(allocator.heap.count==1 && !get(0x2001) && !get(0x2013));
    {
        static unsigned char physical[16384];
        tsfp_allocation_record physical_records[4];
        tsfp_allocation_config c={0};
        c.ram=memory;c.ram_size=SIZE;c.heap_begin=HEAP;c.reserve_begin=0x50000;
        c.physical=physical;c.physical_base=0x80000000u;c.physical_size=sizeof(physical);
        c.physical_begin=0x80001000u;c.physical_end=0x80004000u;
        c.protected_begin=0x80000000u;c.protected_end=0x80001000u;
        assert(tsfp_allocations_init(&allocator,&c,records,64,physical_records,4));
        address=tsfp_physical_allocate(&allocator,37,4096);assert(address==0x80001000u);
        assert(!tsfp_virtual_release(&allocator,address));
        assert(!tsfp_virtual_release(&allocator,0x80000000u));
        assert(allocator.physical.count==2 && tsfp_allocation_remaining(&allocator,address)==37);
        put(0x2001,address);
        assert(tsfp_guest_vm_free(&vm,0x2001,0,0x8000,0)==0 && !get(0x2001));
        assert(allocator.physical.count==2 && tsfp_allocation_remaining(&allocator,address)==37);
        assert(tsfp_physical_release(&allocator,address));
        tsfp_allocations_stop(&allocator);
    }
}
static void query_checks(void) {
    const uint32_t addresses[]={0,0x11fff,0x12000,0x12123,0x5ffff,0x60000,UINT32_MAX};
    setup();
    for(size_t i=0;i<sizeof(addresses)/sizeof(*addresses);++i) {
        uint32_t a=addresses[i], page=a & ~4095u;
        int committed=page>=0x12000 && page<0x60000;
        memset(memory+0x2000,0xa5,40);
        assert(tsfp_guest_vm_query(&vm,a,0x2001)==0);
        assert(get(0x2001)==page && get(0x2005)==page);
        assert(get(0x2009)==(committed?4u:0u));
        assert(get(0x200d)==(committed?0x60000-page:4096u));
        assert(get(0x2011)==(committed?0x1000u:0x10000u));
        assert(get(0x2015)==4 && get(0x2019)==(committed?0x20000u:0u));
        assert(memory[0x2000]==0xa5 && memory[0x201d]==0xa5);
    }
    for(uint32_t tail=1;tail<28;++tail) {
        memcpy(saved,memory,SIZE);
        assert(tsfp_guest_vm_query(&vm,0x12000,SIZE-tail)==ARG);
        assert(!memcmp(saved,memory,SIZE));
    }
    assert(tsfp_guest_vm_query(&vm,0x12000,SIZE-28)==0);
    assert(tsfp_guest_vm_query(&vm,0,0)==ARG);
    assert(tsfp_guest_vm_query(&vm,0,UINT32_MAX-10)==ARG);
    assert(tsfp_guest_vm_query(NULL,0,0x2000)==ARG);
}
static void protect_checks(void) {
    setup(); put(0x2001,0x40003); put(0x2013,17); put(0x2025,123);
    inaccessible=1; /* The target is mapped but PAGE_NOACCESS; restore it. */
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,4,0x2025)==0);
    assert(calls==1 && protect_at==0x40003 && protect_bytes==17 && protect_flags==4);
    assert(get(0x2025)==0x20 && get(0x2001)==0x40003 && get(0x2013)==17);
    fail_protect=1; put(0x2025,123); memcpy(saved,memory,SIZE);
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,1,0x2025)==ACCESS);
    assert(calls==2 && !memcmp(saved,memory,SIZE));
    fail_protect=0; denied=0x2028;
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,4,0x2025)==ACCESS && calls==2);
    denied=0; inaccessible=0;
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,1,0x40080)==ACCESS && calls==2);
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,1,0x3fffe)==ACCESS && calls==2);
    put(0x2001,UINT32_MAX-8); put(0x2013,32);
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,4,0)==ACCESS && calls==2);
    put(0x2001,0x40000); put(0x2013,0);
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,4,0)==ACCESS && calls==2);
    put(0x2013,32);
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,4,0)==0 && calls==3);
    vm.host_page_size=16384;
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,1,0x41080)==ACCESS && calls==3);
    vm.host_page_size=3;
    assert(tsfp_guest_vm_protect(&vm,0x2001,0x2013,1,0)==ACCESS && calls==3);
}
int main(int argc, char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"allocate")) allocation_checks();
    else if(!strcmp(argv[1],"invalid")) invalid_checks();
    else if(!strcmp(argv[1],"free")) free_checks();
    else if(!strcmp(argv[1],"query")) query_checks();
    else if(!strcmp(argv[1],"protect")) protect_checks();
    else abort();
    puts("guest virtual memory contracts passed"); return 0;
}
