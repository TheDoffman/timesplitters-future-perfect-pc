#include "host_pages.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#endif

static int valid_access(uint32_t p) {
    return p==1 || p==2 || p==4 || p==8 || p==16 || p==32 || p==64;
}
#if !defined(_WIN32)
static int posix_access(uint32_t p) {
    if(p==1)return PROT_NONE;
    if(p==2)return PROT_READ;
    if(p==4 || p==8)return PROT_READ|PROT_WRITE;
    if(p==16)return PROT_EXEC;
    if(p==32)return PROT_READ|PROT_EXEC;
    return PROT_READ|PROT_WRITE|PROT_EXEC;
}
#endif
size_t tsfp_host_page_size(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    long size=sysconf(_SC_PAGESIZE);
    return size>0?(size_t)size:0;
#endif
}
void *tsfp_host_pages_map(size_t bytes,uint32_t protection) {
    size_t page=tsfp_host_page_size();
    if(!page || !bytes || bytes%page || !valid_access(protection))return NULL;
#if defined(_WIN32)
    return VirtualAlloc(NULL,bytes,MEM_RESERVE|MEM_COMMIT,(DWORD)protection);
#else
    void *p=mmap(NULL,bytes,posix_access(protection),MAP_PRIVATE|MAP_ANON,-1,0);
    return p==MAP_FAILED?NULL:p;
#endif
}
int tsfp_host_pages_unmap(void *base,size_t bytes) {
    size_t page=tsfp_host_page_size();
    if(!page || !base || !bytes || (uintptr_t)base%page || bytes%page ||
       (uintptr_t)base>UINTPTR_MAX-bytes)return 0;
#if defined(_WIN32)
    return VirtualFree(base,0,MEM_RELEASE)!=0;
#else
    return munmap(base,bytes)==0;
#endif
}
int tsfp_host_pages_protect(void *address,size_t bytes,uint32_t protection) {
    size_t page=tsfp_host_page_size(),offset,length;
    uintptr_t start=(uintptr_t)address;
    if(!page || !start || !bytes || !valid_access(protection) ||
       start>UINTPTR_MAX-bytes)return 0;
    offset=start%page;
    if(bytes>SIZE_MAX-offset)return 0;
    length=bytes+offset;
    if(length>SIZE_MAX-(page-1))return 0;
    length=((length+page-1)/page)*page;
    start-=offset;
    if(start>UINTPTR_MAX-length)return 0;
#if defined(_WIN32)
    DWORD previous;
    return VirtualProtect((void *)start,length,(DWORD)protection,&previous)!=0;
#else
    return mprotect((void *)start,length,posix_access(protection))==0;
#endif
}
int tsfp_host_pages_query(const void *address,tsfp_host_page_region *out) {
    tsfp_host_page_region result;
    uintptr_t pointer=(uintptr_t)address;
    if(!pointer || !out)return 0;
#if defined(_WIN32)
    MEMORY_BASIC_INFORMATION info;
    if(!VirtualQuery(address,&info,sizeof(info)) || info.State==MEM_FREE)return 0;
    result.base=(uintptr_t)info.BaseAddress;
    result.bytes=info.RegionSize;
    result.protection=info.Protect;
#elif defined(__APPLE__)
    mach_vm_address_t base=(mach_vm_address_t)pointer;
    mach_vm_size_t bytes=0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object=MACH_PORT_NULL;
    kern_return_t status=mach_vm_region(mach_task_self(),&base,&bytes,
        VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object);
    if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
    if(status!=KERN_SUCCESS || count<VM_REGION_BASIC_INFO_COUNT_64)return 0;
    result.base=(uintptr_t)base;result.bytes=(size_t)bytes;
    if(info.protection&VM_PROT_EXECUTE)
        result.protection=(info.protection&VM_PROT_WRITE)?64u:
            ((info.protection&VM_PROT_READ)?32u:16u);
    else result.protection=(info.protection&VM_PROT_WRITE)?4u:
        ((info.protection&VM_PROT_READ)?2u:1u);
#else
    /* Only the supported Windows and Darwin hosts provide region queries. */
    return 0;
#endif
    if(!result.bytes || pointer<result.base || pointer-result.base>=result.bytes ||
       result.base>UINTPTR_MAX-result.bytes)return 0;
    *out=result;return 1;
}
