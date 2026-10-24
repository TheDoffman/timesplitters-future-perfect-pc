#include <stdio.h>
#include <stdlib.h>
#include "pc_guest_memory.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);return 1;}}while(0)
static unsigned full_queries;
static PcGuestRegion counted_region(uintptr_t address)
{ ++full_queries;return pc_guest_query_region(address); }
#define pc_guest_query_region counted_region
#ifdef _WIN32
#include <psapi.h>
static int fail_page_query;
static BOOL counted_page_query(HANDLE process,PVOID data,DWORD bytes)
{ return fail_page_query?FALSE:QueryWorkingSetEx(process,data,bytes); }
#undef QueryWorkingSetEx
#define QueryWorkingSetEx counted_page_query
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "pc_guest_span.h"
#undef pc_guest_query_region
#include "pc_display_memory.h"
#ifndef GUEST_SPAN_MAPPED
#define GUEST_SPAN_MAPPED 0
#endif

static int protect(void *p,size_t bytes,int mode)
{
#ifdef _WIN32
    DWORD previous;
    return VirtualProtect(p,bytes,mode==2?PAGE_READWRITE:mode==1?PAGE_READONLY:PAGE_NOACCESS,&previous)!=0;
#else
    return !mprotect(p,bytes,mode==2?PROT_READ|PROT_WRITE:mode==1?PROT_READ:PROT_NONE);
#endif
}
int main(void)
{
#ifdef _WIN32
    SYSTEM_INFO system;GetSystemInfo(&system);size_t page=system.dwPageSize;
    HANDLE mapping=GUEST_SPAN_MAPPED?CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,(DWORD)(page*4),NULL):NULL;
    CHECK(!GUEST_SPAN_MAPPED || mapping);
    unsigned char *memory=mapping?MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,0):
        VirtualAlloc(NULL,page*4,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(memory);
#else
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *memory=mmap(NULL,page*4,PROT_READ|PROT_WRITE,(GUEST_SPAN_MAPPED?MAP_SHARED:MAP_PRIVATE)|MAP_ANON,-1,0);
    CHECK(memory!=MAP_FAILED);
#endif
    uintptr_t offset=(uintptr_t)memory-0x1000;
    for(size_t i=0;i<page*3;i+=page)memory[i]=1;
    PcGuestRegion region={0};
    CHECK(pc_display_writable(&region,offset,0x1100,64));
#ifdef _WIN32
    CHECK(full_queries==0 && region.size==page);
    /* A failed page query must retain the full-region fallback, not reject a
     * valid camera or rely on an earlier update's cached permissions. */
    fail_page_query=1;region=(PcGuestRegion){0};
    CHECK(pc_display_writable(&region,offset,0x1100,64) && full_queries==1);
    fail_page_query=0;
#endif
    region=(PcGuestRegion){0};full_queries=0;
    CHECK(pc_display_writable(&region,offset,0x1000+(uint32_t)page-16,32));
    CHECK(full_queries==1); /* Whole access needs more than one page. */
    CHECK(protect(memory+page,page,1));
    region=(PcGuestRegion){0};
    CHECK(!pc_display_writable(&region,offset,0x1000+(uint32_t)page,4));
    region=(PcGuestRegion){0};
    CHECK(!pc_display_writable(&region,offset,0x1000+(uint32_t)page-16,32));
    PcGuestRegion readable=pc_guest_query_span((uintptr_t)memory+page,4);
    CHECK(pc_guest_region_remaining(&readable,(uintptr_t)memory+page,0)>=4);
    CHECK(!pc_guest_region_remaining(&readable,(uintptr_t)memory+page,1));
    CHECK(protect(memory+page,page,0));
    region=(PcGuestRegion){0};
    CHECK(!pc_display_writable(&region,offset,0x1000+(uint32_t)page,4));
#ifdef _WIN32
    DWORD previous;
    CHECK(VirtualProtect(memory+page,page,PAGE_READWRITE|PAGE_GUARD,&previous));
    region=(PcGuestRegion){0};
    CHECK(!pc_display_writable(&region,offset,0x1000+(uint32_t)page,4));
#endif
    CHECK(protect(memory+page,page,2));
    region=(PcGuestRegion){0};
    CHECK(pc_display_writable(&region,offset,0x1000+(uint32_t)page,4));
    /* The untouched last committed page is valid even if it is not resident. */
    region=(PcGuestRegion){0};
    CHECK(pc_display_writable(&region,offset,0x1000+(uint32_t)page*3,64));
    CHECK(!pc_display_writable(&region,offset,0,4));
    CHECK(!pc_display_writable(&region,offset,UINT32_MAX-1,4));
    CHECK(!pc_display_writable(&region,UINTPTR_MAX-2,0x1000,4));
#ifdef _WIN32
    if(mapping) {CHECK(UnmapViewOfFile(memory));CHECK(CloseHandle(mapping));}
    else CHECK(VirtualFree(memory,0,MEM_RELEASE));
    region=(PcGuestRegion){0};
    CHECK(!pc_display_writable(&region,offset,0x1100,4));
#else
    CHECK(!munmap(memory,page*4));
#endif
    puts("PASS: fresh page permissions, failed/cold-query fallback, crossing spans and bounds");
    return 0;
}
