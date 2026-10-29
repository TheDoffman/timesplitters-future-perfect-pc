#include <windows.h>
#include "guest_io_pool.h"
#include "guest_allocation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"native pool line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static tsfp_io_pool pool;static tsfp_io_registry_entry entries[32];
static tsfp_allocations allocations;static tsfp_allocation_record records[64];
static unsigned char ram[1048576];static CRITICAL_SECTION locks[2];
static __declspec(thread) unsigned depth[2];
static LONG metadata_live,allocated,released;static HANDLE ready[4],proceed;
static unsigned ids[4]={0,1,2,3};
static void lock(void *ctx) {CHECK(ctx==&pool);EnterCriticalSection(&locks[0]);CHECK(!depth[0]);++depth[0];}
static void unlock(void *ctx) {CHECK(ctx==&pool && depth[0]==1);--depth[0];LeaveCriticalSection(&locks[0]);}
static void registry_lock(void *ctx) {CHECK(ctx==&pool && !depth[0]);EnterCriticalSection(&locks[1]);CHECK(!depth[1]);++depth[1];}
static void registry_unlock(void *ctx) {CHECK(ctx==&pool && depth[1]==1 && !depth[0]);--depth[1];LeaveCriticalSection(&locks[1]);}
static void *metadata_allocate(void *ctx,size_t bytes) {
    void *p;CHECK(ctx==&pool && !depth[0] && !depth[1]);p=malloc(bytes);CHECK(p);
    InterlockedIncrement(&metadata_live);return p;
}
static void metadata_free(void *ctx,void *p) {
    CHECK(ctx==&pool && !depth[0] && !depth[1]);InterlockedDecrement(&metadata_live);free(p);
}
static uint32_t allocate(void *ctx,uint32_t bytes) {
    uint32_t address;CHECK(ctx==&pool && !depth[0] && !depth[1]);
    address=tsfp_heap_allocate(&allocations,bytes,16);CHECK(address);InterlockedIncrement(&allocated);return address;
}
static void *resolve(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==&pool && !depth[0] && !depth[1]);
    CHECK(tsfp_allocation_remaining(&allocations,address)>=bytes);return ram+address;
}
static int deallocate(void *ctx,uint32_t address) {
    CHECK(ctx==&pool && !depth[0] && !depth[1]);CHECK(tsfp_heap_release(&allocations,address));
    InterlockedIncrement(&released);return 1;
}
static void complete(void *ctx,tsfp_io_owner *owner) {
    CHECK(ctx==&pool && !depth[0] && !depth[1]);CHECK(tsfp_io_owner_complete(owner,0x12345678,2048,NULL,NULL));
}
static void pending(void *ctx,tsfp_io_owner *owner) {CHECK(ctx==&pool && tsfp_io_owner_pending(owner));}
static DWORD WINAPI worker(void *ctx) {
    unsigned id=*(unsigned *)ctx,i;uint32_t address;tsfp_io_owner *owner;
    for(i=0;i<250;++i) {
        address=tsfp_io_pool_allocate(&pool,1+(i%4));CHECK(address);
        owner=tsfp_io_registry_acquire(&pool.registry,address);CHECK(owner && owner->bytes[0]==6);
        CHECK(tsfp_io_registry_remove(&pool.registry,address));
        CHECK(tsfp_io_owner_dispatch(owner,complete,&pool));
        CHECK(owner->bytes[0x10]==0x78 && owner->bytes[0x11]==0x56 && owner->bytes[0x15]==8);
        CHECK(tsfp_io_owner_release(owner));
    }
    address=tsfp_io_pool_allocate(&pool,2);CHECK(address);
    owner=tsfp_io_registry_acquire(&pool.registry,address);CHECK(owner);
    CHECK(tsfp_io_owner_dispatch(owner,pending,&pool));CHECK(SetEvent(ready[id]));
    CHECK(WaitForSingleObject(proceed,10000)==WAIT_OBJECT_0);
    CHECK(!tsfp_io_registry_acquire(&pool.registry,address));
    CHECK(owner->bytes[0]==6 && tsfp_io_owner_complete(owner,0,0,NULL,NULL));
    CHECK(tsfp_io_owner_release(owner));return 0;
}
int main(void) {
    tsfp_io_pool_ops ops;tsfp_allocation_config config;HANDLE threads[4];unsigned i;
    size_t live,retained,uncertain;
    for(i=0;i<2;++i) {InitializeCriticalSection(&locks[i]);}
    memset(&config,0,sizeof(config));config.ram=ram;config.ram_size=sizeof(ram);
    config.heap_begin=0x1000;config.reserve_begin=0x80000;
    CHECK(tsfp_allocations_init(&allocations,&config,records,64,NULL,0));
    memset(&ops,0,sizeof(ops));ops.context=&pool;ops.lock=lock;ops.unlock=unlock;
    ops.registry_lock=registry_lock;ops.registry_unlock=registry_unlock;
    ops.metadata_allocate=metadata_allocate;ops.metadata_free=metadata_free;
    ops.allocate=allocate;ops.resolve=resolve;ops.deallocate=deallocate;
    CHECK(tsfp_io_pool_init(&pool,entries,32,&ops));
    proceed=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(proceed);
    for(i=0;i<4;++i) {
        ready[i]=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(ready[i]);
        threads[i]=CreateThread(NULL,0,worker,&ids[i],0,NULL);CHECK(threads[i]);
    }
    CHECK(WaitForMultipleObjects(4,ready,TRUE,20000)==WAIT_OBJECT_0);
    tsfp_io_pool_stop(&pool);tsfp_io_pool_counts(&pool,&live,&retained,&uncertain);
    CHECK(live==4 && metadata_live==4 && !retained && !uncertain);
    CHECK(!tsfp_io_pool_allocate(&pool,2));CHECK(SetEvent(proceed));
    CHECK(WaitForMultipleObjects(4,threads,TRUE,20000)==WAIT_OBJECT_0);
    tsfp_io_pool_counts(&pool,&live,&retained,&uncertain);
    CHECK(!live && !retained && !uncertain && !metadata_live && allocated==1004 && released==1004);
    CHECK(!allocations.heap.count);
    for(i=0;i<4;++i) {CHECK(CloseHandle(threads[i]));CHECK(CloseHandle(ready[i]));}
    CHECK(CloseHandle(proceed));for(i=0;i<2;++i) {DeleteCriticalSection(&locks[i]);}
    puts("PASS: four native allocation/dispatch workers, pending stop and complete guest/host reclamation");return 0;
}
