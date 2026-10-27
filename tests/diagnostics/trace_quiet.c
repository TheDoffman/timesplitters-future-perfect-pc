/* The complete production tracing module with observable diagnostic dispatch. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#define RECOMP_TLS _Thread_local
static unsigned char memory[65536];
static unsigned diagnostic_calls,idle_calls;
static uint32_t g_xbox_code_lo,g_xbox_code_hi;
ptrdiff_t g_xbox_mem_offset;
static uintptr_t xbox_GetMemoryOffset(void) { return (uintptr_t)memory; }
static size_t xbox_GetMappedSize(void) { return sizeof memory; }
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
/* PRODUCTION_TRACE */
#ifdef _WIN32
static void idle(void) { ++idle_calls; }
#endif
static void worker_trace(unsigned count)
{
    g_esp=512;g_ecx=1024;
    for(unsigned i=0;i<count;i++) {
        recomp_trace_enter("worker",0x10010);
        recomp_trace_exit("worker",0x10010);
    }
}
#ifdef _WIN32
static DWORD WINAPI thread_trace(void *arg) {worker_trace((unsigned)(uintptr_t)arg);return 0;}
#else
static void *thread_trace(void *arg) {worker_trace((unsigned)(uintptr_t)arg);return NULL;}
#endif
static void workers(unsigned count,unsigned calls)
{
#ifdef _WIN32
    HANDLE threads[4];
    for(unsigned i=0;i<count;i++){threads[i]=CreateThread(NULL,0,thread_trace,(void *)(uintptr_t)calls,0,NULL);assert(threads[i]);}
    for(unsigned i=0;i<count;i++){assert(WaitForSingleObject(threads[i],INFINITE)==WAIT_OBJECT_0);CloseHandle(threads[i]);}
#else
    pthread_t threads[4];
    for(unsigned i=0;i<count;i++)assert(!pthread_create(&threads[i],NULL,thread_trace,(void *)(uintptr_t)calls));
    for(unsigned i=0;i<count;i++)assert(!pthread_join(threads[i],NULL));
#endif
}
static void budget(const char *value)
{
#ifdef _WIN32
    assert(!_putenv_s("RECOMP_TRACE_BUDGET",value));
#else
    assert(!setenv("RECOMP_TRACE_BUDGET",value,1));
#endif
}
int main(int argc,char **argv)
{
    assert(argc==2);g_esp=512;g_ecx=1024;
    *(uint32_t *)(memory+256)=123;
    if(!strcmp(argv[1],"startup-cache")) {
        budget("0");recomp_trace_enter("main",0x10000);
        assert(diagnostic_calls==0);
        budget("2");recomp_trace_enter("main",0x10000);
        assert(diagnostic_calls==0); /* This thread keeps its startup cache. */
        unsigned before=diagnostic_calls;workers(1,1);
        assert(diagnostic_calls>before); /* New thread initializes independently. */
        budget("0");before=diagnostic_calls;workers(4,4096);
        assert(diagnostic_calls==before);
        puts("PASS: per-thread startup settings and concurrent quiet dispatch");return 0;
    }
    int quiet=!strcmp(argv[1],"quiet");
#ifdef _WIN32
    xbox_SetRecompIdle(idle);
#endif
    unsigned count=quiet?1024:3;
    for(unsigned n=0;n<count;n++) {
        recomp_trace_enter("alpha",0x10000);
        recomp_trace_exit("alpha",0x10000);
        if(n==0)*(uint32_t *)(memory+256)=456;
    }
    assert(quiet?diagnostic_calls==0:diagnostic_calls>0);
#ifdef _WIN32
    assert(idle_calls==(quiet?4u:0u));
    xbox_SetRecompIdle(NULL);
    for(unsigned n=0;n<256;n++)recomp_trace_enter("other",0x10004);
    assert(idle_calls==(quiet?4u:0u));
#endif
    if(!strcmp(argv[1],"profile"))assert(g_prof_calls>=3);
    puts("PASS: quiet dispatch and enabled diagnostics; idle callback preserved");
    return 0;
}
