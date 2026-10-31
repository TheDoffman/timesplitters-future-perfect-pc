#include "guest_memory_services.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE 0x20000u
#define PHYS_BASE 0x80000000u
#define PHYS_SIZE 0x40000u
#define ARG UINT32_C(0xc000000d)
static unsigned char ram[RAM_SIZE],saved[RAM_SIZE],physical[PHYS_SIZE];
static tsfp_allocation_record heaps[32],physicals[32];
static tsfp_allocations allocations;
static tsfp_guest_mm mm;
static uint32_t denied;
static int inaccessible,host_failure;
static unsigned resolves,queries,protects,stats_calls;
static uint64_t host_bytes;
static void *last_address;
static uint32_t last_bytes,last_protection;

static void *resolve(void *host,uint32_t address,uint32_t bytes,int access) {
    assert(host==ram);++resolves;
    if(address<4096 || !bytes || (uint64_t)address+bytes>RAM_SIZE)return NULL;
    if(access==1 && denied && address<=denied && (uint64_t)address+bytes>denied)return NULL;
    if(inaccessible && access!=-1)return NULL;
    return ram+address;
}
static uint32_t query(void *host,const void *address) {
    assert(host==ram);++queries;last_address=(void *)address;
    return inaccessible?1:4;
}
static int protect(void *host,void *address,uint32_t bytes,uint32_t protection) {
    assert(host==ram);++protects;last_address=address;
    last_bytes=bytes;last_protection=protection;
    return !host_failure;
}
static int available(void *host,uint64_t *bytes) {
    assert(host==ram);++stats_calls;*bytes=host_bytes;return !host_failure;
}
static uint32_t get(uint32_t address) {
    uint32_t result=0;
    for(unsigned i=0;i<4;++i)result|=(uint32_t)ram[address+i]<<(i*8);
    return result;
}
static void setup(void) {
    tsfp_allocation_config config={0};
    memset(ram,0x5a,sizeof(ram));memset(physical,0x7a,sizeof(physical));
    config.ram=ram;config.ram_size=RAM_SIZE;config.heap_begin=0x4000;config.reserve_begin=0x18000;
    config.physical=physical;config.physical_base=PHYS_BASE;config.physical_size=PHYS_SIZE;
    config.physical_begin=PHYS_BASE+0x10000;config.physical_end=PHYS_BASE+PHYS_SIZE-0x2000;
    config.protected_begin=PHYS_BASE+0x8000;config.protected_end=PHYS_BASE+0x9000;
    assert(tsfp_allocations_init(&allocations,&config,heaps,32,physicals,32));
    memset(&mm,0,sizeof(mm));mm.vm.allocations=&allocations;mm.vm.host=ram;mm.vm.resolve=resolve;
    mm.physical_base=PHYS_BASE;mm.physical_size=PHYS_SIZE;mm.gpu_default=0x2000;
    mm.query_access=query;mm.set_access=protect;mm.available=available;
    denied=0;inaccessible=0;host_failure=0;resolves=0;queries=0;protects=0;stats_calls=0;
    host_bytes=UINT64_C(1)<<40;last_address=NULL;last_bytes=last_protection=0;
}
static void allocation_checks(void) {
    for(int kind=TSFP_MM_CONTIGUOUS;kind<=TSFP_MM_STACK;++kind) {
        const uint32_t sizes[]={0,1,17,4097};
        for(unsigned i=0;i<4;++i) {
            uint32_t result,base,bytes=sizes[i];
            enum tsfp_guest_mm_kind k=(enum tsfp_guest_mm_kind)kind;
            setup();result=tsfp_guest_mm_allocate(&mm,k,bytes);
            if(!bytes && (k==TSFP_MM_SYSTEM || k==TSFP_MM_DEBUG)) {
                assert(!result && !allocations.heap.count && allocations.physical.count==1);continue;
            }
            assert(result);
            base=k==TSFP_MM_STACK?result-bytes:result;
            if(k==TSFP_MM_CONTIGUOUS) {
                assert(base==PHYS_BASE+0x10000 && allocations.physical.count==2);
                for(uint32_t j=0;j<bytes;++j)assert(!physical[base-PHYS_BASE+j]);
            } else {
                assert(base==0x4000 && allocations.heap.count==1);
                for(uint32_t j=0;j<(bytes<16?16:bytes);++j)assert(!ram[base+j]);
                assert(tsfp_guest_mm_size(&mm,base)==(bytes<16?16:bytes));
            }
            tsfp_guest_mm_release(&mm,k,base+1);
            assert(allocations.heap.count+(allocations.physical.count-1)==1);
            tsfp_guest_mm_release(&mm,k,base);
            assert(!allocations.heap.count && allocations.physical.count==1);
            tsfp_guest_mm_release(&mm,k,base);
        }
    }
    setup();
    {
        uint32_t base=tsfp_guest_mm_allocate(&mm,TSFP_MM_SYSTEM,31);
        tsfp_guest_mm_release(&mm,TSFP_MM_CONTIGUOUS,base); /* historical heap fallback */
        assert(!allocations.heap.count);
    }
    assert(!tsfp_guest_mm_allocate(&mm,TSFP_MM_STACK,UINT32_MAX));
    assert(!tsfp_guest_mm_allocate(&mm,(enum tsfp_guest_mm_kind)99,1));
    assert(tsfp_guest_mm_contiguous_ex(&mm,0x200,0x6100,0x62ff,3)==PHYS_BASE+0x6100);
    assert(!tsfp_guest_mm_contiguous_ex(&mm,0x100,0x6200,0x62ff,4096));
    assert(!tsfp_guest_mm_contiguous_ex(&mm,1,0x8000,0x8000,4096));
    assert(!tsfp_guest_mm_contiguous_ex(&mm,32,0x1000,0x100f,4096));
    assert(!tsfp_guest_mm_contiguous_ex(&mm,32,PHYS_SIZE,UINT32_MAX,4096));
    assert(!tsfp_guest_mm_contiguous_ex(&mm,32,0x2000,0x1000,4096));
    assert(tsfp_guest_mm_contiguous_ex(&mm,31,0x11001,0x21000,4096)==PHYS_BASE+0x12000);
    assert(tsfp_guest_mm_size(&mm,PHYS_BASE+0x12001)==30);
    assert(!tsfp_guest_mm_size(&mm,PHYS_BASE+0x1201f));
    assert(tsfp_guest_mm_physical(&mm,PHYS_BASE+PHYS_SIZE-1)==PHYS_SIZE-1);
    assert(tsfp_guest_mm_physical(&mm,PHYS_BASE+PHYS_SIZE)==PHYS_BASE+PHYS_SIZE);
    assert(tsfp_guest_mm_physical(&mm,0x1234)==0x1234);
    tsfp_allocations_stop(&allocations);
    assert(!tsfp_guest_mm_allocate(&mm,TSFP_MM_SYSTEM,1));
}
static void statistics_checks(void) {
    const uint32_t invalid[]={0,1,RAM_SIZE-35,RAM_SIZE,UINT32_MAX-2,UINT32_MAX};
    setup();denied=0x2001+35;memcpy(saved,ram,sizeof(ram));
    assert(tsfp_guest_mm_statistics(&mm,0x2001)==ARG);
    assert(!stats_calls && !memcmp(saved,ram,sizeof(ram)));
    denied=0;
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
        assert(tsfp_guest_mm_statistics(&mm,invalid[i])==ARG);
        assert(!stats_calls && !memcmp(saved,ram,sizeof(ram)));
    }
    host_failure=1;
    assert(tsfp_guest_mm_statistics(&mm,0x2001)==UINT32_C(0xc0000001));
    assert(stats_calls==1 && !memcmp(saved,ram,sizeof(ram)));
    host_failure=0;
    assert(!tsfp_guest_mm_statistics(&mm,0x2001));
    assert(get(0x2001)==36 && get(0x2005)==16384 && get(0x2009)==8192);
    for(unsigned i=3;i<9;++i)assert(!get(0x2001+i*4));
    assert(ram[0x2000]==0x5a && ram[0x2001+36]==0x5a);
    host_bytes=4097;assert(!tsfp_guest_mm_statistics(&mm,RAM_SIZE-36));
    assert(get(RAM_SIZE-28)==1);
    assert(tsfp_guest_mm_global(&mm,0x3001)==0x3001 && get(0x3001)==0x3001);
    assert(ram[0x3000]==0x5a && ram[0x3005]==0x5a);
    memcpy(saved,ram,sizeof(ram));denied=0x3004;
    assert(!tsfp_guest_mm_global(&mm,0x3001));
    assert(!tsfp_guest_mm_global(&mm,RAM_SIZE-3));
    assert(!memcmp(saved,ram,sizeof(ram)));
}
static void gpu_checks(void) {
    setup();
    assert(tsfp_guest_mm_gpu(&mm,UINT32_MAX,0x2001)==PHYS_BASE+PHYS_SIZE-0x2000);
    assert(!get(0x2001) && ram[0x2000]==0x5a && ram[0x2005]==0x5a);
    assert(tsfp_guest_mm_gpu(&mm,0,0)==PHYS_BASE+PHYS_SIZE);
    assert(tsfp_guest_mm_gpu(&mm,PHYS_SIZE,0)==PHYS_BASE);
    assert(!allocations.heap.count && allocations.physical.count==1);
    memset(ram,0x5a,sizeof(ram));memcpy(saved,ram,sizeof(ram));
    assert(!tsfp_guest_mm_gpu(&mm,PHYS_SIZE+1,0x2001));
    assert(!memcmp(saved,ram,sizeof(ram)));
    denied=0x2004;assert(!tsfp_guest_mm_gpu(&mm,1,0x2001));
    assert(!memcmp(saved,ram,sizeof(ram)));
    assert(!tsfp_guest_mm_gpu(&mm,1,UINT32_MAX));
    mm.gpu_default=PHYS_SIZE+1;assert(!tsfp_guest_mm_gpu(&mm,UINT32_MAX,0));
    mm.physical_base=UINT32_MAX;assert(!tsfp_guest_mm_gpu(&mm,1,0));
}
static void access_checks(void) {
    setup();
    assert(!tsfp_guest_mm_query_access(&mm,0));
    assert(tsfp_guest_mm_query_access(&mm,1)==1 && !queries);
    assert(tsfp_guest_mm_query_access(&mm,0x2001)==4 && last_address==ram+0x2001);
    inaccessible=1;
    assert(tsfp_guest_mm_query_access(&mm,0x2001)==1);
    assert(tsfp_guest_mm_set_access(&mm,0x2001,17,4));
    assert(last_address==ram+0x2001 && last_bytes==17 && last_protection==4);
    host_failure=1;assert(!tsfp_guest_mm_set_access(&mm,0x2001,17,2));
    {
        unsigned before=protects;
        assert(!tsfp_guest_mm_set_access(&mm,0x2001,UINT32_MAX,4));
        assert(!tsfp_guest_mm_set_access(&mm,0,1,4));
        assert(!tsfp_guest_mm_set_access(&mm,0x2001,0,4) && protects==before);
    }
    for(uint32_t address=0;;address=UINT32_MAX) {
        assert(tsfp_guest_mm_advisory(TSFP_MM_VALID_ADDRESS,address)==1);
        assert(tsfp_guest_mm_advisory(TSFP_MM_AVAILABLE_PAGES,address)==0x10000);
        assert(tsfp_guest_mm_advisory(TSFP_MM_WRITE_CHECK,address)==address);
        assert(!tsfp_guest_mm_advisory(TSFP_MM_NOOP,address));
        if(address==UINT32_MAX)break;
    }
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"allocation"))allocation_checks();
    else if(!strcmp(argv[1],"statistics"))statistics_checks();
    else if(!strcmp(argv[1],"gpu"))gpu_checks();
    else if(!strcmp(argv[1],"access"))access_checks();
    else abort();
    puts("guest memory-service contracts passed");return 0;
}
