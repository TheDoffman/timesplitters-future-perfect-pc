/* Execute real EBP diagnostics/imports with guarded memory and explicit guest state. */
#include "guest_stack_trace.h"
#include "guest_kernel_rtl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static unsigned char ram[4096];
#define RECOMP_TLS
uint32_t g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_esp,g_ebp,g_xbox_kernel_caller,g_xbox_code_lo,g_xbox_code_hi;
static unsigned lookups,available,frame_bytes,expect_frame,missing,bounds_calls;static uint32_t low_bound,high_bound,seen,words[4];
static uint32_t read32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void write32(uint32_t v){unsigned char p[4];for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));assert(fwrite(p,1,4,stdout)==4);}
static void *resolve(void *host,uint32_t a,uint32_t n,int write) {
    uint32_t at=a&UINT32_C(0x7fffffff);(void)host;
    if(expect_frame){expect_frame=0;assert(!write && n==frame_bytes);if(a!=g_esp || n>available)return NULL;}
    if(!at || !n || at>=sizeof(ram) || n>sizeof(ram)-at || (at<2816 && at+n>2800))return NULL;
    if(write && at<3520 && at+n>3500)return NULL;
    return ram+at;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm){++lookups;memset(vm,0,sizeof(*vm));if(!missing)vm->resolve=resolve;expect_frame=1;}
static int xbox_GetGuestStackBounds(uint32_t sp,uint32_t *low,uint32_t *high){assert(sp==g_esp);++bounds_calls;*low=low_bound;*high=high_bound;return low_bound<high_bound;}
void tsfp_kernel_rtl_assert(const tsfp_guest_vm *vm,uint32_t a,uint32_t b,uint32_t c,uint32_t d){assert(vm->resolve);++seen;words[0]=a;words[1]=b;words[2]=c;words[3]=d;}
#include "guest_kernel_stack_trace.inc"
static void invoke(unsigned op){if(op==0)bridge_RtlCaptureStackBackTrace();else if(op==1)bridge_RtlWalkFrameChain();else if(op==2)bridge_RtlGetCallersAddress();else bridge_RtlAssert();}
static void setup(unsigned op,uint32_t esp) {
    g_eax=0x12345678;g_ebx=0x65432109;g_edx=0x87654321;g_ecx=0xabcdef01;g_esi=0x12344321;g_edi=0x56788765;g_esp=esp;
    frame_bytes=available=op==0 || op==3?16:op==1?12:8;lookups=missing=expect_frame=bounds_calls=seen=0;memset(words,0,sizeof(words));
}
int main(int argc,char **argv) {
    unsigned char packet[4144],before[4096];int bridge=argc>1 && !strcmp(argv[1],"bridge");
#ifdef _WIN32
    assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
    for(unsigned op=0;op<4;++op)for(unsigned bad=0;bad<5;++bad) {
        unsigned bytes=op==0 || op==3?16:op==1?12:8;uint32_t stack=bad==0?0:bad==1?UINT32_MAX-(bytes-2):bad==4?4096:64;
        setup(op,stack);memset(ram,0xa5,sizeof(ram));memcpy(before,ram,sizeof(ram));if(bad==2)--available;if(bad==3)missing=1;
        invoke(op);assert(g_eax==0 && g_esp==stack && g_edx==0x87654321 && g_ecx==0xabcdef01 && lookups==1 && !seen && !memcmp(before,ram,sizeof(ram)));
    }
    while(fread(packet,1,sizeof(packet),stdin)==sizeof(packet)) {
        uint32_t op=read32(packet),a[4],result;tsfp_guest_stack_view v;
        assert(op<4);memcpy(&v,packet+4,sizeof(v));memcpy(a,packet+32,sizeof(a));setup(op,v.esp);
        g_ebp=v.ebp;g_xbox_kernel_caller=v.caller;low_bound=v.low;high_bound=v.high;g_xbox_code_lo=v.code_low;g_xbox_code_hi=v.code_high;memcpy(ram,packet+48,sizeof(ram));
        if(bridge){invoke(op);result=g_eax;assert(lookups==1 && !expect_frame && bounds_calls==(op==3?0u:1u));}
        else {
            tsfp_guest_vm vm;memset(&vm,0,sizeof(vm));vm.resolve=resolve;
            if(op==0)result=tsfp_stack_trace(&vm,&v,a[0],a[1]>65535?65535:a[1],a[2],a[3]);
            else if(op==1)result=(a[2]&255)?0:tsfp_stack_trace(&vm,&v,a[2]>>8,a[1],a[0],0);
            else if(op==2){(void)tsfp_stack_callers(&vm,&v,a[0],a[1]);result=0;}
            else {tsfp_kernel_rtl_assert(&vm,a[0],a[1],a[2],a[3]);result=0;}
            assert(!lookups && !bounds_calls);
        }
        assert(g_ebx==0x65432109 && g_edx==0x87654321 && g_ecx==0xabcdef01 && g_esi==0x12344321 && g_edi==0x56788765 && g_esp==v.esp && g_ebp==v.ebp);
        write32(result);write32(seen);for(unsigned i=0;i<4;++i)write32(words[i]);assert(fwrite(ram,1,sizeof(ram),stdout)==sizeof(ram));
    }
    return 0;
}
