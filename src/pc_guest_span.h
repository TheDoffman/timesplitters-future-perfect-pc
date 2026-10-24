#ifndef TSFP_PC_GUEST_SPAN_H
#define TSFP_PC_GUEST_SPAN_H
#include "pc_guest_memory.h"
#ifndef __APPLE__
#include <psapi.h>
#endif

/* Fresh bounds for a known-size access, not an unbounded string/region scan.
 * VirtualQuery can walk a large mapped allocation to find its end. Windows'
 * resident-page query supplies current permissions without that walk. Batch
 * at most four pages, checking every page; cold/guarded pages and larger spans
 * use the existing full-region query. Never retain either snapshot across guest
 * calls or mapping/protection changes. Valid gates Win32Protection; see
 * https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-psapi_working_set_ex_block */
static inline PcGuestRegion pc_guest_query_span(uintptr_t address,size_t bytes)
{
#ifndef __APPLE__
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    if(system.dwPageSize && bytes && bytes<=UINTPTR_MAX-address) {
        uintptr_t base=address-address%system.dwPageSize;
        size_t pages=(address-base+bytes-1)/system.dwPageSize+1;
        if(pages<=4) {
            PSAPI_WORKING_SET_EX_INFORMATION info[4]={0};
            for(size_t i=0;i<pages;i++)
                info[i].VirtualAddress=(void*)(base+i*system.dwPageSize);
            if(QueryWorkingSetEx(GetCurrentProcess(),info,(DWORD)(pages*sizeof info[0]))) {
                const DWORD reads=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|
                    PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
                const DWORD writes=PAGE_READWRITE|PAGE_WRITECOPY|
                    PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
                int readable=1,writable=1;
                for(size_t i=0;i<pages;i++) {
                    DWORD protection=(DWORD)info[i].VirtualAttributes.Win32Protection;
                    if(!info[i].VirtualAttributes.Valid || info[i].VirtualAttributes.Bad ||
                       !(protection&reads) || (protection&(PAGE_GUARD|PAGE_NOACCESS))) {
                        readable=0;break;
                    }
                    writable=writable && (protection&writes)!=0;
                }
                if(readable) {
                    PcGuestRegion region={base,pages*system.dwPageSize,writable};
                    return region;
                }
            }
        }
    }
#else
    (void)bytes;
#endif
    return pc_guest_query_region(address);
}
/* Same uninterrupted-operation lifetime as pc_guest_memory_in_region, with
 * a bounded known-size query on misses. Never carry this through guest calls. */
static inline int pc_guest_memory_in_span(PcGuestRegion *region,
                                         uintptr_t address,size_t bytes,int writing)
{
    if(bytes>UINTPTR_MAX-address)return 0;
    if(bytes<=pc_guest_region_remaining(region,address,writing))return 1;
    *region=pc_guest_query_span(address,bytes);
    return bytes<=pc_guest_region_remaining(region,address,writing);
}
#endif
