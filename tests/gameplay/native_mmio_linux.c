/* Real Linux SIGSEGV recovery, protected aliases and scalar x64 decoding. */
#define _GNU_SOURCE 1
#include "x64_access_cases.h"
#include "platform/win32_compat.h"
#include "apu/apu.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
ptrdiff_t g_xbox_mem_offset;
MCPXAPUState *g_apu_state;
uint64_t mcpx_apu_mmio_read(MCPXAPUState *d,uint64_t a,unsigned s){(void)d;(void)a;(void)s;abort();}
void mcpx_apu_mmio_write(MCPXAPUState *d,uint64_t a,uint64_t v,unsigned s){(void)d;(void)a;(void)v;(void)s;abort();}
extern int tsfp_native_mmio(siginfo_t *,void *);
static volatile sig_atomic_t faults;
static void handle(int sig,siginfo_t *info,void *context){if(!tsfp_native_mmio(info,context))_Exit(128+sig);faults++;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    CHECK(tsfp_x64_decoder_cases());
    void *base=w32_guest_arena_reserve(UINT64_C(0x100000000));CHECK(base);g_xbox_mem_offset=(ptrdiff_t)base;
    unsigned char *nv=w32_guest_mmio_map((unsigned char *)base+0xfd000000u,0x1000000);
    unsigned char *mc=w32_guest_mmio_map((unsigned char *)base+0xfe800000u,0x800000);CHECK(nv&&mc);
    uint32_t *status=w32_guest_mmio_alias(nv+0x600100,4);CHECK(status);*status=1;*(uint32_t *)(nv+0x100)=0x01000000u;
    DWORD old;CHECK(VirtualProtect(nv+0x600000,0x4000,PAGE_READONLY,&old));CHECK(VirtualProtect(mc+0x400000,0x4000,PAGE_READONLY,&old));
    struct sigaction sa={0};sa.sa_flags=SA_SIGINFO;sa.sa_sigaction=handle;sigemptyset(&sa.sa_mask);
    CHECK(!sigaction(SIGSEGV,&sa,NULL));CHECK(!sigaction(SIGBUS,&sa,NULL));
    *(volatile uint32_t *)(nv+0x600140)=0x12345678;
    CHECK(*(volatile uint32_t *)(nv+0x600140)==0x12345678);
    *(volatile uint32_t *)(nv+0x600100)=1;CHECK(*status==0&&*(uint32_t *)(nv+0x100)==0);
    *(volatile unsigned char *)(mc+0x40011b)=3;CHECK(*(volatile unsigned char *)(mc+0x40011b)==1);
    CHECK(faults==3);CHECK(w32_guest_arena_release());
    puts("PASS: x64 decoder, protected writes, shared CRTC acknowledgement and AC97 reset");return 0;
}
