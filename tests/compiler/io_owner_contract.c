#include "guest_io_owner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION mutex;
static __declspec(thread) int depth;
static LONG notifications;
#else
static int depth,notifications;
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"owner line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned char memory[160];static unsigned frees;
static tsfp_io_owner owner;
static void lock(void *ctx) {
    CHECK(ctx==&owner);
#ifdef _WIN32
    EnterCriticalSection(&mutex);
#endif
    CHECK(!depth);++depth;
}
static void unlock(void *ctx) {
    CHECK(ctx==&owner && depth==1);--depth;
#ifdef _WIN32
    LeaveCriticalSection(&mutex);
#endif
}
static void dispose(void *ctx,uint32_t address,void *p,size_t span) {
    CHECK(ctx==&owner && !depth && address==0x1000 && p==memory && span==sizeof(memory));
    CHECK(tsfp_io_owner_phase(&owner)==TSFP_IO_DEAD);CHECK(!frees);
    ++frees;memset(p,0xdd,span);
    CHECK(!tsfp_io_owner_release(&owner));CHECK(!tsfp_io_owner_retain(&owner));
}
static void init(void) {
    tsfp_io_owner_ops ops={&owner,lock,unlock,dispose};
    frees=0;notifications=0;memset(memory,0xab,sizeof(memory));
    CHECK(tsfp_io_owner_init(&owner,&ops,memory,sizeof(memory),0x1000,152,2,0x1234));
    CHECK(tsfp_io_owner_phase(&owner)==TSFP_IO_READY);
}
static void unused(void *ctx,tsfp_io_owner *o) {(void)ctx;(void)o;CHECK(0);}
static void notify(void *ctx,tsfp_io_owner *o) {
    CHECK(ctx==&owner && o==&owner && !depth && !frees);
    CHECK(tsfp_io_owner_phase(o)==TSFP_IO_COMPLETING);
    CHECK(memory[0x10]==0x78 && memory[0x11]==0x56 && memory[0x12]==0x34 && memory[0x13]==0x12);
    CHECK(memory[0x14]==0 && memory[0x15]==8 && !memory[0x16] && !memory[0x17]);
    CHECK(!tsfp_io_owner_complete(o,0,0,unused,NULL));
    CHECK(!tsfp_io_owner_pending(o));CHECK(!tsfp_io_owner_dispatch(o,unused,NULL));
#ifdef _WIN32
    InterlockedIncrement(&notifications);
#else
    ++notifications;
#endif
}
static void synchronous(void *ctx,tsfp_io_owner *o) {
    CHECK(ctx==&owner && !depth);CHECK(tsfp_io_owner_release(o));
    CHECK(!frees && tsfp_io_owner_phase(o)==TSFP_IO_ACTIVE);
    CHECK(tsfp_io_owner_complete(o,0x12345678,2048,notify,&owner));
    CHECK(!frees && tsfp_io_owner_phase(o)==TSFP_IO_COMPLETE);
    CHECK(memory[0x10]==0x78); /* dispatch pin still owns packet */
}
static void pending(void *ctx,tsfp_io_owner *o) {
    CHECK(ctx==&owner && !depth);CHECK(tsfp_io_owner_pending(o));
    CHECK(!tsfp_io_owner_pending(o));CHECK(!tsfp_io_owner_dispatch(o,unused,NULL));
}
static void cases(void) {
    tsfp_io_owner before;unsigned char saved[160];tsfp_io_owner_ops bad;
    init();before=owner;memcpy(saved,memory,sizeof(memory));bad=owner.ops;bad.dispose=NULL;
    CHECK(!tsfp_io_owner_init(&owner,&bad,memory,sizeof(memory),0x1000,152,2,0x1234));
    CHECK(!memcmp(&before,&owner,sizeof(owner)) && !memcmp(saved,memory,sizeof(memory)));
    CHECK(!tsfp_io_owner_complete(&owner,0,0,unused,NULL));
    CHECK(!tsfp_io_owner_dispatch(&owner,NULL,NULL));
    CHECK(tsfp_io_owner_retain(&owner));CHECK(tsfp_io_owner_release(&owner));CHECK(!frees);
    CHECK(tsfp_io_owner_release(&owner));CHECK(frees==1);
    CHECK(!tsfp_io_owner_dispatch(&owner,unused,NULL));
    init();CHECK(tsfp_io_owner_dispatch(&owner,synchronous,&owner));
    CHECK(frees==1 && notifications==1 && tsfp_io_owner_phase(&owner)==TSFP_IO_DEAD);
    init();CHECK(tsfp_io_owner_dispatch(&owner,pending,&owner));
    CHECK(tsfp_io_owner_release(&owner));CHECK(!frees);
    /* Corrupted guest header must never supply host refcounts or phase. */
    memset(memory,0xff,104);
    CHECK(tsfp_io_owner_retain(&owner));CHECK(tsfp_io_owner_release(&owner));CHECK(!frees);
    CHECK(tsfp_io_owner_complete(&owner,0x12345678,2048,notify,&owner));
    CHECK(frees==1 && notifications==1);
    CHECK(!tsfp_io_owner_complete(&owner,0,0,unused,NULL));
}
#ifdef _WIN32
static LONG winners;static HANDLE gate;
static DWORD WINAPI competing_completion(void *ctx) {
    CHECK(ctx==&owner);CHECK(WaitForSingleObject(gate,5000)==WAIT_OBJECT_0);
    if(tsfp_io_owner_complete(&owner,0x12345678,2048,notify,&owner))InterlockedIncrement(&winners);
    return 0;
}
static void race(void) {
    HANDLE threads[8];unsigned i;
    init();winners=0;CHECK(tsfp_io_owner_dispatch(&owner,pending,&owner));
    CHECK(tsfp_io_owner_release(&owner));CHECK(!frees);
    gate=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(gate);
    for(i=0;i<8;++i) {threads[i]=CreateThread(NULL,0,competing_completion,&owner,0,NULL);CHECK(threads[i]);}
    CHECK(SetEvent(gate));CHECK(WaitForMultipleObjects(8,threads,TRUE,10000)==WAIT_OBJECT_0);
    for(i=0;i<8;++i) {CHECK(CloseHandle(threads[i]));}CHECK(CloseHandle(gate));
    CHECK(winners==1 && notifications==1 && frees==1);
}
#endif
int main(void) {
#ifdef _WIN32
    unsigned i;InitializeCriticalSection(&mutex);
#endif
    cases();
#ifdef _WIN32
    for(i=0;i<100;++i) {race();}DeleteCriticalSection(&mutex);
#endif
    puts("PASS: retained request ownership, reentrant callbacks and exactly-once completion");return 0;
}
