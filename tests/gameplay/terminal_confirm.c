/* Execute the original terminal dispatcher and original controller-mask read.
 * Rendering, sound, scripted input and icon callbacks are controlled services. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
#include "pc_terminal_confirm_guest.h"
enum {CONSOLE=0x1000,T=0x2000,CONFIG=0x3000,CHILD=0x4000,
      ENTRY=0x5000,LINES=0x6000,DESC=0x8000,OPERATOR=0xa000,STACK=0x20000};
static unsigned event,legacy_buttons,legacy_latch,physical,network;
static void (*select_call(uint32_t site,void (*fn)(void)))(void)
{
    if(site==0x3700ec && !legacy_buttons)
        return RECOMP_DIRECT_CALLEE_AT(0x3700ec,sub_00042760);
    return fn;
}
static uint32_t select_store(uint32_t site,uint32_t address,uint32_t value)
{
    if(legacy_latch && site==0x370461)return value;
    if(site==0x370461 && address==0x76c288)
        return RECOMP_STORE_GLOBAL_U32(0x370461,0x76c288,value);
    return value;
}
#undef RECOMP_DIRECT_CALLEE_AT
#define RECOMP_DIRECT_CALLEE_AT(site,fn) select_call(site,fn)
#undef RECOMP_STORE_GLOBAL_U32
#define RECOMP_STORE_GLOBAL_U32(site,address,value) select_store(site,address,value)
static void callback(uint32_t target)
{
    assert(target==0x123456);
    event=MEM32(g_esp+12);g_esp+=4;
}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) callback(target)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) do{fprintf(stderr,"UNEXPECTED tail %x\n",(unsigned)(target));abort();}while(0)
void sub_0036D000(void){g_esp+=4;}
void sub_0036D3C0(void){g_esp+=4;}
void sub_00042970(void){g_fp_top=(g_fp_top-1u)&7u;g_fp_stack[g_fp_top]=0;g_esp+=4;}
void sub_00371FD0(void){g_eax=0;g_esp+=4;}
void sub_00045820(void){g_eax=0;g_esp+=4;}
void sub_00059D20(void){g_eax=1;g_esp+=4;}
void sub_0036C8B0(void){g_eax=0;g_esp+=4;}
void sub_00045390(void){g_eax=0;g_esp+=4;}
void sub_0036FD30(void){g_esp+=4;}
void sub_0036D5A0(void){g_esp+=4;}
void sub_0036DA80(void){g_esp+=4;}
void sub_0036DC00(void){g_esp+=4;}
void sub_0036DDA0(void){g_esp+=4;}
/* SERVICES */
/* CODE */
static void setup(int native)
{
    memset((void*)(g_xbox_mem_offset+CONSOLE),0,0xa000);
    MEM32(CONSOLE+0x7c)=T;MEM32(T+0x30)=ENTRY;
    MEM32(T+0x14)=OPERATOR;MEM32(OPERATOR+0x20)=0x80;
    MEM32(OPERATOR+0x114)=0x2000;MEM32(0x76c27c)=CONSOLE;
    MEM32(0x76c274)=CONFIG;MEM32(CONFIG)=DESC;MEM32(CONFIG+0xc)=CHILD;
    MEM32(CONFIG+0x10)=1;MEM32(CHILD+0x14)=2;MEM32(CHILD+0x28)=0x123456;
    MEM32(T+0x154)=1;MEM32(T+0x12c)=0;MEM32(T+0x40)=0xffffffff;
    MEM32(0x76c288)=0;MEM32(0x6b93c0)=physical;
    MEM32(0x6b9888)=0;MEM32(0x76b0fc)=network;
    memset((void*)(g_xbox_mem_offset+0x6b93d0),0,0x200);
    g_tsfp_native_timing_active=native;
}
static uint32_t pad(void){return (network?0x6b94c8:0x6b93d0)+physical*0x3c;}
static void step(unsigned buttons,unsigned whole,float delta)
{
    MEM32(pad()+0xc)=buttons;
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    event=0;g_fp_top=0;
    g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=CONSOLE;
    g_ebp=0x1111;g_ebx=0x2222;g_esi=0x3333;g_edi=0x4444;
    sub_00370490();
    assert(g_esp==STACK+4 && g_ebp==0x1111 && g_ebx==0x2222 && g_esi==0x3333 && g_edi==0x4444);
    assert(MEMF(0x4e794c)==delta && MEM32(0x4e7944)==whole);
}
int main(int argc,char **argv)
{
    legacy_buttons=argc>1 && !strcmp(argv[1],"buttons");
    legacy_latch=argc>1 && !strcmp(argv[1],"latch");
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x900000);assert(g_xbox_mem_offset);
    /* CONSTANTS */
    for(network=0;network<2;network++)for(physical=0;physical<(network?1u:4u);physical++) {
        for(unsigned native=0;native<2;native++) {
            setup(native);step(0,1,1);
            step(0x180,1,1);
            if(event!=3){fputs("FAIL: unrelated held input blocks terminal confirmation\n",stderr);return 1;}
            step(0x180,1,1);assert(event==2); /* held, not another press */
            step(0x100,1,1);assert(event==1); /* confirm released, other still held */
            step(0x100,1,1);assert(event==0);
            step(0x80,1,1);assert(event==3);
            step(0,1,1);assert(event==1);
            MEM32(pad()+0x38)=0x80;step(0x80,1,1);assert(event==0);
        }
        setup(1);step(0,1,1);
        /* Also covers a jittered nominal60 schedule with an extra positive
         * fractional call before its authored tick. */
        step(0x80,0,.25f);step(0x80,1,.75f);
        if(event!=3){fputs("FAIL: fractional update consumes terminal confirmation\n",stderr);return 1;}
        step(0,0,.25f);step(0,1,.75f);assert(event==1);
        setup(1);step(0,1,1);step(0x80,0,0); /* pause inhibits held input */
        step(0x80,1,1);assert(event==0);
        step(0,1,1);step(0x80,1,1);assert(event==3);
        setup(0);step(0,1,1);step(0x80,0,.25f);step(0x80,1,.75f);assert(event==0);
        const unsigned rates[]={60,120,144,180,240};
        for(unsigned slow=1;slow<=4;slow*=4)for(unsigned rate=0;rate<5;rate++) {
            unsigned fps=rates[rate],previous=0,presses=0,releases=0;
            setup(1);step(0,1,1);
            for(unsigned frame=1;frame<=fps;frame++) {
                unsigned tick=frame*60/(fps*slow),whole=tick-previous;previous=tick;
                step(frame<=fps/5?0x180:0x100,whole,60.f/(fps*slow));
                presses+=event==3;releases+=event==1;
            }
            assert(presses==1 && releases==1);
        }
        setup(0);MEM32(OPERATOR+0x20)=0x400;step(0,1,1);
        step(0x500,1,1);assert(event==3); /* configured confirm follows remap */
        step(0x100,1,1);assert(event==1);
    }
    free((void*)g_xbox_mem_offset);
    puts("PASS: original terminal confirmation, release, mapping, inhibition, fractional ticks and pause");
    return 0;
}
