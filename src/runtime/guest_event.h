/* Original nonblocking guest dispatcher-event serialization and atomics. */
#ifndef TSFP_GUEST_EVENT_H
#define TSFP_GUEST_EVENT_H
#include <stddef.h>
#include <stdint.h>
typedef struct {void *signal;uint32_t address;unsigned synchronization;} tsfp_event_view;
/* Serialize exactly16 quiescent bytes: dispatcher Size is4 guest words, empty
 * list points to address+8. Type0 notification,1 synchronization; any nonzero
 * initial state becomes1. Byte output may be unaligned, guest address must be
 * DWORD aligned. Rejection preserves all output. This creates no host handle. */
int tsfp_event_initialize(void *,size_t,uint32_t address,uint32_t type,int initial);
/* Caller owns/pins stable full writable16-byte mapping and serializes header
 * mutation. Signal must be host/guest DWORD aligned and have state0/1. Header
 * links do not establish authority and are never followed. View is trusted host
 * metadata, not a guest capability; keep mapping alive through every operation.
 * Failure preserves output/data. No wait queues, host events or APCs supplied. */
int tsfp_event_bind(tsfp_event_view *,void *,size_t,uint32_t address);
/* State/get/set/reset return0/1, or-1 malformed view/state. Set/reset preserve
 * malformed states. Full-barrier host atomics publish preceding IOSB writes;
 * the caller supplies real wake/scheduling after set. Notification wait never
 * consumes signal; synchronization wait consumes one signal atomically. */
int tsfp_event_state(const tsfp_event_view *);
int tsfp_event_set(const tsfp_event_view *);
int tsfp_event_reset(const tsfp_event_view *);
int tsfp_event_try_wait(const tsfp_event_view *);
#endif
