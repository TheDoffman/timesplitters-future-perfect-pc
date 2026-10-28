#include "guest_map_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
typedef int BOOL;typedef uint32_t DWORD;typedef int32_t LONG;typedef void *HANDLE;
#define TRUE 1
#define FALSE 0
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define XBOX_CONTIG_SIZE 0x20000u
#define XBOX_NUM_MIRRORS 28
static void *g_memory_base,*g_contig_memory,*g_kernel_memory,*g_nv2a_memory,*g_mcpx_memory,*g_flash_memory,*g_tiled_view,*g_mcpx_regs;
static void *g_mirror_views[28];
static size_t g_memory_size,g_xbox_map_size,g_xbox_total_ram=0x100000;
static ptrdiff_t g_memory_offset,g_xbox_mem_offset;
static tsfp_guest_map_layout g_host_layout;
static tsfp_guest_host_session g_host_session;
static int g_guest_threads,g_guest_allocations;
static HANDLE g_nv2a_ack_thread,g_nv2a_ack_wake;
static volatile LONG g_nv2a_ack_stop,g_nv2a_ack_notifiers;
static unsigned stops,closes,signals,waits,ticks;
static DWORD tick=UINT32_MAX-500,wait_result=WAIT_OBJECT_0;
static int xbox_EnvSwitch(const char *name,int fallback) { (void)name;(void)fallback;return 1; }
static void tsfp_thread_memory_stop(int *p) { CHECK(p==&g_guest_threads);++stops; }
static void tsfp_allocations_stop(int *p) { CHECK(p==&g_guest_allocations);++stops; }
static DWORD model_tick(void) { tick+=100;++ticks;return tick; }
static LONG model_exchange(volatile LONG *p,LONG n) { LONG old=*p;*p=n;return old; }
static LONG model_compare(volatile LONG *p,LONG desired,LONG expected) { LONG old=*p;if(old==expected)*p=desired;return old; }
static void model_sleep(DWORD ms) { CHECK(ms==0); }
static BOOL model_signal(HANDLE h) { CHECK(h==g_nv2a_ack_wake);++signals;return TRUE; }
static DWORD model_wait(HANDLE h,DWORD timeout) { CHECK(h==g_nv2a_ack_thread && timeout==1000);++waits;return wait_result; }
static BOOL model_close(HANDLE h) { CHECK(h==g_nv2a_ack_thread || h==g_nv2a_ack_wake);++closes;return TRUE; }
#define GetTickCount model_tick
#define InterlockedExchange model_exchange
#define InterlockedCompareExchange model_compare
#define Sleep model_sleep
#define SetEvent model_signal
#define WaitForSingleObject model_wait
#define CloseHandle model_close
void xbox_MemoryLayoutShutdown(void);
#include "guest_layout_binding.inc"
int main(int argc,char **argv) {
    int mode=argc>1?atoi(argv[1]):0;
    void *base;
    CHECK(xbox_MapHostLayout());base=g_memory_base;
    CHECK(base && g_contig_memory && g_tiled_view && g_host_layout.ready);
    CHECK(g_kernel_memory==(unsigned char *)g_contig_memory+0x10000);
    CHECK(g_host_layout.tiled_bytes==XBOX_CONTIG_SIZE);
    CHECK(xbox_TiledApertureSize()==XBOX_CONTIG_SIZE);
    CHECK(!*(uint32_t *)((unsigned char *)g_contig_memory+0x1000)); /* alias probe restored bytes */
    if(mode==0) {
        xbox_MemoryLayoutShutdown();
        CHECK(!g_host_layout.started && !g_memory_base && !g_contig_memory && !g_kernel_memory);
        CHECK(!g_nv2a_memory && !g_mcpx_memory && !g_tiled_view && !g_memory_offset && !g_xbox_mem_offset);
        CHECK(stops==2 && !closes && !waits);
        CHECK(xbox_MapHostLayout());xbox_MemoryLayoutShutdown();CHECK(stops==4);
    } else {
        CHECK(tsfp_guest_map_publish(&g_host_layout));
        g_nv2a_ack_thread=(void *)(uintptr_t)1;g_nv2a_ack_wake=(void *)(uintptr_t)2;
        wait_result=WAIT_TIMEOUT;
        if(mode==2)g_nv2a_ack_notifiers=1;
        xbox_MemoryLayoutShutdown();
        CHECK(g_memory_base==base && g_host_layout.published && g_host_layout.started);
        CHECK(!closes && g_nv2a_ack_thread && g_nv2a_ack_wake && g_nv2a_ack_stop==1);
        if(mode==2)CHECK(!waits && !signals && ticks>=11 && ticks<=12);
        else CHECK(waits==1 && signals==1);
        *(volatile uint32_t *)g_memory_base=0x55667788;
        CHECK(*(volatile uint32_t *)g_memory_base==0x55667788);
        g_nv2a_ack_notifiers=0;wait_result=WAIT_OBJECT_0;
        xbox_MemoryLayoutShutdown();
        CHECK(closes==2 && !g_nv2a_ack_thread && !g_nv2a_ack_wake);
        CHECK(g_memory_base==base && g_host_layout.published && !g_host_layout.ready);
        CHECK(*(volatile uint32_t *)g_memory_base==0x55667788);
    }
    puts("PASS: actual layout binding, alias verification, bounded worker wait and published map retention");return 0;
}
