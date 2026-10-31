#include "guest_io_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION mutexes[4];static __declspec(thread) int depth[4];
#else
static int depth[4];
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"registry line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static tsfp_io_registry registry;static tsfp_io_registry_entry entries[2];
static tsfp_io_owner owners[3];static unsigned char packets[3][128];
static unsigned frees[3];static int ids[4]={0,1,2,3};
static int stop_probe;
static void lock(void *ctx) {
    int id=*(int *)ctx;
#ifdef _WIN32
    EnterCriticalSection(&mutexes[id]);
#endif
    CHECK(!depth[id]);++depth[id];
}
static void unlock(void *ctx) {
    int id=*(int *)ctx;CHECK(depth[id]==1);--depth[id];
#ifdef _WIN32
    LeaveCriticalSection(&mutexes[id]);
#endif
}
static void dispose(void *ctx,uint32_t address,void *p,size_t span) {
    unsigned id=(unsigned)*(int *)ctx-1;
    CHECK(id<3 && !depth[0] && !depth[id+1] && !frees[id]);
    CHECK(p==packets[id] && span==128 && address==owners[id].address);
    CHECK(!tsfp_io_registry_acquire(&registry,address));
    if(stop_probe && id==0)CHECK(!tsfp_io_registry_acquire(&registry,0x2000));
    ++frees[id];memset(p,0xdd,span);
}
static void init(void) {
    unsigned i;stop_probe=0;CHECK(tsfp_io_registry_init(&registry,entries,2,&ids[0],lock,unlock));
    for(i=0;i<3;++i) {
        tsfp_io_owner_ops ops={&ids[i+1],lock,unlock,dispose};frees[i]=0;
        CHECK(tsfp_io_owner_init(&owners[i],&ops,packets[i],128,0x1000+i*0x1000,128,1,0x1234));
    }
}
static void cleanup_other(void) {
    CHECK(tsfp_io_owner_release(&owners[1]));CHECK(tsfp_io_owner_release(&owners[2]));
    CHECK(frees[1]==1 && frees[2]==1);
}
static void pending(void *ctx,tsfp_io_owner *owner) {
    CHECK(ctx==&registry && tsfp_io_owner_pending(owner));
}
static void cases(void) {
    tsfp_io_owner *borrow;init();
    CHECK(!tsfp_io_registry_acquire(&registry,0));CHECK(!tsfp_io_registry_acquire(&registry,0x1234));
    CHECK(tsfp_io_registry_publish(&registry,&owners[0]));
    CHECK(!tsfp_io_registry_publish(&registry,&owners[0]));
    owners[2].address=owners[0].address;
    CHECK(!tsfp_io_registry_publish(&registry,&owners[2]));owners[2].address=0x3000;
    CHECK(tsfp_io_registry_publish(&registry,&owners[1]));
    CHECK(!tsfp_io_registry_publish(&registry,&owners[2]));
    borrow=tsfp_io_registry_acquire(&registry,0x1000);CHECK(borrow==&owners[0]);
    CHECK(tsfp_io_owner_release(&owners[0]));
    CHECK(tsfp_io_registry_remove(&registry,0x1000));CHECK(!frees[0]);
    CHECK(!tsfp_io_registry_remove(&registry,0x1000));
    CHECK(!tsfp_io_registry_acquire(&registry,0x1000));
    CHECK(tsfp_io_owner_release(borrow));CHECK(frees[0]==1);
    tsfp_io_registry_stop(&registry);tsfp_io_registry_stop(&registry);
    CHECK(!frees[1] && !tsfp_io_registry_publish(&registry,&owners[2]));
    CHECK(!tsfp_io_registry_acquire(&registry,0x2000));cleanup_other();
    init();CHECK(tsfp_io_registry_publish(&registry,&owners[0]));
    CHECK(tsfp_io_owner_dispatch(&owners[0],pending,&registry));
    CHECK(tsfp_io_owner_release(&owners[0]));tsfp_io_registry_stop(&registry);
    CHECK(!frees[0]);CHECK(tsfp_io_owner_complete(&owners[0],0,0,NULL,NULL));
    CHECK(frees[0]==1);cleanup_other();
    init();CHECK(tsfp_io_registry_publish(&registry,&owners[0]));
    CHECK(tsfp_io_registry_publish(&registry,&owners[1]));
    CHECK(tsfp_io_owner_release(&owners[0]));CHECK(tsfp_io_owner_release(&owners[1]));
    stop_probe=1;tsfp_io_registry_stop(&registry);
    CHECK(frees[0]==1 && frees[1]==1);CHECK(tsfp_io_owner_release(&owners[2]));
}
#ifdef _WIN32
static HANDLE ready[4],proceed;
static DWORD WINAPI lookup_worker(void *ctx) {
    unsigned index=(unsigned)*(int *)ctx;tsfp_io_owner *borrow;
    borrow=tsfp_io_registry_acquire(&registry,0x1000);CHECK(borrow==&owners[0]);
    CHECK(SetEvent(ready[index]));CHECK(WaitForSingleObject(proceed,5000)==WAIT_OBJECT_0);
    CHECK(!tsfp_io_registry_acquire(&registry,0x1000));
    CHECK(tsfp_io_owner_phase(borrow)==TSFP_IO_READY && borrow->bytes[0]==0x34);
    CHECK(tsfp_io_owner_release(borrow));return 0;
}
static void race(void) {
    HANDLE threads[4];unsigned i;init();
    CHECK(tsfp_io_registry_publish(&registry,&owners[0]));CHECK(tsfp_io_owner_release(&owners[0]));
    proceed=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(proceed);
    for(i=0;i<4;++i) {
        ready[i]=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(ready[i]);
        threads[i]=CreateThread(NULL,0,lookup_worker,&ids[i],0,NULL);CHECK(threads[i]);
    }
    CHECK(WaitForMultipleObjects(4,ready,TRUE,10000)==WAIT_OBJECT_0);
    CHECK(tsfp_io_registry_remove(&registry,0x1000));CHECK(!frees[0]);
    CHECK(SetEvent(proceed));CHECK(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);
    CHECK(frees[0]==1);
    for(i=0;i<4;++i) {CHECK(CloseHandle(ready[i]));CHECK(CloseHandle(threads[i]));}
    CHECK(CloseHandle(proceed));tsfp_io_registry_stop(&registry);cleanup_other();
}
#endif
int main(void) {
#ifdef _WIN32
    unsigned i;for(i=0;i<4;++i) {InitializeCriticalSection(&mutexes[i]);}
#endif
    cases();
#ifdef _WIN32
    for(i=0;i<20;++i) {race();}for(i=0;i<4;++i) {DeleteCriticalSection(&mutexes[i]);}
#endif
    puts("PASS: address registry retains borrows, rejects duplicates and drains outside locks");return 0;
}
