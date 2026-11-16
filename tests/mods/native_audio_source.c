/* Actual shared attribution/wrapper; guest memory and original callee are doubles. */
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
#endif
#define TSFP_MOD_AUDIO_SOURCE_IMPLEMENTATION
#include "mods/mod_audio_source.h"
#undef TSFP_MOD_AUDIO_SOURCE_IMPLEMENTATION
static unsigned char memory[4096];
static uintptr_t g_xbox_mem_offset;
static uint32_t g_esp,g_eax,expected;
static unsigned calls;
static int denied,escape;
static jmp_buf recovery;
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
static int guest_writable(uint32_t address,uint32_t bytes)
{return !denied && address<=sizeof(memory) && bytes<=sizeof(memory)-address;}
#endif
#include "mods/pc_mod_audio_guest.inc"
static void word(uint32_t address,uint32_t value){memcpy(memory+address,&value,4);}
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
    g_esp=sp;g_eax=0;denied=blocked;
    if(sp<sizeof(memory)-8)word(sp+4,pointer);
    if(pointer && pointer<sizeof(memory)-4)word(pointer,event);
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    expected=!blocked && pointer && pointer<sizeof(memory)-4 && event<TSFP_MOD_AUDIO_EVENTS?event:TSFP_MOD_AUDIO_UNKNOWN;
#else
    expected=TSFP_MOD_AUDIO_UNKNOWN;
#endif
    pc_mod_audio_request(original);
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
    g_xbox_mem_offset=(uintptr_t)memory;
    request(2048,257,0,0);request(2048,257,5702,0);
    request(2048,257,5703,0);request(2048,257,UINT32_MAX,0);
    request(2048,0,12,0);request(2048,4094,12,0);
    request(2048,257,12,1);request(UINT32_MAX,0,0,0);request(0,0,0,0);
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
        g_esp=2048;denied=0;word(2052,257);word(257,19);escape=1;
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
    puts("PASS: exact callee effects, guarded IDs, nested masks, overflow, unwind and thread isolation");
    return 0;
}
