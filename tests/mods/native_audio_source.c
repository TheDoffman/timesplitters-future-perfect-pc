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
static uint32_t expected_rate,expected_channels,expected_loop;
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
    {TsfpModAudioSource source=tsfp_mod_audio_source_snapshot(g_esp);
     assert(source.event==expected && source.authored_rate==expected_rate &&
            source.channels==expected_channels && source.looping==expected_loop);}
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
    assert(!tsfp_mod_audio_source_snapshot(2900).authored_rate);
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
    memory=VirtualAlloc(NULL,6*1024*1024,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(memory);
#else
    page=(size_t)sysconf(_SC_PAGESIZE);assert(page>=4096);
    memory=mmap(NULL,6*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);
#endif
    g_xbox_total_ram=6*1024*1024;
    g_xbox_mem_offset=(uintptr_t)memory;
    request(2048,257,0,0);request(2048,257,5702,0);
    request(2048,257,5703,0);request(2048,257,UINT32_MAX,0);
    request(2048,0,12,0);request(2048,(uint32_t)g_xbox_total_ram-2,12,0);
    request(2048,257,12,1);request(UINT32_MAX,0,0,0);request(0,0,0,0);
    request(2048,(uint32_t)page+257,12,2);
    request(2048,(uint32_t)page+257,12,3);
    request(2048,257,12,4);
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    /* Real supported-US metadata layout, independent known authored examples.
       No game assets are needed for guarded reads of these synthetic pages. */
    word(0x4ca9b4u,0x500001u);
    {uint16_t q12=1881;memcpy(memory+0x4ca9b8u+20u*2u,&q12,2);}
    memory[0x500001u+42u*20u+4u]=20;word(0x500001u+42u*20u+8u,0x210);
    expected_rate=22042;expected_channels=2;expected_loop=1;
    request(2048,257,42,0);
    protection(0x500000u,2);request(2048,257,42,0);protection(0x500000u,0);
    expected_rate=expected_channels=expected_loop=0;
    g_xbox_total_ram=0x4ca9b6u;request(2048,257,42,0);g_xbox_total_ram=6*1024*1024;
    protection(0x500000u,1);request(2048,257,42,0);protection(0x500000u,0);
    protection(0x4ca000u,1);request(2048,257,42,0);protection(0x4ca000u,0);
    memory[0x500001u+42u*20u+4u]=35;request(2048,257,42,0);
    memory[0x500001u+42u*20u+4u]=20;
    {uint16_t changed=1882;memcpy(memory+0x4ca9b8u+20u*2u,&changed,2);}
    request(2048,257,42,0); /* A changed live rate is not the pinned authored table. */
    word(0x4ca9b4u,UINT32_MAX-10u);request(2048,257,5702,0);
    word(0x4ca9b4u,0x500001u);memory[0x500001u+4u]=0;
    {uint16_t q12=500;memcpy(memory+0x4ca9b8u,&q12,2);}
    expected_rate=5859;expected_channels=1;request(2048,257,0,0);
    expected_rate=expected_channels=0;word(0x4ca9b4u,0);
#endif
    {TsfpModAudioSource source={67,16007,2,1};
     outer=tsfp_mod_audio_source_enter_metadata(&source,2200);source.authored_rate=1;
     assert(tsfp_mod_audio_source_snapshot(2100).authored_rate==16007);}
    assert(tsfp_mod_audio_source_current(2100)==67);
    inner=tsfp_mod_audio_source_enter(UINT32_MAX,2000);
    assert(tsfp_mod_audio_source_current(1900)==TSFP_MOD_AUDIO_UNKNOWN);
    assert(!tsfp_mod_audio_source_snapshot(1900).authored_rate);
    tsfp_mod_audio_source_leave(inner);
    assert(tsfp_mod_audio_source_current(2100)==67);
    assert(tsfp_mod_audio_source_snapshot(2100).authored_rate==16007);
    {const TsfpModAudioSource invalid={42,48001,1,0};
     inner=tsfp_mod_audio_source_enter_metadata(&invalid,2000);
     assert(tsfp_mod_audio_source_current(1900)==42 && !tsfp_mod_audio_source_snapshot(1900).authored_rate);
     tsfp_mod_audio_source_leave(inner);}
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
    assert(!munmap(memory,6*1024*1024));
#endif
    puts("PASS: exact callee effects, guarded IDs, nested masks, overflow, unwind and thread isolation");
    return 0;
}
