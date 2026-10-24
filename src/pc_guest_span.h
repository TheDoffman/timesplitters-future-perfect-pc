#ifndef TSFP_PC_GUEST_SPAN_H
#define TSFP_PC_GUEST_SPAN_H
#include "pc_guest_memory.h"
#ifndef __APPLE__
#include <psapi.h>
#endif

/* Fresh bounds for a known-size access, not an unbounded string/region scan.
 * VirtualQuery can walk a large mapped allocation to find its end. Windows'
 * resident-page query supplies current permissions without that walk. Limit
 * the fast result to one OS page; cold/guarded pages and crossing accesses use
 * the existing full-region query. Never retain either snapshot across guest
 * calls or mapping/protection changes. Valid gates Win32Protection; see
 * https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-psapi_working_set_ex_block */
static inline PcGuestRegion pc_guest_query_span(uintptr_t address,size_t bytes)
{
#ifndef __APPLE__
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    if(system.dwPageSize && bytes && bytes<=UINTPTR_MAX-address) {
        uintptr_t base=address-address%system.dwPageSize;
        if(bytes<=system.dwPageSize-(address-base)) {
            PSAPI_WORKING_SET_EX_INFORMATION info={0};
            info.VirtualAddress=(void*)address;
            if(QueryWorkingSetEx(GetCurrentProcess(),&info,sizeof info) &&
               info.VirtualAttributes.Valid && !info.VirtualAttributes.Bad) {
                DWORD protection=(DWORD)info.VirtualAttributes.Win32Protection;
                const DWORD reads=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|
                    PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
                const DWORD writes=PAGE_READWRITE|PAGE_WRITECOPY|
                    PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
                if((protection&reads) && !(protection&(PAGE_GUARD|PAGE_NOACCESS))) {
                    PcGuestRegion region={base,system.dwPageSize,(protection&writes)!=0};
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
#endif
