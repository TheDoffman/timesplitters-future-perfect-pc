/* Original guest ABI for shared object ownership. Shared provider. */
#ifndef TSFP_GUEST_OBJECT_SERVICES_H
#define TSFP_GUEST_OBJECT_SERVICES_H
#include "guest_objects.h"

enum tsfp_guest_object_kind {
    TSFP_KIND_UNKNOWN, TSFP_KIND_THREAD, TSFP_KIND_EVENT_MANUAL,
    TSFP_KIND_EVENT_AUTO, TSFP_KIND_MUTANT, TSFP_KIND_SEMAPHORE,
    TSFP_KIND_TIMER, TSFP_KIND_FILE, TSFP_KIND_PROCESS, TSFP_KIND_COUNT
};
#define TSFP_OBJECT_SUCCESS UINT32_C(0)
#define TSFP_OBJECT_BAD_ADDRESS UINT32_C(0xc0000005)
#define TSFP_OBJECT_BAD_HANDLE UINT32_C(0xc0000008)
#define TSFP_OBJECT_BAD_PARAMETER UINT32_C(0xc000000d)
#define TSFP_OBJECT_WRONG_TYPE UINT32_C(0xc0000024)
#define TSFP_OBJECT_NO_MEMORY UINT32_C(0xc000009a)
#define TSFP_OBJECT_HOST_FAILURE UINT32_C(0xc0000001)

typedef struct tsfp_guest_object_services {
    tsfp_guest_objects *objects;
    void *context;
    /* Guest mappings/access and caller-owned output buffers stay stable for
     * the whole call. Resolve an entire writable span without MMIO or mapping
     * side effects. Never execute guest code in these adapter callbacks. */
    void *(*resolve_output)(void *,uint32_t address,uint32_t bytes);
    /* Key for current process (-1) or current thread (-2), retained by the
     * adapter for its lifetime. No global thread-pseudo identity cache. */
    uint64_t (*current_key)(void *,uint32_t pseudo);
    /* Allocate a unique, fully initialized object body in guest storage.
     * Kind-specific layout is adapter-owned; return zero on failure. */
    uint32_t (*create_body)(void *,const tsfp_guest_object_lease *);
    /* Guaranteed disposal of an unpublished body owned by this call. This
     * is distinct from the owner's retryable published-body cleanup. */
    void (*discard_body)(void *,uint32_t address);
    /* Refresh only thread or manual event state without consuming a signal.
     * Serialize/atomically store shared state; zero indicates a host failure.
     * The lease pins the host object and body throughout this callback. */
    int (*refresh_signal)(void *,const tsfp_guest_object_lease *,uint32_t body);
    uint32_t type_id[TSFP_KIND_COUNT];
} tsfp_guest_object_services;

/* Failures preserve output bytes. A successful reference is owned by the guest
 * until dereference_body, independently of every handle's lifetime. */
uint32_t tsfp_guest_object_reference(tsfp_guest_object_services *,uint32_t handle,
    uint32_t expected_type,uint32_t output_address);
int tsfp_guest_object_reference_body(tsfp_guest_object_services *,uint32_t body);
int tsfp_guest_object_dereference_body(tsfp_guest_object_services *,uint32_t body);
uint32_t tsfp_guest_object_close(tsfp_guest_object_services *,uint32_t handle);
/* Single guest process: close-source bit 1, same-access bit 2. Access masks
 * and handle attributes must be validated by the caller's API adapter before
 * entry; this function does not model permissions or cross-process transfers. */
uint32_t tsfp_guest_object_duplicate(tsfp_guest_object_services *,uint32_t source,
    uint32_t options,uint32_t output_address);
#endif
