#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "pc_display_memory.h"
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(1); } } while(0)
int main(void)
{
    size_t page;
    unsigned char *memory;
#ifdef _WIN32
    SYSTEM_INFO info;GetSystemInfo(&info);page=info.dwPageSize;
    memory=VirtualAlloc(NULL,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    page=(size_t)sysconf(_SC_PAGESIZE);
    memory=mmap(NULL,page*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    if(memory==MAP_FAILED)memory=NULL;
#endif
    CHECK(memory);
    uintptr_t offset=(uintptr_t)memory-0x1000;
    PcGuestRegion region={0,0,0};
    CHECK(pc_display_writable(&region,offset,0x1000+64,64));
    CHECK(pc_display_writable(&region,offset,0x1000,64));
    CHECK(!pc_display_writable(&region,offset,0,4));
    CHECK(!pc_display_writable(&region,offset,UINT32_MAX-1,4));
    CHECK(!pc_display_writable(&region,UINTPTR_MAX-2,0x1000,4));
    /* A new correction must query again after protection changes. */
#ifdef _WIN32
    DWORD previous;
    CHECK(VirtualProtect(memory+page,page,PAGE_READONLY,&previous));
    CHECK(VirtualProtect(memory+page*2,page,PAGE_NOACCESS,&previous));
#else
    CHECK(!mprotect(memory+page,page,PROT_READ));
    CHECK(!mprotect(memory+page*2,page,PROT_NONE));
#endif
    region=(PcGuestRegion){0,0,0};
    CHECK(pc_display_writable(&region,offset,0x1000+(uint32_t)page-16,16));
    CHECK(!pc_display_writable(&region,offset,0x1000+(uint32_t)page-16,17));
    CHECK(!pc_display_writable(&region,offset,0x1000+(uint32_t)page,4));
    CHECK(!pc_display_writable(&region,offset,0x1000+(uint32_t)page*2,4));
    CHECK(pc_display_writable(&region,offset,0x1000,64));
#ifdef _WIN32
    CHECK(VirtualProtect(memory,page,PAGE_READWRITE|PAGE_GUARD,&previous));
    region=(PcGuestRegion){0,0,0};
    CHECK(!pc_display_writable(&region,offset,0x1000,4));
    CHECK(VirtualFree(memory,0,MEM_RELEASE));
#else
    CHECK(!munmap(memory,page*3));
#endif
    puts("PASS: writable bounds, readonly/noaccess pages, overflow and fresh snapshots");
    return 0;
}
