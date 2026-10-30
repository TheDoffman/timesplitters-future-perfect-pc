#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_devices.h"
static tsfp_io_owner request_owner;static unsigned char request_bytes[256];
static tsfp_io_devices binding;static tsfp_io_device_entry entries[2];
static unsigned request_depth,retired_count,disposed,reject_release,expected_clean;
static void request_lock(void *ctx) {CHECK(ctx==&request_owner && !request_depth);++request_depth;}
static void request_unlock(void *ctx) {CHECK(ctx==&request_owner && request_depth==1);--request_depth;}
static void retired(void *ctx,tsfp_io_devices *b,int clean) {
    CHECK(ctx==&request_owner && b==&binding && !request_depth && !retired_count && !disposed);
    CHECK(clean==(int)expected_clean && b->closed && !b->owner);++retired_count;
}
static void request_dispose(void *ctx,uint32_t address,void *bytes,size_t size) {
    CHECK(ctx==&request_owner && !request_depth && retired_count==1 && !disposed);
    CHECK(address==0x9000 && bytes==request_bytes && size==256);++disposed;
}
static int fixture_release(tsfp_guest_object_lease *lease) {
    if(reject_release && lease==&entries[1].lease) {reject_release=0;return 0;}
    return tsfp_guest_objects_release(lease);
}
#define tsfp_guest_objects_release fixture_release
#include "guest_io_devices.c"
#undef tsfp_guest_objects_release
static void scenario(unsigned capacity,unsigned clean) {
    tsfp_io_owner_ops ops={&request_owner,request_lock,request_unlock,request_dispose};
    tsfp_io_ticket dispatch={0};uint32_t first,second,span;void *mapping,*old;
    init();retired_count=disposed=reject_release=0;expected_clean=clean;
    CHECK(create(0,0)==TSFP_DEVICE_OK);first=value(ram+0x801);
    CHECK(create(0,0)==TSFP_DEVICE_OK);second=value(ram+0x801);CHECK(first!=second);
    CHECK(tsfp_io_owner_init(&request_owner,&ops,request_bytes,256,0x9000,256,2,6));
    CHECK(tsfp_io_devices_init(&binding,&request_owner,&factory,entries,capacity,retired,&request_owner));
    CHECK(tsfp_io_devices_pin(&binding,first,&mapping,&span) && binding.count==1);old=mapping;
    CHECK(tsfp_io_devices_pin(&binding,first,&mapping,&span) && binding.count==1 && mapping==old);
    mapping=(void *)(uintptr_t)0x1234;span=0x5678;
    if(capacity==1) {
        CHECK(!tsfp_io_devices_pin(&binding,second,&mapping,&span));CHECK(mapping==(void *)(uintptr_t)0x1234 && span==0x5678 && binding.count==1);
    } else CHECK(tsfp_io_devices_pin(&binding,second,&mapping,&span) && binding.count==2);
    CHECK(tsfp_io_owner_dispatch_begin(&request_owner,&dispatch));CHECK(tsfp_io_owner_release(&request_owner));
    CHECK(tsfp_device_factory_delete(&factory,first) && tsfp_device_factory_delete(&factory,second));
    CHECK(!tsfp_guest_objects_collect(&objects));CHECK(frees==(capacity==1?1u:0u));
    CHECK(tsfp_io_owner_dispatch_end(&dispatch,1));CHECK(!retired_count && !disposed && request_owner.internal==1);
    if(!clean)reject_release=1;
    CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));CHECK(retired_count==1 && disposed==1);
    CHECK(!tsfp_guest_objects_collect(&objects));
    CHECK(!tsfp_io_devices_pin(&binding,first,&mapping,&span));
    if(clean) {CHECK(frees==2 && !binding.uncertain);counts(0,0,0);}
    else {
        CHECK(frees==1 && binding.uncertain==1 && entries[1].lease.owner);counts(1,0,0);
        /* The fixture injected a known retained release, so it owns recovery;
         * the production provider reported quarantine and did not retry it. */
        CHECK(tsfp_guest_objects_release(&entries[1].lease));CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);CHECK(frees==2);
    }
}
int main(void) {
    scenario(1,1);scenario(2,1);scenario(2,0);
    puts("PASS: bounded distinct-device bindings survive pending work, release once, preserve failed outputs and report uncertain cleanup");return 0;
}
