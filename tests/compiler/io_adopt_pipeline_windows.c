#include <windows.h>
#define TSFP_REAL_DEVICE_BODY
#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_call.h"
#include "guest_io_pool.h"
#include "guest_io_build.h"
#include "guest_io_complete.h"
#include "guest_event.h"
ptrdiff_t g_xbox_mem_offset;
static tsfp_io_pool request_pool;static tsfp_io_registry_entry registry_entries[1];
static tsfp_io_owner *active_owner;static tsfp_io_devices *active_binding;
static void *metadata_region,*binding_region;static uint32_t request_address,device_body;
static unsigned request_depth,registry_depth,metadata_freed,binding_freed,called;
static tsfp_guest_object_scopes scopes;
static HANDLE completion_event,event_pin;
static unsigned prepared,notified,released;
static unsigned char caller_final[152];
static tsfp_event_view guest_event;
static int prepare_delivery(void *context,const tsfp_io_result *result,tsfp_io_delivery *delivery) {
    CHECK(context==&request_pool && !request_depth && !registry_depth && !depth && !prepared);
    CHECK(result->address==request_address && result->status==0 && result->information==17);
    CHECK(result->flags==0x14 && result->user_iosb==0x8011 && result->user_event==0x7000 && !result->thread && !result->original_file);
    CHECK(MEM32(0x8011)==0xa7a7a7a7 && MEM32(0x8015)==0xa7a7a7a7);
    CHECK(guest_event.address==result->user_event && tsfp_event_state(&guest_event)==0);
    CHECK(DuplicateHandle(GetCurrentProcess(),completion_event,GetCurrentProcess(),&event_pin,0,FALSE,DUPLICATE_SAME_ACCESS));
    ++prepared;delivery->cookie=event_pin;delivery->iosb=ram+0x8011;delivery->iosb_bytes=8;return 1;
}
static int notify_delivery(void *context,void *cookie,const tsfp_io_result *result) {
    CHECK(context==&request_pool && cookie==event_pin && !request_depth && !registry_depth && !depth && !notified);
    CHECK(result->information==17 && MEM32(0x8011)==0 && MEM32(0x8015)==17 && active_owner->phase==TSFP_IO_COMPLETING);
    ++notified;CHECK(tsfp_event_set(&guest_event)==0);CHECK(SetEvent(event_pin));
    CHECK(WaitForSingleObject(completion_event,0)==WAIT_OBJECT_0);return 1;
}
static int release_delivery(void *context,void *cookie,int disposition) {
    CHECK(context==&request_pool && cookie==event_pin && disposition==TSFP_IO_DELIVERY_DONE && !released && !request_depth && !registry_depth && !depth);
    CHECK(active_owner->phase==TSFP_IO_COMPLETE && active_owner->clients==4 && active_owner->internal==2);
    CHECK(CloseHandle(event_pin));event_pin=NULL;++released;
    return tsfp_io_registry_remove(&request_pool.registry,request_address);
}
static int prepare_request(void *context,tsfp_io_owner *owner);
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
    CHECK(MEM32(sp+4)==device_body && MEM32(sp+8)==request_address && active_owner->clients==(location==2?2u:3u));
    CHECK(MEM32(MEM32(request_address+0x5c)+4)==0x100 && MEM32(MEM32(request_address+0x5c)+8)==0);
    CHECK(MEM32(MEM32(request_address+0x5c)+12)==0x55667788 && MEM32(MEM32(request_address+0x5c)+16)==0x11223344);
    CHECK(MEM32(MEM32(request_address+0x5c)+20)==device_body);
    ++called;
    if(location==2) {
        CHECK(sp==0x8ffb && active_owner->internal==2);
        CHECK(tsfp_device_factory_delete(&factory,device_body));
        /* A forwarding driver constructs its lower slot; the composer itself
         * preserves payload. Keep the registry ref until completion delivery. */
        memcpy(active_owner->bytes+104,active_owner->bytes+128,24);
        CHECK(tsfp_io_owner_retain(active_owner));
        {
            tsfp_io_call_ops call_ops={{&request_pool,lookup_driver,callback_output},&request_pool,driver_input};
            CHECK(tsfp_io_call_driver(&scopes,active_owner,active_binding,device_body,&call_ops,&child_status)==TSFP_IO_CALL_RETURNED && child_status==0x55);
        }
        CHECK(called==2 && active_owner->clients==1 && active_owner->internal==1 && active_owner->phase==TSFP_IO_COMPLETE && active_binding->count==1 && g_esp==sp);
    } else {
        CHECK(location==1 && sp==0x8fef && active_owner->internal==3);
        tsfp_io_complete_ops completion={&request_pool,prepare_delivery,notify_delivery,release_delivery};
        put(active_owner->bytes+0x10,0);put(active_owner->bytes+0x14,17);
        CHECK(tsfp_io_owner_retain(active_owner));
        CHECK(tsfp_io_complete_request(&scopes,&objects,active_owner,&completion)==TSFP_IO_COMPLETE_DONE);
    }
    CHECK(!metadata_freed && !binding_freed && !frees);
    memcpy(caller_final,ram+request_address,152);
    g_eax=0x55;g_esp=sp+12;
}
static tsfp_guest_entry_t lookup_driver(void *context,uint32_t address) {CHECK(context==&request_pool);return address==0x4444?driver_entry:NULL;}
static void *callback_output(void *context,uint32_t address,uint32_t bytes) {CHECK(context==&request_pool && bytes==12);return address>sizeof(ram)-bytes?NULL:ram+address;}
static const void *driver_input(void *context,uint32_t address,uint32_t bytes) {CHECK(context==&request_pool && address==0x201 && bytes==68);return ram+address;}
static int prepare_request(void *context,tsfp_io_owner *owner) {
    tsfp_io_device_entry *device_entries;
    CHECK(context==&request_pool && !request_depth && !registry_depth && !depth);
    CHECK(!tsfp_io_registry_acquire(&request_pool.registry,owner->address));
    binding_region=VirtualAlloc(NULL,sizeof(tsfp_io_devices)+2*sizeof(tsfp_io_device_entry),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(binding_region);
    active_binding=(tsfp_io_devices *)binding_region;device_entries=(tsfp_io_device_entry *)(active_binding+1);
    return tsfp_io_devices_init(active_binding,owner,&factory,device_entries,2,binding_retired,&request_pool);
}
int main(void) {
    tsfp_io_pool_ops ops;tsfp_io_call_ops call_ops={{&request_pool,lookup_driver,callback_output},&request_pool,driver_input};
    uint32_t status=0xfeedface;MEMORY_BASIC_INFORMATION page;size_t live,retained_count,uncertain;
    tsfp_io_build_plan plan={0};unsigned char initial[152];
    init();g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x9007;g_eax=0x1111;
    memset(ram+0x8011,0xa7,8);completion_event=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(completion_event);
    CHECK(tsfp_event_initialize(ram+0x7000,16,0x7000,0,0));CHECK(tsfp_event_bind(&guest_event,ram+0x7000,16,0x7000));
    CHECK(create(0,0)==TSFP_DEVICE_OK);device_body=value(ram+0x801);memset(ram+0x201,0,68);put(ram+0x201+12+4*2,0x4444);
    memset(&ops,0,sizeof(ops));ops.context=&request_pool;ops.lock=pool_enter;ops.unlock=pool_leave;ops.registry_lock=registry_enter;ops.registry_unlock=registry_leave;
    ops.metadata_allocate=pool_metadata;ops.metadata_free=pool_metadata_free;ops.allocate=pool_allocate;ops.deallocate=pool_deallocate;ops.resolve=pool_resolve;
    CHECK(tsfp_io_pool_init(&request_pool,registry_entries,1,&ops));
    request_address=allocate(&factory,152);CHECK(request_address);
    plan.transfer.major=2;plan.transfer.length=0x100;plan.transfer.device=device_body;
    plan.transfer.byte_offset=UINT64_C(0x1122334455667788);plan.flags=0x14;
    plan.user_buffer=0x5000;plan.user_iosb=0x8011;plan.user_event=0x7000;
    CHECK(tsfp_io_build_packet(ram+request_address,152,request_address,152,2,&plan));
    memcpy(initial,ram+request_address,152);
    CHECK(tsfp_io_pool_adopt(&request_pool,request_address,152,prepare_request,&request_pool));
    CHECK(!memcmp(initial,ram+request_address,152));
    active_owner=tsfp_io_registry_acquire(&request_pool.registry,request_address);CHECK(active_owner);

    CHECK(tsfp_io_call_driver(&scopes,active_owner,active_binding,device_body,&call_ops,&status)==TSFP_IO_CALL_RETURNED);
    /* Both pointed-to control blocks are now unmapped. Compare saved addresses
     * through VirtualQuery, never read the reclaimed owner/binding again. */
    CHECK(status==0x55 && g_eax==0x1111 && g_esp==0x9007 && called==2 && metadata_freed==1 && binding_freed==1 && !scopes.current);
    CHECK(VirtualQuery(metadata_region,&page,sizeof(page)) && page.State==MEM_FREE);
    CHECK(VirtualQuery(binding_region,&page,sizeof(page)) && page.State==MEM_FREE);
    tsfp_io_pool_counts(&request_pool,&live,&retained_count,&uncertain);CHECK(!live && !retained_count && !uncertain);
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);CHECK(frees==1 && memory.heap.count==1 && tsfp_allocation_remaining(&memory,request_address)==152 && !request_pool.nodes);
    CHECK(!memcmp(caller_final,ram+request_address,152));
    CHECK(tsfp_heap_release(&memory,request_address) && !memory.heap.count);
    CHECK(prepared==1 && notified==1 && released==1 && !event_pin);
    CHECK(MEM32(0x8011)==0 && MEM32(0x8015)==17 && tsfp_event_state(&guest_event)==1 && WaitForSingleObject(completion_event,0)==WAIT_OBJECT_0);
    CHECK(CloseHandle(completion_event));tsfp_io_pool_stop(&request_pool);
    puts("PASS: caller-owned packet adoption, nested dispatch, real IOSB/event delivery, metadata unmap and retained caller storage");return 0;
}
