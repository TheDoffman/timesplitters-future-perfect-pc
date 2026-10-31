#define main pool_fixture_main
#include "io_pool_contract.c"
#undef main
#include "guest_io_build.h"
static tsfp_io_build_plan plan;
static unsigned preparation_calls,retired,mode;static uint32_t prepared_address;
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void release_attachment(void *context) {
    CHECK(context==&plan && !depth && !registry_depth && !retired && metadata_live==1);++retired;
}
static int prepare(void *context,tsfp_io_owner *owner) {
    CHECK(context==&plan && !depth && !registry_depth && owner && owner->clients==1 && !owner->internal && owner->phase==TSFP_IO_READY);
    ++preparation_calls;prepared_address=owner->address;
    CHECK(!tsfp_io_registry_acquire(&pool.registry,owner->address));
    if(!tsfp_io_build_packet(owner->bytes,owner->span,owner->address,(uint32_t)owner->span,
                            owner->bytes[0x18],&plan))return 0;
    CHECK(tsfp_io_owner_attach(owner,&plan,release_attachment));
    if(mode==2)tsfp_io_pool_stop(&pool);
    return mode!=1;
}
static void setup(unsigned selected) {
    init();memset(&plan,0,sizeof(plan));mode=selected;preparation_calls=retired=0;prepared_address=0;
    plan.transfer.major=2;plan.transfer.length=0x100;plan.transfer.device=0x6000;
    plan.transfer.byte_offset=UINT64_C(0x1122334455667788);plan.flags=0x14;
    plan.user_buffer=0x8000;plan.user_iosb=0x9000;plan.user_event=0xa000;plan.thread=0xb000;
}
int main(void) {
    uint32_t address;tsfp_io_owner *borrow;unsigned calls;
    setup(0);address=tsfp_io_pool_construct(&pool,2,prepare,&plan);CHECK(address && address==prepared_address);
    CHECK(preparation_calls==1 && !retired);counts(1,0,0);
    borrow=tsfp_io_registry_acquire(&pool.registry,address);CHECK(borrow && borrow->clients==2);
    CHECK(word(borrow->bytes+0x50)==0xb000 && word(borrow->bytes+0x4c)==0);
    CHECK(word(borrow->bytes+0x1c)==0x9000 && word(borrow->bytes+0x20)==0xa000 && word(borrow->bytes+0x30)==0x8000);
    CHECK(borrow->bytes[128]==2 && word(borrow->bytes+128+4)==0x100 && word(borrow->bytes+128+20)==0x6000);
    CHECK(tsfp_io_owner_dispatch(borrow,pending,&pool));
    CHECK(tsfp_io_registry_remove(&pool.registry,address) && tsfp_io_owner_release(borrow));
    CHECK(!retired && !free_calls && borrow->internal==1 && borrow->phase==TSFP_IO_PENDING);
    CHECK(tsfp_io_owner_complete(borrow,0,17,NULL,NULL));CHECK(retired==1 && free_calls==1);counts(0,0,0);
    CHECK(!allocations.heap.count);tsfp_io_pool_stop(&pool);
    setup(1);CHECK(!tsfp_io_pool_construct(&pool,2,prepare,&plan));
    CHECK(preparation_calls==1 && retired==1 && free_calls==1 && !allocations.heap.count);counts(0,0,0);
    CHECK(!tsfp_io_registry_acquire(&pool.registry,prepared_address));tsfp_io_pool_stop(&pool);
    setup(2);CHECK(!tsfp_io_pool_construct(&pool,2,prepare,&plan));
    CHECK(preparation_calls==1 && retired==1 && free_calls==1 && !allocations.heap.count);counts(0,0,0);
    calls=allocation_calls;CHECK(!tsfp_io_pool_construct(&pool,2,prepare,&plan) && allocation_calls==calls);
    setup(0);plan.transfer.major=4;CHECK(!tsfp_io_pool_construct(&pool,2,prepare,&plan));
    CHECK(preparation_calls==1 && !retired && free_calls==1 && !allocations.heap.count);counts(0,0,0);
    CHECK(!tsfp_io_registry_acquire(&pool.registry,prepared_address));tsfp_io_pool_stop(&pool);
    puts("PASS: private request construction before publication, pending lifetime and initializer/stop rollback without lost attachments");return 0;
}
