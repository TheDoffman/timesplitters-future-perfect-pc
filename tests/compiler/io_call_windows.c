#include <windows.h>
#define TSFP_REAL_DEVICE_BODY
#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_call.h"
#include "guest_io_pool.h"
ptrdiff_t g_xbox_mem_offset;
static tsfp_io_pool request_pool;static tsfp_io_registry_entry registry_entries[1];
static tsfp_io_owner *active_owner;static tsfp_io_devices *active_binding;
static void *metadata_region,*binding_region;static uint32_t request_address,device_body;
static unsigned request_depth,registry_depth,metadata_freed,binding_freed,called;
static tsfp_guest_object_scopes scopes;
static void pool_enter(void *context) {CHECK(context==&request_pool && !request_depth);++request_depth;}
static void pool_leave(void *context) {CHECK(context==&request_pool && request_depth==1);--request_depth;}
static void registry_enter(void *context) {CHECK(context==&request_pool && !registry_depth);++registry_depth;}
static void registry_leave(void *context) {CHECK(context==&request_pool && registry_depth==1);--registry_depth;}
static void *pool_metadata(void *context,size_t bytes) {
    CHECK(context==&request_pool && !request_depth && !registry_depth && !metadata_region);
    metadata_region=VirtualAlloc(NULL,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);return metadata_region;
}
static void pool_metadata_free(void *context,void *mapping) {
    CHECK(context==&request_pool && mapping==metadata_region && !request_depth && !registry_depth && !metadata_freed && binding_freed==1);
    CHECK(VirtualFree(mapping,0,MEM_RELEASE));++metadata_freed;
}
static uint32_t pool_allocate(void *context,uint32_t bytes) {CHECK(context==&request_pool);return allocate(&factory,bytes);}
static int pool_deallocate(void *context,uint32_t address) {CHECK(context==&request_pool && !request_depth && !registry_depth);++frees;CHECK(tsfp_heap_release(&memory,address));return 1;}
static void *pool_resolve(void *context,uint32_t address,uint32_t bytes) {CHECK(context==&request_pool);return output(&factory,address,bytes);}
static void binding_retired(void *context,tsfp_io_devices *owned,int clean) {
    CHECK(context==&request_pool && owned==active_binding && clean && !binding_freed && !metadata_freed && !request_depth && !registry_depth);
    active_binding=NULL;CHECK(VirtualFree(owned,0,MEM_RELEASE));++binding_freed;
}
static tsfp_guest_entry_t lookup_driver(void *,uint32_t);
static void *callback_output(void *,uint32_t,uint32_t);
static const void *driver_input(void *,uint32_t,uint32_t);
static void driver_entry(void) {
    uint32_t sp=g_esp,child_status=0xfeedface;unsigned location=active_owner->bytes[0x19];
    CHECK(!request_depth && !registry_depth && !depth && MEM32(sp)==0);
    CHECK(MEM32(sp+4)==device_body && MEM32(sp+8)==request_address && active_owner->clients==2);
    ++called;
    if(location==2) {
        CHECK(sp==0x8ffb && active_owner->internal==2);
        CHECK(tsfp_device_factory_delete(&factory,device_body));
        CHECK(tsfp_io_registry_remove(&request_pool.registry,request_address));CHECK(active_owner->clients==1);
        CHECK(tsfp_io_owner_retain(active_owner));
        {
            tsfp_io_call_ops call_ops={{&request_pool,lookup_driver,callback_output},&request_pool,driver_input};
            CHECK(tsfp_io_call_driver(&scopes,active_owner,active_binding,device_body,&call_ops,&child_status)==TSFP_IO_CALL_RETURNED && child_status==0x55);
        }
        CHECK(called==2 && active_owner->clients==1 && active_owner->internal==1 && active_owner->phase==TSFP_IO_COMPLETE && active_binding->count==1 && g_esp==sp);
    } else {
        CHECK(location==1 && sp==0x8fef && active_owner->internal==3);
        CHECK(tsfp_io_owner_complete(active_owner,0,17,NULL,NULL));
    }
    CHECK(!metadata_freed && !binding_freed && !frees);
    g_eax=0x55;g_esp=sp+12;
}
static tsfp_guest_entry_t lookup_driver(void *context,uint32_t address) {CHECK(context==&request_pool);return address==0x4444?driver_entry:NULL;}
static void *callback_output(void *context,uint32_t address,uint32_t bytes) {CHECK(context==&request_pool && bytes==12);return address>sizeof(ram)-bytes?NULL:ram+address;}
static const void *driver_input(void *context,uint32_t address,uint32_t bytes) {CHECK(context==&request_pool && address==0x201 && bytes==68);return ram+address;}
int main(void) {
    tsfp_io_pool_ops ops;tsfp_io_call_ops call_ops={{&request_pool,lookup_driver,callback_output},&request_pool,driver_input};
    tsfp_io_device_entry *device_entries;uint32_t status=0xfeedface;MEMORY_BASIC_INFORMATION page;size_t live,retained_count,uncertain;
    init();g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x9007;g_eax=0x1111;
    CHECK(create(0,0)==TSFP_DEVICE_OK);device_body=value(ram+0x801);memset(ram+0x201,0,68);put(ram+0x201+12+4*2,0x4444);
    memset(&ops,0,sizeof(ops));ops.context=&request_pool;ops.lock=pool_enter;ops.unlock=pool_leave;ops.registry_lock=registry_enter;ops.registry_unlock=registry_leave;
    ops.metadata_allocate=pool_metadata;ops.metadata_free=pool_metadata_free;ops.allocate=pool_allocate;ops.deallocate=pool_deallocate;ops.resolve=pool_resolve;
    CHECK(tsfp_io_pool_init(&request_pool,registry_entries,1,&ops));request_address=tsfp_io_pool_allocate(&request_pool,2);CHECK(request_address);
    active_owner=tsfp_io_registry_acquire(&request_pool.registry,request_address);CHECK(active_owner);active_owner->bytes[104+24]=active_owner->bytes[104]=2;
    binding_region=VirtualAlloc(NULL,sizeof(tsfp_io_devices)+2*sizeof(tsfp_io_device_entry),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(binding_region);
    active_binding=(tsfp_io_devices *)binding_region;device_entries=(tsfp_io_device_entry *)(active_binding+1);
    CHECK(tsfp_io_devices_init(active_binding,active_owner,&factory,device_entries,2,binding_retired,&request_pool));
    CHECK(tsfp_io_call_driver(&scopes,active_owner,active_binding,device_body,&call_ops,&status)==TSFP_IO_CALL_RETURNED);
    /* Both pointed-to control blocks are now unmapped. Compare saved addresses
     * through VirtualQuery, never read the reclaimed owner/binding again. */
    CHECK(status==0x55 && g_eax==0x1111 && g_esp==0x9007 && called==2 && metadata_freed==1 && binding_freed==1 && !scopes.current);
    CHECK(VirtualQuery(metadata_region,&page,sizeof(page)) && page.State==MEM_FREE);
    CHECK(VirtualQuery(binding_region,&page,sizeof(page)) && page.State==MEM_FREE);
    tsfp_io_pool_counts(&request_pool,&live,&retained_count,&uncertain);CHECK(!live && !retained_count && !uncertain);
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);CHECK(frees==2 && !memory.heap.count);
    tsfp_io_pool_stop(&request_pool);
    puts("PASS: real Windows request/binding metadata unmap at nested inline completion without stale accesses or hidden references");return 0;
}
