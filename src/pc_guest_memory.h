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
static inline size_t pc_guest_region(uintptr_t address, int writing)
{
#ifdef __APPLE__
    mach_vm_address_t base=address;mach_vm_size_t size=0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object=MACH_PORT_NULL;
    kern_return_t result=mach_vm_region(mach_task_self(),&base,&size,VM_REGION_BASIC_INFO_64,
                                       (vm_region_info_t)&info,&count,&object);
    if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
    if(result!=KERN_SUCCESS || base>address || address-base>=size ||
       !(info.protection&VM_PROT_READ) || (writing && !(info.protection&VM_PROT_WRITE)))return 0;
    return (size_t)(size-(address-base));
#else
    MEMORY_BASIC_INFORMATION m;
    if(!VirtualQuery((void*)address,&m,sizeof m) || m.State!=MEM_COMMIT ||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
    if(writing && !(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return 0;
    return address>=(uintptr_t)m.BaseAddress && address-(uintptr_t)m.BaseAddress<m.RegionSize?
        m.RegionSize-(address-(uintptr_t)m.BaseAddress):0;
#endif
}
static inline int pc_guest_memory(uintptr_t address,size_t bytes,int writing)
{
    return bytes<=UINTPTR_MAX-address && bytes<=pc_guest_region(address,writing);
}
#endif
