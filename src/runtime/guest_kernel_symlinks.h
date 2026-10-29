#ifndef TSFP_GUEST_KERNEL_SYMLINKS_H
#define TSFP_GUEST_KERNEL_SYMLINKS_H
#include "guest_symlinks.h"
/* Process-lifetime shared namespace:32 bindings, same count limit as the
 * transitional table. No guest memory or caller pointer escapes these calls. */
int tsfp_kernel_symlink_create(const void *,size_t,const void *,size_t);
int tsfp_kernel_symlink_remove(const void *,size_t);
int tsfp_kernel_symlink_copy(const void *,size_t,void *,size_t,size_t *);
#endif
