#include "host_memory_stats.h"
#include "host_pages.h"
#include <stddef.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/vm_statistics.h>
#endif

int tsfp_host_available_memory(uint64_t *bytes) {
    uint64_t result;
    if(!bytes)return 0;
#if defined(_WIN32)
    MEMORYSTATUSEX info={0};
    info.dwLength=sizeof(info);
    if(!GlobalMemoryStatusEx(&info))return 0;
    result=(uint64_t)info.ullAvailPhys;
#elif defined(__APPLE__)
    vm_statistics64_data_t info={0};
    mach_msg_type_number_t count=HOST_VM_INFO64_COUNT;
    mach_port_t host=mach_host_self();
    kern_return_t status;
    size_t page=tsfp_host_page_size();
    /* Older kernels may return an earlier record revision. Only require the
     * prefix containing the two counters we actually read. */
    const size_t needed=(offsetof(vm_statistics64_data_t,inactive_count)+
        sizeof(info.inactive_count)+sizeof(integer_t)-1)/sizeof(integer_t);
    if(host==MACH_PORT_NULL)return 0;
    status=host_statistics64(host,HOST_VM_INFO64,(host_info64_t)&info,&count);
    (void)mach_port_deallocate(mach_task_self(),host);
    if(status!=KERN_SUCCESS || count<needed || !page)return 0;
    result=(uint64_t)info.free_count+(uint64_t)info.inactive_count;
    if(result>UINT64_MAX/page)return 0;
    result*=page;
#else
    /* The supported native hosts are Windows and Darwin. */
    return 0;
#endif
    *bytes=result;return 1;
}
