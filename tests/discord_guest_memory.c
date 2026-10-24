/* Exercise the real Discord read callback against current OS protections. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_guest_memory.h"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);exit(1);} } while(0)
static unsigned full_queries, page_queries;
static PcGuestRegion counted_region(uintptr_t address)
{ ++full_queries;return pc_guest_query_region(address); }
static int counted_memory(uintptr_t address,size_t bytes,int writing)
{ ++full_queries;return pc_guest_memory(address,bytes,writing); }
#define pc_guest_query_region counted_region
#ifdef _WIN32
#include <psapi.h>
static int fail_pages;
static BOOL counted_pages(HANDLE process,PVOID data,DWORD bytes)
{ ++page_queries;return fail_pages?FALSE:QueryWorkingSetEx(process,data,bytes); }
#undef QueryWorkingSetEx
#define QueryWorkingSetEx counted_pages
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "pc_guest_span.h"
#define pc_guest_memory counted_memory
static uintptr_t g_xbox_mem_offset;
/* PRODUCTION_READ */
static void protect(void *memory,size_t size,int mode)
{
#ifdef _WIN32
    DWORD previous;
    CHECK(VirtualProtect(memory,size,mode==2?PAGE_READWRITE:mode==1?PAGE_READONLY:PAGE_NOACCESS,&previous));
#else
    CHECK(!mprotect(memory,size,mode==2?PROT_READ|PROT_WRITE:mode==1?PROT_READ:PROT_NONE));
#endif
}
int main(void)
{
#ifdef _WIN32
    SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
    unsigned char *memory=VirtualAlloc(NULL,page*6,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(memory);
#else
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *memory=mmap(NULL,page*6,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    CHECK(memory!=MAP_FAILED);
#endif
    unsigned char *out=malloc(page*6);CHECK(out);
    for(size_t i=0;i<page*6;i++)memory[i]=(unsigned char)(i*37u+11u);
    g_xbox_mem_offset=(uintptr_t)memory-0x1000;
    /* Read bytes/words across many calls; every callback must validate afresh. */
    for(unsigned i=0;i<96;i++) {
        size_t count=i%4+1;
        unsigned before=full_queries+page_queries;
        CHECK(discord_guest_read(NULL,0x1000+i,out,count));
        CHECK(!memcmp(out,memory+i,count));
        CHECK(full_queries+page_queries==before+1);
#ifdef _WIN32
        CHECK(full_queries==0);
#endif
    }
    CHECK(discord_guest_read(NULL,0x1000+(uint32_t)page-2,out,4));
    CHECK(!memcmp(out,memory+page-2,4));
    protect(memory+page,page,1);
    CHECK(discord_guest_read(NULL,0x1000+(uint32_t)page,out,4));
    CHECK(!memcmp(out,memory+page,4));
    protect(memory+page,page,0);
    memset(out,0xA5,8);
    CHECK(!discord_guest_read(NULL,0x1000+(uint32_t)page,out,4));
    CHECK(!discord_guest_read(NULL,0x1000+(uint32_t)page-2,out,4));
    CHECK(!discord_guest_read(NULL,0,out,4));
    CHECK(!discord_guest_read(NULL,UINT32_MAX-1,out,4));
    for(unsigned i=0;i<8;i++)CHECK(out[i]==0xA5);
    protect(memory+page,page,2);
    CHECK(discord_guest_read(NULL,0x1000+(uint32_t)page,out,4));
    CHECK(!memcmp(out,memory+page,4));
    unsigned before=full_queries;
    CHECK(discord_guest_read(NULL,0x1000,out,page*5));
    CHECK(full_queries==before+1 && !memcmp(out,memory,page*5));
#ifdef _WIN32
    fail_pages=1;before=full_queries;
    CHECK(discord_guest_read(NULL,0x1000,out,4));
    CHECK(full_queries==before+1 && !memcmp(out,memory,4));
    CHECK(VirtualFree(memory,0,MEM_RELEASE));
#else
    CHECK(!munmap(memory,page*6));
#endif
    free(out);
    puts("PASS: Discord bytes, fresh bounds, read-only/noaccess, overflow and fallback");
    return 0;
}
