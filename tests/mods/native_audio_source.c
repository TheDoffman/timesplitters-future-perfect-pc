/* Real OS memory permissions and production wrapper; original callee is doubled. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#define TSFP_MOD_AUDIO_SOURCE_IMPLEMENTATION
#include "mods/mod_audio_source.h"
#undef TSFP_MOD_AUDIO_SOURCE_IMPLEMENTATION
static unsigned char *memory;
static size_t page;
size_t g_xbox_total_ram;
static uintptr_t g_xbox_mem_offset;
static uint32_t g_esp,g_eax,expected;
static unsigned calls;
static int escape;
static jmp_buf recovery;
#include "mods/pc_mod_audio_guest.inc"
static void word(uint32_t address,uint32_t value){memcpy(memory+address,&value,4);}
static void protection(size_t offset,int mode)
{
#if defined(_WIN32)
    DWORD previous;
    assert(VirtualProtect(memory+offset,page,mode==0?PAGE_READWRITE:mode==1?PAGE_NOACCESS:PAGE_READONLY,&previous));
#else
    assert(!mprotect(memory+offset,page,mode==0?PROT_READ|PROT_WRITE:mode==1?PROT_NONE:PROT_READ));
#endif
}
static void original(void)
{
    ++calls;g_esp-=64;
    assert(tsfp_mod_audio_source_current(g_esp)==expected);
    g_eax=0xabc123u;
    if(escape)longjmp(recovery,1);
    g_esp+=68;
}
static void request(uint32_t sp,uint32_t pointer,uint32_t event,int blocked)
{
    uint32_t before=calls;
    g_esp=sp;g_eax=0;
    if(sp<g_xbox_total_ram-8)word(sp+4,pointer);
    if(pointer && pointer<=g_xbox_total_ram-4)word(pointer,event);
    if(blocked==1)protection(0,1);
    if(blocked==2)protection(page,1);
    if(blocked==3)protection(page,2);
    if(blocked==4)g_xbox_mem_offset=UINTPTR_MAX-100;
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    expected=(blocked==0 || blocked==3) && sp && pointer && pointer<=g_xbox_total_ram-4 && event<TSFP_MOD_AUDIO_EVENTS?event:TSFP_MOD_AUDIO_UNKNOWN;
#else
    expected=TSFP_MOD_AUDIO_UNKNOWN;
#endif
    pc_mod_audio_request(original);
    if(blocked==1)protection(0,0);
    if(blocked==2 || blocked==3)protection(page,0);
    if(blocked==4)g_xbox_mem_offset=(uintptr_t)memory;
    assert(calls==before+1 && g_esp==sp+4u && g_eax==0xabc123u);
    assert(tsfp_mod_audio_source_current(g_esp)==TSFP_MOD_AUDIO_UNKNOWN);
}
#if defined(_WIN32)
static DWORD WINAPI other_thread(void *unused)
#else
static void *other_thread(void *unused)
#endif
{
    unsigned token;(void)unused;
    assert(tsfp_mod_audio_source_current(2900)==TSFP_MOD_AUDIO_UNKNOWN);
    token=tsfp_mod_audio_source_enter(23,3000);
    assert(tsfp_mod_audio_source_current(2900)==23);
    tsfp_mod_audio_source_leave(token);
    return 0;
}
int main(void)
{
    unsigned outer,inner;
#if defined(_WIN32)
    {SYSTEM_INFO info;GetSystemInfo(&info);page=info.dwPageSize;}
    memory=VirtualAlloc(NULL,page*3,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(memory);
#else
    page=(size_t)sysconf(_SC_PAGESIZE);assert(page>=4096);
    memory=mmap(NULL,page*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);
#endif
    g_xbox_total_ram=page*3;
    g_xbox_mem_offset=(uintptr_t)memory;
    request(2048,257,0,0);request(2048,257,5702,0);
    request(2048,257,5703,0);request(2048,257,UINT32_MAX,0);
    request(2048,0,12,0);request(2048,(uint32_t)g_xbox_total_ram-2,12,0);
    request(2048,257,12,1);request(UINT32_MAX,0,0,0);request(0,0,0,0);
    request(2048,(uint32_t)page+257,12,2);
    request(2048,(uint32_t)page+257,12,3);
    request(2048,257,12,4);
    outer=tsfp_mod_audio_source_enter(67,2200);
    assert(tsfp_mod_audio_source_current(2100)==67);
    inner=tsfp_mod_audio_source_enter(UINT32_MAX,2000);
    assert(tsfp_mod_audio_source_current(1900)==TSFP_MOD_AUDIO_UNKNOWN);
    tsfp_mod_audio_source_leave(inner);
    assert(tsfp_mod_audio_source_current(2100)==67);
    inner=tsfp_mod_audio_source_enter(42,2000);
    assert(tsfp_mod_audio_source_current(2000)==67); /* returned/skipped frame */
    tsfp_mod_audio_source_leave(inner);
#if defined(_WIN32)
    {HANDLE thread=CreateThread(NULL,0,other_thread,NULL,0,NULL);
     assert(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);}
#else
    {pthread_t thread;assert(!pthread_create(&thread,NULL,other_thread,NULL));assert(!pthread_join(thread,NULL));}
#endif
    assert(tsfp_mod_audio_source_current(2100)==67);
    tsfp_mod_audio_source_leave(outer);
    for(unsigned i=0;i<8;i++)(void)tsfp_mod_audio_source_enter(i,2200-i*16);
    assert(tsfp_mod_audio_source_current(1900)==7);
    (void)tsfp_mod_audio_source_enter(100,1800);
    assert(tsfp_mod_audio_source_current(1700)==TSFP_MOD_AUDIO_UNKNOWN);
    tsfp_mod_audio_source_leave(0);
    assert(tsfp_mod_audio_source_current(0)==TSFP_MOD_AUDIO_UNKNOWN);
    /* A guest exception skips wrapper leave. An already-returned stack frame
       cannot lend its ID to a subsequent unrelated HLE call. */
    if(!setjmp(recovery)) {
        g_esp=2048;word(2052,257);word(257,19);escape=1;
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
        expected=19;
#else
        expected=TSFP_MOD_AUDIO_UNKNOWN;
#endif
        pc_mod_audio_request(original);assert(0);
    }
    escape=0;g_esp=2048;
    assert(tsfp_mod_audio_source_current(g_esp)==TSFP_MOD_AUDIO_UNKNOWN);
    request(2048,257,31,0);
#if defined(_WIN32)
    assert(VirtualFree(memory,0,MEM_RELEASE));
#else
    assert(!munmap(memory,page*3));
#endif
    puts("PASS: exact callee effects, guarded IDs, nested masks, overflow, unwind and thread isolation");
    return 0;
}
