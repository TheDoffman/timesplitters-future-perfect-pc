#include "guest_kernel_symlinks.h"
#include <stdlib.h>
static void *allocate(void *context,size_t n) {(void)context;return malloc(n);}
static void dispose(void *context,void *p) {(void)context;free(p);}
/* Static C initialization precedes every guest/path worker. Never reset a
 * live namespace; process exit reclaims its process-lifetime allocations. */
static tsfp_guest_symlinks links={NULL,0,32,NULL,allocate,dispose,0,0};
int tsfp_kernel_symlink_create(const void *name,size_t n,const void *target,size_t bytes) {
    return tsfp_symlinks_create(&links,name,n,target,bytes);
}
int tsfp_kernel_symlink_remove(const void *name,size_t n) {return tsfp_symlinks_remove(&links,name,n);}
int tsfp_kernel_symlink_copy(const void *name,size_t n,void *out,size_t capacity,size_t *length) {
    return tsfp_symlinks_copy(&links,name,n,out,capacity,length);
}
