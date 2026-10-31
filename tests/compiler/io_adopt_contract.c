#define main pool_fixture_main
#include "io_pool_contract.c"
#undef main
static unsigned resolve_calls,forbid_resolve,attachment_released,preparation_mode;
static uint32_t candidate_address;static void *last_metadata;
static unsigned char snapshot[65536];
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
static void *observed_resolve(void *c,uint32_t address,uint32_t bytes) {CHECK(!forbid_resolve);++resolve_calls;return resolve(c,address,bytes);}
static void *observed_metadata(void *c,size_t bytes) {last_metadata=metadata_allocate(c,bytes);return last_metadata;}
static void setup(void) {
    init();resolve_calls=forbid_resolve=attachment_released=preparation_mode=0;last_metadata=NULL;
    pool.ops.resolve=observed_resolve;pool.ops.metadata_allocate=observed_metadata;
}
static void retained_storage(uint32_t base,uint32_t bytes) {
    CHECK(tsfp_allocation_remaining(&allocations,base)>=bytes && !free_calls);
    CHECK(!memcmp(ram+base,snapshot,bytes));
}
static void readonly_layouts(void) {
    uint32_t stacks,location,bytes,base,address;tsfp_io_request_layout layout;
    for(stacks=0;stacks<=127;++stacks) {
        setup();bytes=TSFP_IO_REQUEST_BYTES+stacks*TSFP_IO_STACK_BYTES;
        base=tsfp_heap_allocate(&allocations,bytes+2,16);CHECK(base);address=base+1;
        memset(ram+base,0xa5,bytes+2);CHECK(tsfp_io_request_initialize(ram+address,bytes,address,bytes,stacks,6));
        memset(ram+address+104,0x5a,bytes-104);
        for(location=1;location<=stacks+1;++location) {
            ram[address+0x19]=(unsigned char)location;
            put(ram+address+0x5c,address+104+(location-1)*24);
            CHECK(tsfp_io_request_validate(ram+address,bytes,address,6,&layout));
            CHECK(layout.bytes==bytes && layout.stacks==stacks && layout.location==location && layout.current==address+104+(location-1)*24);
        }
        ram[address+0x19]=1;put(ram+address+0x5c,address+104);put(ram+address+0x10,0x103);
        memcpy(snapshot,ram+base,bytes+2);
        CHECK(tsfp_io_pool_adopt(&pool,address,bytes,NULL,NULL));counts(1,0,0);
        {
            tsfp_io_owner *owner=tsfp_io_registry_acquire(&pool.registry,address);void *metadata=last_metadata;
            CHECK(owner && owner->phase==TSFP_IO_READY && owner->clients==2 && !owner->internal && owner->span==bytes);
            CHECK(owner->address==address && owner->bytes==ram+address && pool.nodes);
            CHECK(tsfp_io_registry_remove(&pool.registry,address));CHECK(tsfp_io_owner_release(owner));
#ifdef _WIN32
            {MEMORY_BASIC_INFORMATION page;CHECK(VirtualQuery(metadata,&page,sizeof(page)) && page.State==MEM_FREE);}
#else
            (void)metadata;
#endif
        }
        counts(0,0,0);CHECK(!pool.nodes && !allocation_calls);retained_storage(base,bytes+2);
        CHECK(tsfp_heap_release(&allocations,base));tsfp_io_pool_stop(&pool);CHECK(!allocations.heap.count);
    }
}
static void borrowed_attachment(void *c) {CHECK(c==&pool && !depth && !registry_depth && !attachment_released);++attachment_released;}
static int prepare(void *c,tsfp_io_owner *owner) {
    CHECK(c==&pool && !depth && !registry_depth && owner->phase==TSFP_IO_READY && owner->clients==1 && !owner->internal);
    CHECK(owner->address==candidate_address && !tsfp_io_registry_acquire(&pool.registry,candidate_address));
    CHECK(tsfp_io_owner_attach(owner,&pool,borrowed_attachment));
    if(preparation_mode==2)tsfp_io_pool_stop(&pool);
    if(preparation_mode==3) {
        unsigned calls=resolve_calls;forbid_resolve=1;
        CHECK(!tsfp_io_pool_adopt(&pool,candidate_address,152,NULL,NULL));
        forbid_resolve=0;CHECK(resolve_calls==calls);
    }
    return preparation_mode!=1;
}
static void private_preparation(void) {
    unsigned mode;uint32_t base;
    for(mode=0;mode<4;++mode) {
        setup();preparation_mode=mode;base=tsfp_heap_allocate(&allocations,154,16);CHECK(base);candidate_address=base+1;
        memset(ram+base,0xa5,154);CHECK(tsfp_io_request_initialize(ram+candidate_address,152,candidate_address,152,2,6));
        memcpy(snapshot,ram+base,154);
        CHECK(tsfp_io_pool_adopt(&pool,candidate_address,152,prepare,&pool)==(mode==0 || mode==3));
        if(mode==0 || mode==3) {CHECK(!attachment_released);CHECK(tsfp_io_registry_remove(&pool.registry,candidate_address));}
        counts(0,0,0);CHECK(attachment_released==1 && !pool.nodes);retained_storage(base,154);
        CHECK(tsfp_heap_release(&allocations,base));tsfp_io_pool_stop(&pool);
    }
}
static void detached_ranges(void) {
    uint32_t base,a,b;unsigned calls;tsfp_io_owner *owner;
    setup();base=tsfp_heap_allocate(&allocations,512,16);CHECK(base);a=base+1;b=a+128;
    CHECK(tsfp_io_request_initialize(ram+a,256,a,256,2,6));
    CHECK(tsfp_io_request_initialize(ram+b,152,b,152,2,6)); /* arbitrary opaque bytes of first packet */
    CHECK(tsfp_io_pool_adopt(&pool,a,256,NULL,NULL));
    owner=tsfp_io_registry_acquire(&pool.registry,a);CHECK(owner);
    CHECK(tsfp_io_owner_dispatch(owner,pending,&pool));CHECK(tsfp_io_owner_release(owner));
    CHECK(tsfp_io_registry_remove(&pool.registry,a));counts(1,0,0);CHECK(pool.nodes);
    calls=resolve_calls;forbid_resolve=1;
    CHECK(!tsfp_io_pool_adopt(&pool,a,256,NULL,NULL));CHECK(!tsfp_io_pool_adopt(&pool,b,152,NULL,NULL));
    forbid_resolve=0;CHECK(resolve_calls==calls && !free_calls);counts(1,0,0);
    CHECK(tsfp_io_owner_complete(owner,0,17,NULL,NULL));counts(0,0,0);CHECK(!pool.nodes && !free_calls);
    memcpy(snapshot,ram+base,512);
    CHECK(tsfp_io_pool_adopt(&pool,b,152,NULL,NULL));CHECK(tsfp_io_registry_remove(&pool.registry,b));
    counts(0,0,0);retained_storage(base,512);CHECK(tsfp_heap_release(&allocations,base));tsfp_io_pool_stop(&pool);
    setup();a=tsfp_io_pool_allocate(&pool,2);CHECK(a);
    owner=tsfp_io_registry_acquire(&pool.registry,a);CHECK(owner && tsfp_io_registry_remove(&pool.registry,a));
    forbid_resolve=1;CHECK(!tsfp_io_pool_adopt(&pool,a,152,NULL,NULL));CHECK(!tsfp_io_pool_adopt(&pool,a+1,151,NULL,NULL));forbid_resolve=0;
    CHECK(tsfp_io_owner_release(owner));counts(0,0,0);CHECK(free_calls==1 && !pool.nodes && !allocations.heap.count);tsfp_io_pool_stop(&pool);
    setup();mapping_fail=1;free_result=0;CHECK(!tsfp_io_pool_allocate(&pool,1));counts(1,1,0);CHECK(pool.nodes);
    forbid_resolve=1;CHECK(!tsfp_io_pool_adopt(&pool,0x1000,128,NULL,NULL));forbid_resolve=0;
    free_result=1;CHECK(tsfp_io_pool_retry_one(&pool));counts(0,0,0);CHECK(!pool.nodes && !allocations.heap.count);tsfp_io_pool_stop(&pool);
}
static void rejected_bytes(void) {
    uint32_t base,address;unsigned which,calls;unsigned char valid[152],corrupt[152];tsfp_io_request_layout layout,before;
    setup();base=tsfp_heap_allocate(&allocations,154,16);CHECK(base);address=base+1;
    CHECK(tsfp_io_request_initialize(ram+address,152,address,152,2,6));memcpy(valid,ram+address,152);
    for(which=0;which<7;++which) {
        memcpy(ram+address,valid,152);
        if(which==0)ram[address]=7;
        if(which==1)ram[address+2]=151;
        if(which==2)ram[address+0x18]=128;
        if(which==3)ram[address+0x19]=0;
        if(which==4)put(ram+address+0x5c,word(valid+0x5c)+1);
        if(which==5)ram[address+0x19]=4;
        if(which==6)put(ram+address+0x5c,0);
        memcpy(corrupt,ram+address,152);memset(&layout,0xa5,sizeof(layout));memcpy(&before,&layout,sizeof(layout));
        CHECK(!tsfp_io_request_validate(ram+address,152,address,6,&layout));CHECK(!memcmp(&before,&layout,sizeof(layout)));
        CHECK(!tsfp_io_pool_adopt(&pool,address,152,NULL,NULL));CHECK(!memcmp(corrupt,ram+address,152));counts(0,0,0);
    }
    memcpy(ram+address,valid,152);memcpy(snapshot,ram+base,154);
    CHECK(!tsfp_io_pool_adopt(&pool,address,151,NULL,NULL));CHECK(!tsfp_io_pool_adopt(&pool,address,153,NULL,NULL));
    CHECK(!tsfp_io_pool_adopt(&pool,UINT32_MAX-50,152,NULL,NULL));
    metadata_fail=1;CHECK(!tsfp_io_pool_adopt(&pool,address,152,NULL,NULL));metadata_fail=0;
    mapping_fail=1;CHECK(!tsfp_io_pool_adopt(&pool,address,152,NULL,NULL));mapping_fail=0;counts(0,0,0);
    calls=metadata_calls;tsfp_io_pool_stop(&pool);CHECK(!tsfp_io_pool_adopt(&pool,address,152,NULL,NULL) && metadata_calls==calls);
    retained_storage(base,154);CHECK(!allocation_calls && !pool.nodes);CHECK(tsfp_heap_release(&allocations,base));
}
static void pending_stop(void) {
    uint32_t base,address;tsfp_io_owner *owner;
    setup();base=tsfp_heap_allocate(&allocations,152,16);CHECK(base);address=base;
    CHECK(tsfp_io_request_initialize(ram+address,152,address,152,2,6));
    CHECK(tsfp_io_pool_adopt(&pool,address,152,NULL,NULL));owner=tsfp_io_registry_acquire(&pool.registry,address);CHECK(owner);
    CHECK(tsfp_io_owner_dispatch(owner,pending,&pool));CHECK(tsfp_io_owner_release(owner));
    tsfp_io_pool_stop(&pool);counts(1,0,0);CHECK(!free_calls && pool.nodes);
    CHECK(tsfp_io_owner_complete(owner,0,17,NULL,NULL));counts(0,0,0);CHECK(!free_calls && !pool.nodes);
    CHECK(tsfp_allocation_remaining(&allocations,base)==152);CHECK(tsfp_heap_release(&allocations,base));
}
static unsigned manual_disposed;
static void manual_dispose(void *c,uint32_t address,void *mapping,size_t span) {
    CHECK(c==&pool && !depth && !registry_depth && address==candidate_address && mapping==ram+address && span==152);
    CHECK(!manual_disposed && tsfp_allocation_remaining(&allocations,address)>=span);++manual_disposed;
}
static void control_preservation(void) {
    tsfp_io_owner owner,before;tsfp_io_owner_ops ops={&pool,lock,unlock,manual_dispose};uint32_t base;
    setup();manual_disposed=0;base=tsfp_heap_allocate(&allocations,154,16);CHECK(base);candidate_address=base+1;
    CHECK(tsfp_io_request_initialize(ram+candidate_address,152,candidate_address,152,2,6));
    memset(&owner,0xa5,sizeof(owner));memcpy(&before,&owner,sizeof(owner));
    put(ram+candidate_address+0x5c,0);memcpy(snapshot,ram+base,154);
    CHECK(!tsfp_io_owner_adopt(&owner,&ops,ram+candidate_address,152,candidate_address,152,6));
    CHECK(!memcmp(&before,&owner,sizeof(owner)));retained_storage(base,154);
    put(ram+candidate_address+0x5c,candidate_address+152);memcpy(snapshot,ram+base,154);
    CHECK(tsfp_io_owner_adopt(&owner,&ops,ram+candidate_address,152,candidate_address,152,6));
    CHECK(owner.phase==TSFP_IO_READY && owner.clients==1 && !owner.internal);
    CHECK(tsfp_io_owner_release(&owner) && manual_disposed==1);retained_storage(base,154);
    CHECK(tsfp_heap_release(&allocations,base));tsfp_io_pool_stop(&pool);
}
static unsigned handoff_once;static uint32_t handoff_address;
static int handoff_deallocate(void *c,uint32_t address) {
    int result=deallocate(c,address);
    if(result==1 && handoff_once) {
        uint32_t caller;handoff_once=0;
        /* The allocator has consumed the old storage, but the old host metadata
         * is still in its disposal callback. External adoption must wait; a new
         * unique allocator-owned packet may take the new allocation generation. */
        caller=tsfp_heap_allocate(&allocations,128,16);CHECK(caller==address);
        CHECK(tsfp_io_request_initialize(ram+caller,128,caller,128,1,6));
        forbid_resolve=1;CHECK(!tsfp_io_pool_adopt(&pool,caller,128,NULL,NULL));forbid_resolve=0;
        CHECK(tsfp_heap_release(&allocations,caller));
        handoff_address=tsfp_io_pool_allocate(&pool,1);CHECK(handoff_address==address);
    }
    return result;
}
static void allocator_handoff(void) {
    uint32_t address;tsfp_io_owner *owner;
    setup();pool.ops.deallocate=handoff_deallocate;handoff_once=1;handoff_address=0;
    address=tsfp_io_pool_allocate(&pool,2);CHECK(address);
    CHECK(tsfp_io_registry_remove(&pool.registry,address));
    CHECK(!handoff_once && handoff_address==address);counts(1,0,0);CHECK(pool.nodes);
    owner=tsfp_io_registry_acquire(&pool.registry,handoff_address);CHECK(owner && owner->span==128 && owner->bytes[0]==6);
    CHECK(tsfp_io_registry_remove(&pool.registry,handoff_address));CHECK(tsfp_io_owner_release(owner));
    counts(0,0,0);CHECK(!pool.nodes && !allocations.heap.count && free_calls==2);tsfp_io_pool_stop(&pool);
}
int main(void) {
    readonly_layouts();private_preparation();detached_ranges();rejected_bytes();pending_stop();control_preservation();allocator_handoff();
    puts("PASS: all caller-owned layouts, read-only adoption, metadata-only disposal, detached range exclusion, private rollback and pending stop without guest-storage frees");return 0;
}
