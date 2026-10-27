/* Production camera guard with real multi-page permissions and fresh snapshots. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_guest_memory.h"
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
static unsigned full_queries;
static PcGuestRegion counted_region(uintptr_t address)
{ ++full_queries;return pc_guest_query_region(address); }
#define pc_guest_query_region counted_region
#include "pc_guest_span.h"
#undef pc_guest_query_region
#define TSFP_PC_CONTROLS_H
#define TSFP_PC_PROMPTS_H
static void pc_controls_get_mouse(double *s,int *on,int *invert)
{ *s=.1;*on=1;*invert=0; }
static int pc_prompts_keyboard_active(void) { return 1; }
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_eax,g_esi,g_esp;
#include "pc_auto_look_guest.inc"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);exit(1);}}while(0)
static unsigned char *memory;
static uint32_t settings;
static unsigned expected_flag,calls;
static void word(uint32_t address,uint32_t value)
{ memcpy(memory+address,&value,sizeof value); }
static void original(void)
{ CHECK((memory[settings+2]&8u)==expected_flag);++calls;g_esp+=4; }
static void protect(size_t address,size_t size,int mode)
{
#ifdef _WIN32
    DWORD previous;
    CHECK(VirtualProtect(memory+address,size,
          mode==2?PAGE_READWRITE:mode==1?PAGE_READONLY:PAGE_NOACCESS,&previous));
#else
    CHECK(!mprotect(memory+address,size,mode==2?PROT_READ|PROT_WRITE:mode==1?PROT_READ:PROT_NONE));
#endif
}
int main(void)
{
    /* The existing companion fixture exercises these menu entry points. */
    (void)pc_auto_look_choice;(void)pc_auto_look_settings;
    const size_t size=0x800000;
#ifdef _WIN32
    SYSTEM_INFO system;GetSystemInfo(&system);size_t page=system.dwPageSize;
    memory=VirtualAlloc(NULL,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(memory);
#else
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    memory=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    CHECK(memory!=MAP_FAILED);
#endif
    CHECK(page>=4096 && page*9<0x7b0000);
    uint32_t player=(uint32_t)(page*2-32);
    settings=(uint32_t)(page*8+64);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    memset(memory+player,0,0x1504); /* Every page of the full record is resident. */
    word(0x7b0c7c,player);word(player+4,0);word(player+0x1500,settings);
    memory[settings+2]=0x1f;expected_flag=0;
    pc_auto_look_camera(original);
    CHECK(calls==1 && g_esp==4 && memory[settings+2]==0x1f);
#ifdef _WIN32
    CHECK(full_queries==0); /* Real full-record check uses the bounded page batch. */
#endif
    /* An inaccessible intervening record page must reject the full extent,
     * even when the local-slot word itself is still readable. */
    expected_flag=8;protect(page*2,page,0);
    pc_auto_look_camera(original);CHECK(memory[settings+2]==0x1f);
    protect(page*2,page,2);memory[page*2]=0;
    expected_flag=0;pc_auto_look_camera(original);CHECK(memory[settings+2]==0x1f);
    /* Read-only player records are valid; settings still require write access. */
    protect(page,page*3,1);pc_auto_look_camera(original);
    expected_flag=8;protect(page*8,page,1);
    pc_auto_look_camera(original);CHECK(memory[settings+2]==0x1f);
    protect(page*8,page,2);expected_flag=0;pc_auto_look_camera(original);
    protect(page,page*3,2);
    word(player+4,1);expected_flag=8;pc_auto_look_camera(original);
    word(player+4,0);expected_flag=0;pc_auto_look_camera(original);
    CHECK(calls==8 && g_esp==32 && memory[settings+2]==0x1f);
#ifdef _WIN32
    CHECK(VirtualFree(memory,0,MEM_RELEASE));
#else
    CHECK(!munmap(memory,size));
#endif
    puts("PASS: automatic lookahead full-record page checks, permissions and expiry");
    return 0;
}
