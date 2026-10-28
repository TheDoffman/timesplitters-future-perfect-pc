/* Execute the actual POSIX/Mach branch against controlled OS responses. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#undef _WIN32
#ifndef __APPLE__
#define __APPLE__ 1
#endif
#include "host_pages.c"

static long page=4096;
static size_t last_bytes;
static uintptr_t last_address;
static int last_access,map_failed,unmap_failed,protect_failed;
static unsigned maps,unmaps,protects,queries,ports_released;
static kern_return_t query_status;
static mach_msg_type_number_t query_count=VM_REGION_BASIC_INFO_COUNT_64;
static mach_vm_address_t query_base;
static mach_vm_size_t query_bytes;
static int query_access;
static mach_port_t query_object;
static const uintptr_t raw=UINT64_C(0x123400000);

long sysconf(int name) { assert(name==_SC_PAGESIZE);return page; }
void *mmap(void *address,size_t bytes,int access,int flags,int fd,int64_t offset) {
    assert(!address && flags==(MAP_PRIVATE|MAP_ANON) && fd==-1 && offset==0);
    last_bytes=bytes;last_access=access;++maps;
    return map_failed?MAP_FAILED:(void *)raw;
}
int munmap(void *address,size_t bytes) {
    last_address=(uintptr_t)address;last_bytes=bytes;++unmaps;return unmap_failed?-1:0;
}
int mprotect(void *address,size_t bytes,int access) {
    last_address=(uintptr_t)address;last_bytes=bytes;last_access=access;++protects;
    return protect_failed?-1:0;
}
mach_port_t mach_task_self(void) { return 91; }
kern_return_t mach_port_deallocate(mach_port_t task,mach_port_t port) {
    assert(task==91 && port==37);++ports_released;return KERN_SUCCESS;
}
kern_return_t mach_vm_region(mach_port_t task,mach_vm_address_t *base,
    mach_vm_size_t *bytes,int flavor,vm_region_info_t data,
    mach_msg_type_number_t *count,mach_port_t *object) {
    vm_region_basic_info_data_64_t *info=(vm_region_basic_info_data_64_t *)data;
    assert(task==91 && *base==raw+23 && *bytes==0);
    assert(flavor==VM_REGION_BASIC_INFO_64 && *count==VM_REGION_BASIC_INFO_COUNT_64);
    assert(*object==MACH_PORT_NULL);
    *base=query_base;*bytes=query_bytes;*count=query_count;*object=query_object;
    info->protection=query_access;++queries;return query_status;
}
int main(void) {
    const uint32_t nt[]={1,2,4,8,16,32,64};
    const int access[]={PROT_NONE,PROT_READ,PROT_READ|PROT_WRITE,PROT_READ|PROT_WRITE,
        PROT_EXEC,PROT_READ|PROT_EXEC,PROT_READ|PROT_WRITE|PROT_EXEC};
    tsfp_host_page_region result,before;
    for(unsigned host=0;host<2;++host) {
        size_t p;
        page=host?16384:4096;p=(size_t)page;
        assert(tsfp_host_page_size()==p);
        for(size_t i=0;i<sizeof(nt)/sizeof(*nt);++i) {
            assert(tsfp_host_pages_map(p*2,nt[i])==(void *)raw);
            assert(last_bytes==p*2 && last_access==access[i]);
            assert(tsfp_host_pages_protect((void *)(raw+23),p,nt[i]));
            assert(last_address==raw && last_bytes==p*2 && last_access==access[i]);
        }
        assert(tsfp_host_pages_unmap((void *)raw,p*2));
        assert(last_address==raw && last_bytes==p*2);
        map_failed=1;assert(!tsfp_host_pages_map(p,4));map_failed=0;
        unmap_failed=1;assert(!tsfp_host_pages_unmap((void *)raw,p));unmap_failed=0;
        protect_failed=1;assert(!tsfp_host_pages_protect((void *)(raw+23),p,4));protect_failed=0;
        {
            unsigned m=maps,u=unmaps,v=protects;
            assert(!tsfp_host_pages_map(p-1,4));assert(!tsfp_host_pages_map(p,0));
            assert(!tsfp_host_pages_unmap((void *)(raw+1),p));
            assert(!tsfp_host_pages_unmap((void *)raw,p+1));
            assert(!tsfp_host_pages_protect((void *)(UINTPTR_MAX-2),9,4));
            assert(!tsfp_host_pages_protect((void *)(UINTPTR_MAX-p+1),1,4));
            assert(maps==m && unmaps==u && protects==v);
        }
    }
    query_base=raw;query_bytes=4096;query_object=37;
    for(int bits=0;bits<8;++bits) {
        const uint32_t expected[]={1,2,4,4,16,32,64,64};
        query_access=bits;
        assert(tsfp_host_pages_query((void *)(raw+23),&result));
        assert(result.base==raw && result.bytes==4096 && result.protection==expected[bits]);
        assert(ports_released==queries);
    }
    memset(&before,0x31,sizeof(before));
    for(unsigned failure=0;failure<6;++failure) {
        query_base=raw;query_bytes=4096;query_status=KERN_SUCCESS;
        query_count=VM_REGION_BASIC_INFO_COUNT_64;
        if(failure==0)query_base=raw+4096; /* Mach returns the NEXT region for a hole. */
        if(failure==1)query_bytes=23;
        if(failure==2)query_bytes=0;
        if(failure==3)query_status=7;
        if(failure==4)query_count=0;
        if(failure==5)query_bytes=UINT64_MAX;
        result=before;
        assert(!tsfp_host_pages_query((void *)(raw+23),&result));
        assert(!memcmp(&result,&before,sizeof(result)) && ports_released==queries);
    }
    page=-1;assert(!tsfp_host_page_size() && !tsfp_host_pages_map(4096,4));
    assert(!tsfp_host_pages_unmap((void *)raw,4096));
    assert(!tsfp_host_pages_protect((void *)raw,1,4));
    puts("controlled POSIX/Mach page contracts passed");return 0;
}
