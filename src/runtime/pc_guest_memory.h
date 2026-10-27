#ifndef TSFP_PC_GUEST_MEMORY_H
#define TSFP_PC_GUEST_MEMORY_H
#include <stdint.h>
#include <stddef.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#else
#include <windows.h>
#endif
/* Span that can be accessed without crossing an OS mapping/protection boundary.
 * Darwin's compatibility VirtualQuery is a stub, so use its real VM query. */
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
