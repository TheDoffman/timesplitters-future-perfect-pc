/* The complete production tracing module with observable diagnostic dispatch. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
int main(int argc,char **argv)
{
    assert(argc==2);g_esp=512;g_ecx=1024;
    *(uint32_t *)(memory+256)=123;
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
