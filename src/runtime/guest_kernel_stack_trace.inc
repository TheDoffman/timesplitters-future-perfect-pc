/* Original stack/assert diagnostics, checked fixed guest frames and live bounds. */
#include "guest_stack_trace.h"
#include "guest_kernel_rtl.h"
extern RECOMP_TLS uint32_t g_ebp;
static int bridge_trace_arguments(tsfp_guest_vm *vm,uint32_t *a,unsigned n) {
    uint32_t stack=g_esp,bytes=n*4;const unsigned char *p;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    p=vm->resolve(vm->host,stack,bytes,0);if(!p)return 0;
    for(unsigned i=0;i<n;++i)a[i]=(uint32_t)p[i*4]|((uint32_t)p[i*4+1]<<8)|((uint32_t)p[i*4+2]<<16)|((uint32_t)p[i*4+3]<<24);
    return 1;
}
static tsfp_guest_stack_view bridge_trace_view(void) {
    tsfp_guest_stack_view v={g_esp,g_ebp,g_xbox_kernel_caller,0,0,g_xbox_code_lo,g_xbox_code_hi};
    (void)xbox_GetGuestStackBounds(v.esp,&v.low,&v.high);return v;
}
static void bridge_RtlCaptureStackBackTrace(void) {
    tsfp_guest_vm vm;uint32_t a[4];tsfp_guest_stack_view v=bridge_trace_view();
    g_eax=bridge_trace_arguments(&vm,a,4)?tsfp_stack_trace(&vm,&v,a[0],a[1]>65535?65535:a[1],a[2],a[3]):0;
}
static void bridge_RtlWalkFrameChain(void) {
    tsfp_guest_vm vm;uint32_t a[3];tsfp_guest_stack_view v=bridge_trace_view();
    if(!bridge_trace_arguments(&vm,a,3) || (a[2]&255)){g_eax=0;return;}
    g_eax=tsfp_stack_trace(&vm,&v,a[2]>>8,a[1],a[0],0);
}
static void bridge_RtlGetCallersAddress(void) {
    tsfp_guest_vm vm;uint32_t a[2];tsfp_guest_stack_view v=bridge_trace_view();
    if(bridge_trace_arguments(&vm,a,2))(void)tsfp_stack_callers(&vm,&v,a[0],a[1]);g_eax=0;
}
static void bridge_RtlAssert(void) {
    tsfp_guest_vm vm;uint32_t a[4];
    if(bridge_trace_arguments(&vm,a,4))tsfp_kernel_rtl_assert(&vm,a[0],a[1],a[2],a[3]);g_eax=0;
}
