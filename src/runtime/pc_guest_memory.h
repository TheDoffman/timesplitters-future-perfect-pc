#ifndef TSFP_PC_GUEST_MEMORY_H
#define TSFP_PC_GUEST_MEMORY_H
#include <stdint.h>
#include <stddef.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#elif defined(__linux__)
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
/* Linux 6.11 PROCMAP_QUERY UAPI. Keep the fixed ABI layout here so builds
 * using older distro headers can use a newer running kernel. No map snapshots
 * are cached: each query observes current mprotect/unmap boundaries. */
typedef struct PcLinuxMapQuery {
    uint64_t size, flags, address, start, end, permissions, page_size, offset, inode;
    uint32_t device_major, device_minor, name_size, build_id_size;
    uint64_t name_address, build_id_address;
} PcLinuxMapQuery;
#else
#include <windows.h>
#endif
/* Span that can be accessed without crossing an OS mapping/protection boundary.
 * Query the actual host mapping: Mach regions on Darwin, NT regions on Windows. */
typedef struct PcGuestRegion {
    uintptr_t base;
    size_t size;
    int writable;
} PcGuestRegion;

/* A snapshot for consecutive accesses before returning to guest code. Never
 * retain it across calls, protection changes or guest execution. */
static inline PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    PcGuestRegion region={0,0,0};
#ifdef __APPLE__
    mach_vm_address_t base=address;mach_vm_size_t size=0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object=MACH_PORT_NULL;
    kern_return_t result=mach_vm_region(mach_task_self(),&base,&size,VM_REGION_BASIC_INFO_64,
                                       (vm_region_info_t)&info,&count,&object);
    if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
    if(result!=KERN_SUCCESS || base>address || address-base>=size ||
       !(info.protection&VM_PROT_READ))return region;
    region.base=(uintptr_t)base;region.size=(size_t)size;
    region.writable=(info.protection&VM_PROT_WRITE)!=0;
#elif defined(__linux__)
    /* Query current protection boundaries; never cache across guest execution. */
    int fd=open("/proc/self/maps",O_RDONLY
#ifdef O_CLOEXEC
                |O_CLOEXEC
#endif
                );
    if(fd<0)return region;
    PcLinuxMapQuery query={0};query.size=sizeof query;query.address=address;
    int status=ioctl(fd,_IOWR('f',17,PcLinuxMapQuery),&query),error=errno;
    close(fd);
    if(!status) {
        if(query.start<=address && address<query.end && query.end<=UINTPTR_MAX &&
           (query.permissions&1)) {
            region.base=(uintptr_t)query.start;region.size=(size_t)(query.end-query.start);
            region.writable=(query.permissions&2)!=0;
        }
        return region;
    }
    /* Old kernels retain the text parser. Holes and denied queries fail closed. */
    if(error!=ENOTTY && error!=EINVAL && error!=ENOSYS && error!=EOPNOTSUPP)return region;
    FILE *maps=fopen("/proc/self/maps","r");
    if(!maps)return region;
    char line[512],permissions[5];unsigned long long lo,hi;
    while(fgets(line,sizeof line,maps)) {
        if(sscanf(line,"%llx-%llx %4s",&lo,&hi,permissions)!=3)continue;
        if(address<lo || address>=hi)continue;
        if(permissions[0]=='r') {
            region.base=(uintptr_t)lo;region.size=(size_t)(hi-lo);
            region.writable=permissions[1]=='w';
        }
        break;
    }
    fclose(maps);
#else
    MEMORY_BASIC_INFORMATION m;
    if(!VirtualQuery((void*)address,&m,sizeof m) || m.State!=MEM_COMMIT ||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
       address<(uintptr_t)m.BaseAddress || address-(uintptr_t)m.BaseAddress>=m.RegionSize)return region;
    region.base=(uintptr_t)m.BaseAddress;region.size=m.RegionSize;
    region.writable=(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;
#endif
    return region;
}
static inline size_t pc_guest_region_remaining(const PcGuestRegion *region,uintptr_t address,int writing)
{
    if((writing && !region->writable) || address<region->base ||
       address-region->base>=region->size)return 0;
    return region->size-(address-region->base);
}
static inline size_t pc_guest_region(uintptr_t address, int writing)
{
    PcGuestRegion region=pc_guest_query_region(address);
    return pc_guest_region_remaining(&region,address,writing);
}
static inline int pc_guest_memory(uintptr_t address,size_t bytes,int writing)
{
    return bytes<=UINTPTR_MAX-address && bytes<=pc_guest_region(address,writing);
}
/* Reuse a caller-owned snapshot only within one uninterrupted host operation.
 * Initialize to zero each time; never carry it through guest calls, callbacks,
 * allocations or protection changes. Reads never grant write permission. */
static inline int pc_guest_memory_in_region(PcGuestRegion *region,
                                           uintptr_t address,size_t bytes,int writing)
{
    if(bytes>UINTPTR_MAX-address)return 0;
    if(bytes<=pc_guest_region_remaining(region,address,writing))return 1;
    *region=pc_guest_query_region(address);
    return bytes<=pc_guest_region_remaining(region,address,writing);
}
#endif
