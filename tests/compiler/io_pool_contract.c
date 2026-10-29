#include "guest_io_pool.h"
#include "guest_allocation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"pool line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static tsfp_io_pool pool;static tsfp_io_registry_entry entries[2];
static tsfp_allocations allocations;static tsfp_allocation_record records[8];
static unsigned char ram[65536];
static int depth,registry_depth,metadata_fail,allocation_fail,mapping_fail,free_result,stop_on_allocate;
static unsigned metadata_live,metadata_calls,allocation_calls,free_calls;
static void lock(void *ctx) {CHECK(ctx==&pool && !depth);++depth;}
static void unlock(void *ctx) {CHECK(ctx==&pool && depth==1);--depth;}
static void registry_lock(void *ctx) {CHECK(ctx==&pool && !registry_depth && !depth);++registry_depth;}
static void registry_unlock(void *ctx) {CHECK(ctx==&pool && registry_depth==1 && !depth);--registry_depth;}
static void *metadata_allocate(void *ctx,size_t bytes) {
    void *p;CHECK(ctx==&pool && !depth && !registry_depth);++metadata_calls;
    if(metadata_fail)return NULL;
#ifdef _WIN32
    CHECK(bytes<=4096);p=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
#else
    p=malloc(bytes);
#endif
    CHECK(p);++metadata_live;return p;
}
static void metadata_free(void *ctx,void *p) {
    CHECK(ctx==&pool && !depth && !registry_depth && metadata_live);--metadata_live;
#ifdef _WIN32
    CHECK(VirtualFree(p,0,MEM_RELEASE));
#else
    free(p);
#endif
}
static uint32_t allocate(void *ctx,uint32_t bytes) {
    CHECK(ctx==&pool && !depth && !registry_depth);++allocation_calls;
    if(stop_on_allocate)tsfp_io_pool_stop(&pool);
    return allocation_fail?0:tsfp_heap_allocate(&allocations,bytes,16);
}
static void *resolve(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==&pool && !depth && !registry_depth);
    CHECK(tsfp_allocation_remaining(&allocations,address)>=bytes);
    return mapping_fail?NULL:ram+address;
}
static int deallocate(void *ctx,uint32_t address) {
    size_t live,retained,uncertain;
    CHECK(ctx==&pool && !depth && !registry_depth);++free_calls;
    tsfp_io_pool_counts(&pool,&live,&retained,&uncertain);CHECK(live>=1);
    if(free_result!=1)return free_result;
    CHECK(tsfp_heap_release(&allocations,address));return 1;
}
static void counts(size_t live,size_t retained,size_t uncertain) {
    size_t a=99,b=99,c=99;tsfp_io_pool_counts(&pool,&a,&b,&c);
    CHECK(a==live && b==retained && c==uncertain && a==metadata_live);
}
static void init(void) {
    tsfp_allocation_config config;tsfp_io_pool_ops ops;
    CHECK(!metadata_live);memset(&allocations,0,sizeof(allocations));memset(&config,0,sizeof(config));
    config.ram=ram;config.ram_size=sizeof(ram);config.heap_begin=0x1000;config.reserve_begin=0x8000;
    CHECK(tsfp_allocations_init(&allocations,&config,records,8,NULL,0));
    metadata_fail=allocation_fail=mapping_fail=stop_on_allocate=0;free_result=1;
    metadata_calls=allocation_calls=free_calls=0;memset(&ops,0,sizeof(ops));
    ops.context=&pool;ops.lock=lock;ops.unlock=unlock;
    ops.registry_lock=registry_lock;ops.registry_unlock=registry_unlock;
    ops.metadata_allocate=metadata_allocate;ops.metadata_free=metadata_free;
    ops.allocate=allocate;ops.resolve=resolve;ops.deallocate=deallocate;
    CHECK(tsfp_io_pool_init(&pool,entries,2,&ops));counts(0,0,0);
}
static void pending(void *ctx,tsfp_io_owner *owner) {CHECK(ctx==&pool && tsfp_io_owner_pending(owner));}
int main(void) {
    uint32_t a,b;tsfp_io_owner *borrow;unsigned calls;
    init();CHECK(!tsfp_io_pool_allocate(&pool,128));CHECK(!metadata_calls && !allocation_calls);
    metadata_fail=1;CHECK(!tsfp_io_pool_allocate(&pool,2));counts(0,0,0);CHECK(!allocation_calls);
    metadata_fail=0;allocation_fail=1;CHECK(!tsfp_io_pool_allocate(&pool,2));counts(0,0,0);CHECK(!free_calls);
    allocation_fail=0;mapping_fail=1;CHECK(!tsfp_io_pool_allocate(&pool,2));counts(0,0,0);CHECK(free_calls==1);
    mapping_fail=0;a=tsfp_io_pool_allocate(&pool,2);CHECK(a==0x1000);counts(1,0,0);
    CHECK(ram[a]==6 && ram[a+1]==0 && ram[a+2]==152 && ram[a+3]==0);
    CHECK(ram[a+0x18]==2 && ram[a+0x19]==3 && ram[a+0x5c]==0x98 && ram[a+0x5d]==0x10);
    borrow=tsfp_io_registry_acquire(&pool.registry,a);CHECK(borrow && borrow->span==152);
    CHECK(tsfp_io_registry_remove(&pool.registry,a));counts(1,0,0);
    CHECK(tsfp_allocation_remaining(&allocations,a)==152 && borrow->bytes[0]==6);
    CHECK(tsfp_io_owner_release(borrow));counts(0,0,0);CHECK(!tsfp_allocation_remaining(&allocations,a));
    a=tsfp_io_pool_allocate(&pool,0);b=tsfp_io_pool_allocate(&pool,127);CHECK(a && b && a!=b);
    calls=metadata_calls;CHECK(!tsfp_io_pool_allocate(&pool,1) && metadata_calls==calls);counts(2,0,0);
    tsfp_io_pool_stop(&pool);counts(0,0,0);CHECK(!tsfp_io_pool_allocate(&pool,1));
    init();stop_on_allocate=1;CHECK(!tsfp_io_pool_allocate(&pool,1));counts(0,0,0);CHECK(free_calls==1);
    init();a=tsfp_io_pool_allocate(&pool,2);CHECK(a);
    borrow=tsfp_io_registry_acquire(&pool.registry,a);CHECK(borrow);
    CHECK(tsfp_io_owner_dispatch(borrow,pending,&pool));CHECK(tsfp_io_owner_release(borrow));
    tsfp_io_pool_stop(&pool);counts(1,0,0);CHECK(!free_calls);
    /* Internal operation ownership permits this saved callback pointer until
     * completion. No access is permitted after complete reclaims the node. */
    CHECK(tsfp_io_owner_complete(borrow,0,0,NULL,NULL));counts(0,0,0);CHECK(free_calls==1);
    init();mapping_fail=1;free_result=0;CHECK(!tsfp_io_pool_allocate(&pool,1));counts(1,1,0);
    calls=free_calls;CHECK(!tsfp_io_pool_retry_one(&pool));CHECK(free_calls==calls+1);counts(1,1,0);
    free_result=1;CHECK(tsfp_io_pool_retry_one(&pool));counts(0,0,0);CHECK(!tsfp_io_pool_retry_one(&pool));
    init();mapping_fail=1;free_result=-1;CHECK(!tsfp_io_pool_allocate(&pool,1));counts(1,0,1);
    calls=free_calls;CHECK(!tsfp_io_pool_retry_one(&pool));CHECK(free_calls==calls);counts(1,0,1);
    tsfp_io_pool_stop(&pool);counts(1,0,1); /* Deliberately retained uncertain ownership. */
    puts("PASS: real guest heap request allocation, rollback, pending lifetime and cleanup accounting");return 0;
}
