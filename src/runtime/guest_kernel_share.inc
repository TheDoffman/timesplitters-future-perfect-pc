/* Original guest-sharing ABI. Caller holds the guest execution lock and keeps
 * mappings alive. Share groups need caller serialization across host threads. */
#include "guest_io_share.h"
static void bridge_IoCheckShareAccess(void) {
    uint32_t access=STACK_ARG(0),sharing=STACK_ARG(1),file=STACK_ARG(2),group=STACK_ARG(3);
    int update=(uint8_t)STACK_ARG(4)!=0;
    void *f=bridge_object_output(NULL,file,3),*s=bridge_object_output(NULL,group,7);
    g_eax=tsfp_io_share_check(f,3,s,7,access,sharing,update);
}
static void bridge_IoSetShareAccess(void) {
    uint32_t access=STACK_ARG(0),sharing=STACK_ARG(1),file=STACK_ARG(2),group=STACK_ARG(3);
    void *f=bridge_object_output(NULL,file,3),*s=bridge_object_output(NULL,group,7);
    (void)tsfp_io_share_set(f,3,s,7,access,sharing);
}
static void bridge_IoRemoveShareAccess(void) {
    uint32_t file=STACK_ARG(0),group=STACK_ARG(1);
    void *f=bridge_object_output(NULL,file,3),*s=bridge_object_output(NULL,group,7);
    (void)tsfp_io_share_remove(f,3,s,7);
}
