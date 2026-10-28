#ifndef TSFP_NATIVE_VM_HOST_H
#define TSFP_NATIVE_VM_HOST_H
#include "native_reservations.h"
/* Native NT flags and widths. Windows delegates to its OS virtual-memory API.
 * Darwin owns its NT-style reservations independently of the guest arena. */
void *tsfp_native_vm_allocate(void *hint,size_t bytes,uint32_t type,uint32_t protection);
int tsfp_native_vm_release(void *,size_t bytes,uint32_t type);
int tsfp_native_vm_query(const void *,tsfp_native_vm_region *);
int tsfp_native_vm_protect(void *,size_t bytes,uint32_t protection);
#endif
