/* Original RTL status compatibility and bounded diagnostic text services. */
#ifndef TSFP_GUEST_RTL_SERVICES_H
#define TSFP_GUEST_RTL_SERVICES_H
#include "guest_virtual_memory.h"
typedef uint32_t (*tsfp_rtl_error_fallback)(void *,uint32_t);
uint32_t tsfp_rtl_status_error(uint32_t,tsfp_rtl_error_fallback,void *);
/* Diagnostic only: never writes guest memory. 1 complete, 2 truncated ('~'),
 * 0 invalid/null -> "?". Capacity<2 returns0 without touching output. */
int tsfp_rtl_text(const tsfp_guest_vm *,uint32_t,char *,uint32_t capacity);
#endif
