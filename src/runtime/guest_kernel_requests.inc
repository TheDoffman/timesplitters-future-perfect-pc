/* Original guest request allocation adapters. Include after bridge ABI/memory
 * helpers. Initialize once before guest threads; workers must be quiescent for
 * a later stop/drain. This does not install a driver or device packet queue. */
#include "guest_io_pool.h"
#if !defined(_WIN32)
#include "platform/win32_compat.h"
#endif
#define TSFP_KERNEL_REQUEST_LIMIT 1024
static tsfp_io_pool bridge_requests;
static tsfp_io_registry_entry bridge_request_entries[TSFP_KERNEL_REQUEST_LIMIT];
static CRITICAL_SECTION bridge_request_lock,bridge_request_registry_lock;
static int bridge_requests_initialized;
static void bridge_request_enter(void *context) {(void)context;EnterCriticalSection(&bridge_request_lock);}
static void bridge_request_leave(void *context) {(void)context;LeaveCriticalSection(&bridge_request_lock);}
static void bridge_request_registry_enter(void *context) {(void)context;EnterCriticalSection(&bridge_request_registry_lock);}
static void bridge_request_registry_leave(void *context) {(void)context;LeaveCriticalSection(&bridge_request_registry_lock);}
static void *bridge_request_metadata(void *context,size_t bytes) {(void)context;return calloc(1,bytes);}
static void bridge_request_metadata_free(void *context,void *p) {(void)context;free(p);}
static uint32_t bridge_request_allocate(void *context,uint32_t bytes) {(void)context;return xbox_HeapAlloc(bytes,16);}
static void *bridge_request_resolve(void *context,uint32_t address,uint32_t bytes) {
    (void)context;return bridge_object_output(NULL,address,bytes);
}
static int bridge_request_deallocate(void *context,uint32_t address) {
    tsfp_guest_vm memory;int result;
    (void)context;xbox_GetGuestVirtualMemory(&memory);
    result=tsfp_heap_release(memory.allocations,address);
    if(!result)xbox_log(XBOX_LOG_WARN,XBOX_LOG_IO,"Request allocation ownership uncertain at %08x",(unsigned)address);
    return result?1:-1;
}
static void bridge_request_init(void) {
    tsfp_io_pool_ops ops;
    if(bridge_requests_initialized)abort();
    InitializeCriticalSection(&bridge_request_lock);InitializeCriticalSection(&bridge_request_registry_lock);
    memset(&ops,0,sizeof(ops));ops.context=&bridge_requests;
    ops.lock=bridge_request_enter;ops.unlock=bridge_request_leave;
    ops.registry_lock=bridge_request_registry_enter;ops.registry_unlock=bridge_request_registry_leave;
    ops.metadata_allocate=bridge_request_metadata;ops.metadata_free=bridge_request_metadata_free;
    ops.allocate=bridge_request_allocate;ops.resolve=bridge_request_resolve;ops.deallocate=bridge_request_deallocate;
    if(!tsfp_io_pool_init(&bridge_requests,bridge_request_entries,TSFP_KERNEL_REQUEST_LIMIT,&ops))abort();
    bridge_requests_initialized=1;
}
static void bridge_IoAllocateIrp(void) {
    uint32_t stacks=(uint8_t)STACK_ARG(0);
    if(!bridge_requests_initialized)abort();
    g_eax=tsfp_io_pool_allocate(&bridge_requests,stacks);
}
static void bridge_IoFreeIrp(void) {
    uint32_t address=STACK_ARG(0);
    if(!bridge_requests_initialized)abort();
    if(!tsfp_io_registry_remove(&bridge_requests.registry,address))
        xbox_log(XBOX_LOG_WARN,XBOX_LOG_IO,"Ignoring unowned request release at %08x",(unsigned)address);
    g_eax=0;
}
static void bridge_IoInitializeIrp(void) {
    uint32_t address=STACK_ARG(0),bytes=(uint16_t)STACK_ARG(1),stacks=(uint8_t)STACK_ARG(2);
    tsfp_io_owner *owner;void *mapping;int ok=0;
    if(!bridge_requests_initialized)abort();
    owner=tsfp_io_registry_acquire(&bridge_requests.registry,address);
    mapping=bridge_object_output(NULL,address,bytes);
    if(owner) {
        /* Serialize a permitted reset with the READY -> ACTIVE transition.
         * A packet in dispatch or completion must never be reinitialized. */
        owner->ops.lock(owner->ops.context);
        if(owner->phase==TSFP_IO_READY && mapping==owner->bytes && bytes<=owner->span)
            ok=tsfp_io_request_initialize(mapping,bytes,address,bytes,stacks,TSFP_IO_REQUEST_KIND);
        owner->ops.unlock(owner->ops.context);tsfp_io_owner_release(owner);
    } else {
        /* Caller-owned packet storage is allowed; validate the full writable
         * span. The caller owns its lifetime and may not race initialization. */
        ok=tsfp_io_request_initialize(mapping,bytes,address,bytes,stacks,TSFP_IO_REQUEST_KIND);
    }
    if(!ok)xbox_log(XBOX_LOG_WARN,XBOX_LOG_IO,"Rejected request initialization at %08x",(unsigned)address);
    g_eax=0;
}
