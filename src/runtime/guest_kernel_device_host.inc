/* Original kernel host bindings for the project device/namespace providers.
 * Include after project object owner/service/scope declarations. Initialize
 * once after bridge_object_init and before guest threads. The shared patch
 * wires selection, disposal forwarding and thunk/event routes. */
#include "guest_device_body.h"
#include "guest_device_dispatch.h"
#include "guest_device_event_scope.h"
#include "guest_name_service.h"
#if !defined(_WIN32)
#include "platform/win32_compat.h"
#endif
static tsfp_guest_names bridge_device_names;
static tsfp_guest_name_service bridge_name_service;
static tsfp_device_factory bridge_devices;
static CRITICAL_SECTION bridge_device_lock;
static int bridge_devices_initialized;
static RECOMP_TLS int bridge_device_pumping;
static void bridge_device_enter(void *context) {(void)context;EnterCriticalSection(&bridge_device_lock);}
static void bridge_device_leave(void *context) {(void)context;LeaveCriticalSection(&bridge_device_lock);}
static void *bridge_device_metadata(void *context,size_t bytes) {(void)context;return calloc(1,bytes);}
static void bridge_device_metadata_free(void *context,void *p) {(void)context;free(p);}
static const void *bridge_device_input(void *context,uint32_t address,uint32_t bytes) {
    tsfp_guest_vm memory;(void)context;xbox_GetGuestVirtualMemory(&memory);
    return memory.resolve(memory.host,address,bytes,0);
}
static uint32_t bridge_device_allocate(void *context,uint32_t bytes) {(void)context;return xbox_HeapAlloc(bytes,16);}
static int bridge_device_deallocate(void *context,uint32_t body) {
    tsfp_guest_vm memory;(void)context;xbox_GetGuestVirtualMemory(&memory);
    return tsfp_heap_release(memory.allocations,body)?1:-1;
}
static int bridge_device_initialize(void *context,uint32_t body,void *mapping,uint32_t bytes,
    uint32_t driver,uint32_t extension,uint32_t type,int named,int exclusive) {
    (void)context;return tsfp_device_body_initialize(mapping,bytes,body,driver,extension,type,named,exclusive);
}
static void bridge_device_mark_deleted(void *context,uint32_t body,void *mapping,uint32_t bytes) {
    (void)context;
    if(!tsfp_device_body_mark_deleted(mapping,bytes))
        xbox_log(XBOX_LOG_WARN,XBOX_LOG_IO,"Invalid device header on deletion at %08x",(unsigned)body);
}
static int bridge_device_free_body(uint32_t body,uint32_t kind,uint32_t flags) {
    return bridge_devices_initialized?tsfp_device_factory_dispose(&bridge_devices,body,kind,flags):-1;
}
static tsfp_guest_entry_t bridge_device_lookup(void *context,uint32_t address) {
    tsfp_guest_entry_t entry;(void)context;
    entry=recomp_lookup_manual(address);return entry?entry:recomp_lookup(address);
}
static int bridge_device_pump_leave(void *context,int abandoned) {
    (void)abandoned;*(int *)context=0;return 1;
}
static void bridge_device_pump(void) {
    tsfp_guest_object_scope guard;tsfp_guest_callback_ops ops;unsigned remaining=4;
    if(!bridge_devices_initialized || bridge_device_pumping)return;
    if(!tsfp_guest_scope_enter(&bridge_object_scopes,&guard,&bridge_objects))return;
    if(!tsfp_guest_scope_on_leave(&bridge_object_scopes,&guard,bridge_device_pump_leave,&bridge_device_pumping))abort();
    bridge_device_pumping=1;ops.context=NULL;ops.lookup=bridge_device_lookup;ops.output=bridge_object_output;
    while(remaining-- && tsfp_device_dispatch_one(&bridge_devices,&bridge_object_scopes,&ops)!=TSFP_DEVICE_DISPATCH_NOT_STARTED)
        (void)tsfp_guest_objects_collect(&bridge_objects);
    if(!tsfp_guest_scope_leave(&bridge_object_scopes,&guard))abort();
}
static int bridge_device_event_begin(tsfp_device_event_scope *frame,uint32_t address) {
    return bridge_devices_initialized && tsfp_device_event_scope_begin(&bridge_object_scopes,frame,&bridge_devices,address);
}
static void bridge_device_init(void) {
    tsfp_device_factory_ops ops;void *identity;
    if(bridge_devices_initialized || !bridge_objects_initialized)abort();
    InitializeCriticalSection(&bridge_device_lock);
    if(!tsfp_guest_names_init(&bridge_device_names,&bridge_objects,4096,bridge_device_metadata,bridge_device_metadata_free,NULL))abort();
    memset(&ops,0,sizeof(ops));ops.lock=bridge_device_enter;ops.unlock=bridge_device_leave;
    ops.metadata_allocate=bridge_device_metadata;ops.metadata_free=bridge_device_metadata_free;
    ops.allocate=bridge_device_allocate;ops.deallocate=bridge_device_deallocate;
    ops.input=bridge_device_input;ops.output=bridge_object_output;
    ops.initialize=bridge_device_initialize;ops.mark_deleted=bridge_device_mark_deleted;ops.defer_finalization=1;
    if(!tsfp_device_factory_init(&bridge_devices,&bridge_objects,&bridge_device_names,TSFP_DEVICE_BODY_BYTES,256,&ops))abort();
    bridge_name_service.names=&bridge_device_names;bridge_name_service.references=&bridge_object_services;
    bridge_name_service.resolve_input=bridge_device_input;
    /* Same distinct opaque guest type identity policy as other object kinds;
     * not a complete serialized Xbox OBJECT_TYPE implementation. Export70. */
    bridge_object_services.type_id[TSFP_KIND_DEVICE]=XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE;
    identity=bridge_object_output(NULL,XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE,4);
    if(!identity)abort();
    bridge_object_word(identity,XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE);
    bridge_devices_initialized=1;
}
