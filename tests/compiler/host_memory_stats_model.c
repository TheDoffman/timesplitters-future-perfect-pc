/* Controlled host snapshots execute the real adapter branch. */
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if STATS_MODEL_WINDOWS
#ifndef _WIN32
#define _WIN32 1
#endif
#else
#undef _WIN32
#ifndef __APPLE__
#define __APPLE__ 1
#endif
#endif
#include "host_memory_stats.c"

static unsigned queries;
static int fail;
static size_t page=16384;
size_t tsfp_host_page_size(void) { return page; }
#if STATS_MODEL_WINDOWS
static uint64_t available;
BOOL GlobalMemoryStatusEx(MEMORYSTATUSEX *info) {
    assert(info->dwLength==sizeof(*info));
    assert(!info->dwMemoryLoad && !info->ullTotalPhys && !info->ullAvailPhys);
    assert(!info->ullTotalPageFile && !info->ullAvailPageFile && !info->ullTotalVirtual);
    assert(!info->ullAvailVirtual && !info->ullAvailExtendedVirtual);
    ++queries;info->ullAvailPhys=available;return !fail;
}
#else
static unsigned releases;
static mach_port_t host_port=42;
static mach_msg_type_number_t reported=HOST_VM_INFO64_COUNT;
static uint32_t free_pages,inactive_pages;
mach_port_t mach_task_self(void) { return 71; }
mach_port_t mach_host_self(void) { return host_port; }
kern_return_t mach_port_deallocate(mach_port_t task,mach_port_t port) {
    assert(task==71 && port==42);++releases;return KERN_SUCCESS;
}
kern_return_t host_statistics64(mach_port_t host,int flavor,host_info64_t data,
    mach_msg_type_number_t *count) {
    vm_statistics64_data_t *info=(vm_statistics64_data_t *)data;
    assert(host==42 && flavor==HOST_VM_INFO64 && *count==HOST_VM_INFO64_COUNT);
    assert(!info->free_count && !info->inactive_count && !info->active_count);
    ++queries;info->free_count=free_pages;info->inactive_count=inactive_pages;
    *count=reported;return fail?8:KERN_SUCCESS;
}
#endif
int main(void) {
    uint64_t result=UINT64_C(0x3141592653589793),before=result;
    assert(!tsfp_host_available_memory(NULL) && !queries);
#if STATS_MODEL_WINDOWS
    available=UINT64_C(0x100000001);
    assert(tsfp_host_available_memory(&result) && result==available && queries==1);
    available=0;assert(tsfp_host_available_memory(&result) && !result);
    fail=1;available=UINT64_MAX;result=before;
    assert(!tsfp_host_available_memory(&result) && result==before);
#else
    for(unsigned host=0;host<2;++host) {
        page=host?16384:4096;
        free_pages=UINT32_MAX;inactive_pages=3;
        assert(tsfp_host_available_memory(&result));
        assert(result==(UINT64_C(0x100000002)*page) && releases==queries);
    }
    reported=3;free_pages=2;inactive_pages=3;
    assert(tsfp_host_available_memory(&result) && result==5*page);
    free_pages=0;inactive_pages=0;
    assert(tsfp_host_available_memory(&result) && !result);
    for(unsigned fault=0;fault<4;++fault) {
        page=16384;reported=HOST_VM_INFO64_COUNT;fail=0;
        free_pages=2;inactive_pages=3;result=before;
        if(fault==0)fail=1;
        if(fault==1)reported=2;
        if(fault==2)page=0;
        if(fault==3)page=SIZE_MAX/2;
        assert(!tsfp_host_available_memory(&result) && result==before && releases==queries);
    }
    {
        unsigned prior=queries;
        host_port=MACH_PORT_NULL;result=before;
        assert(!tsfp_host_available_memory(&result) && result==before && queries==prior);
    }
#endif
    puts("controlled host memory statistics contracts passed");return 0;
}
