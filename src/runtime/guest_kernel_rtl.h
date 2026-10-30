/* Native integration for original RTL services; guest records remain opaque. */
#ifndef TSFP_GUEST_KERNEL_RTL_H
#define TSFP_GUEST_KERNEL_RTL_H
#include "guest_critical.h"
#include "guest_rtl_services.h"
enum tsfp_kernel_critical_action {TSFP_KERNEL_CRITICAL_INIT,TSFP_KERNEL_CRITICAL_ENTER,
    TSFP_KERNEL_CRITICAL_LEAVE,TSFP_KERNEL_CRITICAL_TRY};
typedef struct tsfp_kernel_rtl_site {uint32_t caller,stack,code_begin,code_end;} tsfp_kernel_rtl_site;
enum tsfp_critical_result tsfp_kernel_rtl_critical(const tsfp_guest_vm *,uint32_t,
    enum tsfp_kernel_critical_action,const tsfp_kernel_rtl_site *);
uint32_t tsfp_kernel_rtl_error(uint32_t);
void tsfp_kernel_rtl_rip(const tsfp_guest_vm *,uint32_t,uint32_t,uint32_t);
/* Infrastructure failure cannot return success and run an unprotected region. */
void tsfp_kernel_rtl_fault(enum tsfp_critical_result);
#endif
