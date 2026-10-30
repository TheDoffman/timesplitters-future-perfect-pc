/* Original RTL kernel imports; every critical operation shares one registry. */
#include "guest_kernel_rtl.h"
static void bridge_rtl_critical(enum tsfp_kernel_critical_action action) {
    uint32_t address=STACK_ARG(0);tsfp_guest_vm vm;enum tsfp_critical_result result;
    tsfp_kernel_rtl_site site={g_xbox_kernel_caller,g_esp,g_xbox_code_lo,g_xbox_code_hi};
    xbox_GetGuestVirtualMemory(&vm);result=tsfp_kernel_rtl_critical(&vm,address,action,&site);
    if(action!=TSFP_KERNEL_CRITICAL_TRY && result!=TSFP_CRITICAL_OK && result!=TSFP_CRITICAL_INVALID && result!=TSFP_CRITICAL_NOT_OWNER)tsfp_kernel_rtl_fault(result);
    g_eax=action==TSFP_KERNEL_CRITICAL_TRY && result==TSFP_CRITICAL_OK?1:0;
}
static void bridge_RtlInitializeCriticalSection(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_INIT);}
static void bridge_RtlEnterCriticalSection(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_ENTER);}
static void bridge_RtlLeaveCriticalSection(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_LEAVE);}
static void bridge_RtlEnterCriticalSectionAndRegion(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_ENTER);}
static void bridge_RtlLeaveCriticalSectionAndRegion(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_LEAVE);}
static void bridge_RtlTryEnterCriticalSection(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_TRY);}
static void bridge_RtlNtStatusToDosError(void){uint32_t status=STACK_ARG(0);g_eax=tsfp_kernel_rtl_error(status);}
static void bridge_RtlRip(void) {
    uint32_t api=STACK_ARG(0),expression=STACK_ARG(1),message=STACK_ARG(2);tsfp_guest_vm vm;
    xbox_GetGuestVirtualMemory(&vm);tsfp_kernel_rtl_rip(&vm,api,expression,message);g_eax=0;
}
