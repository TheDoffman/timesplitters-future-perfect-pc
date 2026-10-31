#include "guest_objects.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static tsfp_guest_objects store;
static tsfp_guest_handle_record handles[16];
static tsfp_guest_object_record objects[16];
static unsigned closes,attempts;
static int close_result=1;
static unsigned frees,free_attempts;
static uint32_t freed_kind,freed_flags;
static int free_result=1;
static uint32_t reenter_token;
static int close_host(void *context,uintptr_t host) {
    CHECK(context==&store && host);
    ++attempts;
    if(reenter_token) { uint32_t token=reenter_token;reenter_token=0;CHECK(tsfp_guest_objects_close(&store,token)); }
    if(close_result==1) ++closes;
    return close_result;
}
static int free_dispatcher(void *context,uint32_t address,uint32_t kind,uint32_t flags) {
    CHECK(context==&store && address);++free_attempts;
    freed_kind=kind;freed_flags=flags;
    if(free_result==1) ++frees;
    return free_result;
}
static void initialize(size_t capacity) {
    memset(&store,0,sizeof(store));closes=attempts=0;close_result=1;reenter_token=0;
    frees=free_attempts=0;free_result=1;
    CHECK(tsfp_guest_objects_init(&store,handles,objects,capacity,close_host,free_dispatcher,&store));
}
static void lifecycle(void) {
    uint32_t a,b,winner=0;
    uint64_t key,other;
    tsfp_guest_object_lease one={0},two={0};
    initialize(4);
    CHECK(tsfp_guest_objects_adopt(&store,42,3,8,&a));
    CHECK(tsfp_guest_objects_reference(&store,a,&key));
    CHECK(tsfp_guest_objects_duplicate(&store,a,0,&b) && a!=b);
    CHECK(tsfp_guest_objects_reference(&store,b,&other) && key==other);
    CHECK(tsfp_guest_objects_publish(&store,key,0x10000,&winner) && winner==0x10000);
    CHECK(tsfp_guest_objects_publish(&store,key,0x20000,&winner) && winner==0x10000);
    CHECK(tsfp_guest_objects_reference_dispatcher(&store,0x10000,&other) && other==key);
    CHECK(tsfp_guest_objects_dereference_dispatcher(&store,0x10000));
    CHECK(tsfp_guest_objects_acquire(&store,b,&one));
    CHECK(one.host==42 && one.kind==3 && one.flags==8 && one.dispatcher==0x10000);
    CHECK(tsfp_guest_objects_close(&store,a));CHECK(!tsfp_guest_objects_close(&store,a));
    CHECK(tsfp_guest_objects_close(&store,b));CHECK(!closes);
    CHECK(!tsfp_guest_objects_acquire(&store,b,&two));
    CHECK(tsfp_guest_objects_acquire_key(&store,key,&two) && two.host==42);
    CHECK(tsfp_guest_objects_release(&two));
    CHECK(tsfp_guest_objects_retain(&store,key));
    CHECK(tsfp_guest_objects_dereference(&store,key));
    CHECK(tsfp_guest_objects_dereference(&store,key));
    CHECK(tsfp_guest_objects_dereference(&store,key));CHECK(!closes);
    CHECK(tsfp_guest_objects_release(&one));CHECK(closes==1 && attempts==1 && frees==1);
    CHECK(freed_kind==3 && freed_flags==8);
    CHECK(!tsfp_guest_objects_reference_dispatcher(&store,0x10000,&other));
    CHECK(!tsfp_guest_objects_release(&one));CHECK(!tsfp_guest_objects_retain(&store,key));
    CHECK(!tsfp_guest_objects_dereference(&store,key));
    CHECK(!tsfp_guest_objects_publish(&store,key,0x30000,&winner));
    CHECK(tsfp_guest_objects_adopt(&store,42,9,0,&b) && b!=a);
    CHECK(!tsfp_guest_objects_acquire(&store,a,&two));
    CHECK(tsfp_guest_objects_acquire(&store,b,&two) && two.dispatcher==0 && two.kind==9 && two.flags==0);
    CHECK(tsfp_guest_objects_close(&store,b));CHECK(tsfp_guest_objects_release(&two));CHECK(closes==2);
}
static void capacity_and_generation(void) {
    uint32_t a,b,out=0xabcdef,previous=0;
    uint64_t ka,kb;
    unsigned i;
    tsfp_guest_object_lease lease={0};
    initialize(2);
    CHECK(tsfp_guest_objects_adopt(&store,7,0,0,&a));
    CHECK(tsfp_guest_objects_adopt(&store,7,0,0,&b) && a!=b);
    CHECK(tsfp_guest_objects_reference(&store,a,&ka));
    CHECK(tsfp_guest_objects_reference(&store,b,&kb) && ka!=kb);
    CHECK(!tsfp_guest_objects_duplicate(&store,a,0,&out) && out==0xabcdef);
    CHECK(!tsfp_guest_objects_adopt(&store,8,0,0,&out) && out==0xabcdef);
    CHECK(tsfp_guest_objects_duplicate(&store,a,1,&out) && out!=a && out!=b);
    CHECK(!tsfp_guest_objects_acquire(&store,a,&lease));
    CHECK(tsfp_guest_objects_close(&store,out));CHECK(tsfp_guest_objects_close(&store,b));
    CHECK(tsfp_guest_objects_dereference(&store,ka));CHECK(tsfp_guest_objects_dereference(&store,kb));
    CHECK(closes==2);
    initialize(1);
    for(i=0;i<1024;++i) {
        CHECK(tsfp_guest_objects_adopt(&store,11,0,0,&a) && a!=previous);
        CHECK(!tsfp_guest_objects_acquire(&store,previous,&lease));
        previous=a;
        if(i==1023) { out=0xabcdef;CHECK(!tsfp_guest_objects_duplicate(&store,a,1,&out));CHECK(out==0xabcdef); }
        else CHECK(tsfp_guest_objects_close(&store,a));
    }
    CHECK(closes==1024 && !tsfp_guest_objects_adopt(&store,12,0,0,&a));
    CHECK(!tsfp_guest_objects_close(&store,previous));
}
static void cleanup_failures(void) {
    uint32_t token,other;unsigned before;
    initialize(2);
    CHECK(tsfp_guest_objects_adopt(&store,1,0,0,&token));
    close_result=0;CHECK(tsfp_guest_objects_close(&store,token));CHECK(attempts==1 && !closes);
    CHECK(tsfp_guest_objects_collect(&store)==1 && attempts==2);
    close_result=1;CHECK(!tsfp_guest_objects_collect(&store) && closes==1);
    CHECK(tsfp_guest_objects_adopt(&store,2,0,0,&token));
    close_result=-1;CHECK(tsfp_guest_objects_close(&store,token));before=attempts;
    close_result=1;CHECK(tsfp_guest_objects_collect(&store)==1 && attempts==before);
    CHECK(tsfp_guest_objects_adopt(&store,3,0,0,&other));
    CHECK(!tsfp_guest_objects_adopt(&store,4,0,0,&token));
    CHECK(tsfp_guest_objects_close(&store,other));CHECK(closes==2);
    /* Deliberately uncertain host ownership remains recorded; do not retry it. */
}
static void callback_reentry(void) {
    uint32_t a,b;
    initialize(2);
    CHECK(tsfp_guest_objects_adopt(&store,1,0,0,&a));CHECK(tsfp_guest_objects_adopt(&store,2,0,0,&b));
    reenter_token=b;CHECK(tsfp_guest_objects_close(&store,a));CHECK(closes==2);
}
static void guest_cleanup_failure(void) {
    uint32_t token,winner;uint64_t key;
    initialize(2);
    CHECK(tsfp_guest_objects_adopt(&store,1,0,0,&token));
    CHECK(tsfp_guest_objects_reference(&store,token,&key));
    CHECK(tsfp_guest_objects_publish(&store,key,0x10000,&winner));
    CHECK(tsfp_guest_objects_close(&store,token));free_result=0;
    CHECK(tsfp_guest_objects_dereference(&store,key));
    CHECK(closes==1 && !frees && free_attempts==1);
    CHECK(tsfp_guest_objects_collect(&store)==1 && closes==1 && attempts==1 && free_attempts==2);
    free_result=1;CHECK(!tsfp_guest_objects_collect(&store));
    CHECK(frees==1 && closes==1 && attempts==1 && free_attempts==3);
}
static void creation_reservations(void) {
    uint32_t ticket,other=0x1234,old;uint64_t key;
    tsfp_guest_object_lease lease={0};
    initialize(1);
    CHECK(tsfp_guest_objects_prepare(&store,7,9,&ticket));old=ticket;
    CHECK(!tsfp_guest_objects_prepare(&store,0,0,&other) && other==0x1234);
    CHECK(!tsfp_guest_objects_adopt(&store,33,0,0,&other) && other==0x1234);
    CHECK(!tsfp_guest_objects_acquire(&store,ticket,&lease));
    CHECK(!tsfp_guest_objects_reference(&store,ticket,&key));
    CHECK(!tsfp_guest_objects_close(&store,ticket));
    CHECK(!tsfp_guest_objects_duplicate(&store,ticket,1,&other));
    CHECK(!tsfp_guest_objects_collect(&store) && !attempts);
    CHECK(!tsfp_guest_objects_commit(&store,ticket,0));
    CHECK(tsfp_guest_objects_cancel(&store,ticket) && !attempts);
    CHECK(!tsfp_guest_objects_cancel(&store,ticket));
    CHECK(tsfp_guest_objects_prepare(&store,7,9,&ticket) && ticket!=old);
    CHECK(!tsfp_guest_objects_commit(&store,old,33));
    CHECK(tsfp_guest_objects_commit(&store,ticket,33));
    CHECK(!tsfp_guest_objects_commit(&store,ticket,44));
    CHECK(!tsfp_guest_objects_cancel(&store,ticket));
    CHECK(tsfp_guest_objects_acquire(&store,ticket,&lease));
    CHECK(lease.host==33 && lease.kind==7 && lease.flags==9);
    CHECK(tsfp_guest_objects_close(&store,ticket) && !closes);
    CHECK(tsfp_guest_objects_release(&lease) && closes==1);
}
static void dispatcher_collision(void) {
    uint32_t a,b,winner=0;uint64_t ka,kb;
    initialize(2);
    CHECK(tsfp_guest_objects_adopt(&store,1,0,0,&a));CHECK(tsfp_guest_objects_adopt(&store,2,0,0,&b));
    CHECK(tsfp_guest_objects_reference(&store,a,&ka));CHECK(tsfp_guest_objects_reference(&store,b,&kb));
    CHECK(tsfp_guest_objects_publish(&store,ka,0x10000,&winner));winner=0xabcdef;
    CHECK(!tsfp_guest_objects_publish(&store,kb,0x10000,&winner) && winner==0xabcdef);
    CHECK(tsfp_guest_objects_publish(&store,kb,0x20000,&winner) && winner==0x20000);
    CHECK(tsfp_guest_objects_dereference_dispatcher(&store,0x10000));
    CHECK(!tsfp_guest_objects_dereference_dispatcher(&store,0x10000));
    CHECK(tsfp_guest_objects_close(&store,a));CHECK(closes==1 && frees==1);
    CHECK(tsfp_guest_objects_close(&store,b));CHECK(closes==1);
    CHECK(tsfp_guest_objects_dereference_dispatcher(&store,0x20000));CHECK(closes==2 && frees==2);
}

#if defined(_WIN32)
typedef volatile LONG flag;
static long read_flag(flag *p) { return InterlockedCompareExchange(p,0,0); }
static void write_flag(flag *p,long n) { (void)InterlockedExchange(p,n); }
static void increment_flag(flag *p) { (void)InterlockedIncrement(p); }
static void yield_thread(void) { Sleep(0); }
#define THREAD_RESULT DWORD WINAPI
#define THREAD_RETURN return 0
#else
typedef long flag;
static long read_flag(flag *p) { return __atomic_load_n(p,__ATOMIC_ACQUIRE); }
static void write_flag(flag *p,long n) { __atomic_store_n(p,n,__ATOMIC_RELEASE); }
static void increment_flag(flag *p) { (void)__atomic_add_fetch(p,1,__ATOMIC_ACQ_REL); }
static void yield_thread(void) { sched_yield(); }
#define THREAD_RESULT void *
#define THREAD_RETURN return NULL
#endif
static flag acquired,release_now,closed;
static uint32_t worker_token;
static int threaded_close(void *ctx,uintptr_t host) {
    CHECK(ctx==&store && host==99);CHECK(!read_flag(&closed));write_flag(&closed,1);return 1;
}
static THREAD_RESULT worker(void *unused) {
    tsfp_guest_object_lease lease={0};(void)unused;
    CHECK(tsfp_guest_objects_acquire(&store,worker_token,&lease));
    write_flag(&acquired,1);
    while(!read_flag(&release_now)) yield_thread();
    CHECK(lease.host==99 && !read_flag(&closed));CHECK(tsfp_guest_objects_release(&lease));
    THREAD_RETURN;
}
static void concurrent_close(void) {
    tsfp_guest_object_lease rejected={0};
#if defined(_WIN32)
    HANDLE thread;
#else
    pthread_t thread;
#endif
    initialize(2);store.close_host=threaded_close;
    CHECK(tsfp_guest_objects_adopt(&store,99,0,0,&worker_token));
#if defined(_WIN32)
    thread=CreateThread(NULL,0,worker,NULL,0,NULL);CHECK(thread!=NULL);
#else
    CHECK(!pthread_create(&thread,NULL,worker,NULL));
#endif
    while(!read_flag(&acquired)) yield_thread();
    CHECK(tsfp_guest_objects_close(&store,worker_token));
    CHECK(!tsfp_guest_objects_acquire(&store,worker_token,&rejected) && !read_flag(&closed));
    write_flag(&release_now,1);
#if defined(_WIN32)
    CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);CHECK(CloseHandle(thread));
#else
    CHECK(!pthread_join(thread,NULL));
#endif
    CHECK(read_flag(&closed));CHECK(!tsfp_guest_objects_collect(&store));
}
static flag stress_ready,stress_start,stress_closed,stress_freed;
static uint32_t stress_root;
static int stress_close(void *ctx,uintptr_t host) {
    CHECK(ctx==&store && (host==77 || (host>=1000 && host<1004)));
    increment_flag(&stress_closed);return 1;
}
static int stress_free(void *ctx,uint32_t address,uint32_t kind,uint32_t flags) {
    CHECK(ctx==&store && address>=0x10000 && address<0x10100);
    CHECK(kind==2 && flags==8);
    increment_flag(&stress_freed);return 1;
}
static THREAD_RESULT stress_worker(void *parameter) {
    unsigned i;uintptr_t id=(uintptr_t)parameter;
    increment_flag(&stress_ready);
    while(!read_flag(&stress_start)) yield_thread();
    for(i=0;i<100;++i) {
        uint32_t token,ticket,winner;uint64_t key;
        tsfp_guest_object_lease lease={0};
        CHECK(tsfp_guest_objects_duplicate(&store,stress_root,0,&token));
        CHECK(tsfp_guest_objects_reference(&store,token,&key));
        CHECK(tsfp_guest_objects_publish(&store,key,0x10000+(uint32_t)id*0x40,&winner));
        CHECK(winner>=0x10000 && winner<0x10100);
        CHECK(tsfp_guest_objects_acquire(&store,token,&lease) && lease.host==77 && lease.dispatcher==winner);
        CHECK(tsfp_guest_objects_close(&store,token));
        CHECK(tsfp_guest_objects_dereference(&store,key));
        CHECK(tsfp_guest_objects_release(&lease));
        CHECK(tsfp_guest_objects_prepare(&store,4,0,&ticket));
        CHECK(tsfp_guest_objects_commit(&store,ticket,1000+id));
        CHECK(tsfp_guest_objects_close(&store,ticket));
    }
    THREAD_RETURN;
}
static void concurrent_publication(void) {
    unsigned i;
#if defined(_WIN32)
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    initialize(16);store.close_host=stress_close;store.free_dispatcher=stress_free;
    CHECK(tsfp_guest_objects_adopt(&store,77,2,8,&stress_root));
    for(i=0;i<4;++i) {
#if defined(_WIN32)
        threads[i]=CreateThread(NULL,0,stress_worker,(void *)(uintptr_t)i,0,NULL);CHECK(threads[i]);
#else
        CHECK(!pthread_create(&threads[i],NULL,stress_worker,(void *)(uintptr_t)i));
#endif
    }
    while(read_flag(&stress_ready)!=4) yield_thread();
    write_flag(&stress_start,1);
    for(i=0;i<4;++i) {
#if defined(_WIN32)
        CHECK(WaitForSingleObject(threads[i],5000)==WAIT_OBJECT_0);CHECK(CloseHandle(threads[i]));
#else
        CHECK(!pthread_join(threads[i],NULL));
#endif
    }
    CHECK(read_flag(&stress_closed)==400 && !read_flag(&stress_freed));
    CHECK(tsfp_guest_objects_close(&store,stress_root));
    CHECK(read_flag(&stress_closed)==401 && read_flag(&stress_freed)==1);
    CHECK(!tsfp_guest_objects_collect(&store));
}
int main(void) {
    lifecycle();capacity_and_generation();callback_reentry();guest_cleanup_failure();creation_reservations();dispatcher_collision();concurrent_close();concurrent_publication();cleanup_failures();
    puts("PASS: guest handles, object references, leases, cleanup and concurrent close");return 0;
}
