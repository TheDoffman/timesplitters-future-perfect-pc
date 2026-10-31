#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_owner.h"
static tsfp_io_owner request_owner;static unsigned char request_bytes[256];
static tsfp_guest_object_lease pinned_device;static unsigned request_depth,released,disposed;
static void request_lock(void *ctx) {CHECK(ctx==&request_owner && !request_depth);++request_depth;}
static void request_unlock(void *ctx) {CHECK(ctx==&request_owner && request_depth==1);--request_depth;}
static void release_device(void *ctx) {
    CHECK(ctx==&pinned_device && !request_depth && !released && !disposed);
    CHECK(tsfp_guest_objects_release(&pinned_device));++released;
}
static void request_dispose(void *ctx,uint32_t address,void *bytes,size_t size) {
    CHECK(ctx==&request_owner && !request_depth && released==1 && !disposed);
    CHECK(address==0x9000 && bytes==request_bytes && size==256);++disposed;
}
int main(void) {
    tsfp_io_owner_ops ops={&request_owner,request_lock,request_unlock,request_dispose};
    tsfp_io_ticket dispatch={0};uint32_t body,size;void *mapping;
    init();CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);
    CHECK(tsfp_device_factory_pin(&factory,body,&pinned_device,&mapping,&size));
    CHECK(tsfp_io_owner_init(&request_owner,&ops,request_bytes,256,0x9000,256,2,6));
    CHECK(tsfp_io_owner_attach(&request_owner,&pinned_device,release_device));
    CHECK(!tsfp_io_owner_attach(&request_owner,&pinned_device,release_device));
    CHECK(tsfp_io_owner_dispatch_begin(&request_owner,&dispatch));
    CHECK(tsfp_io_owner_release(&request_owner));
    CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(!tsfp_guest_objects_collect(&objects) && !frees && !released);
    CHECK(tsfp_io_owner_dispatch_end(&dispatch,1));
    CHECK(request_owner.phase==TSFP_IO_PENDING && request_owner.internal==1 && !request_owner.clients);
    CHECK(!tsfp_io_owner_attach(&request_owner,&pinned_device,release_device));
    CHECK(!tsfp_guest_objects_collect(&objects) && !frees && !released && !disposed);
    CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));
    CHECK(released==1 && disposed==1 && request_owner.phase==TSFP_IO_DEAD);
    CHECK(!tsfp_guest_objects_collect(&objects) && frees==1);counts(0,0,0);
    puts("PASS: request attachment pins deleted device through abandoned dispatch and pending completion, releases once before request disposal");return 0;
}
