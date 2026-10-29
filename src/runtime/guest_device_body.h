#ifndef TSFP_GUEST_DEVICE_BODY_H
#define TSFP_GUEST_DEVICE_BODY_H
#include <stddef.h>
#include <stdint.h>
enum {TSFP_DEVICE_BODY_BYTES=0x48};
/* Original explicit-byte layout constructor, not a native host struct copy.
 * Project policy: Size covers supplied storage; initial I/O references0, one
 * stack slot, empty queue, available auto-event lock, initialization/name/
 * exclusive flags. This does not prove every Xbox kernel default/behavior.
 * Caller owns stable writable storage; invalid inputs preserve all bytes.
 * Type/driver callbacks and embedded Ke-event ownership are separate services.
 * Header/public field facts: nxdk xboxkrnl.h and Cxbx common/types.h. */
int tsfp_device_body_initialize(void *,size_t span,uint32_t guest_address,
    uint32_t driver,uint32_t extension,uint32_t device_type,int named,int exclusive);
/* Requires a complete valid initialized allocation; sets only DeletePending.
 * Host ownership and exactly-once creator deletion remain factory policy. */
int tsfp_device_body_mark_deleted(void *,size_t span);
#endif
