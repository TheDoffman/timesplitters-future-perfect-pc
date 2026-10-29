#include "guest_io_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION queue_mutex,owner_mutex[8];
#else
#include <pthread.h>
static pthread_mutex_t queue_mutex,owner_mutex[8];
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"queue line%d %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {tsfp_io_owner owner;unsigned char bytes[128];unsigned depth,freed;} packet;
static packet packets[8];static tsfp_io_queue queue;
static tsfp_io_owner *entries[8];static unsigned depth,disposed;
static void qlock(void *c) {
#ifdef _WIN32
    EnterCriticalSection(&queue_mutex);
#else
    CHECK(!pthread_mutex_lock(&queue_mutex));
#endif
    CHECK(c==&queue && !depth);++depth;
}
static void qunlock(void *c) {
    CHECK(c==&queue && depth==1);--depth;
#ifdef _WIN32
    LeaveCriticalSection(&queue_mutex);
#else
    CHECK(!pthread_mutex_unlock(&queue_mutex));
#endif
}
static void olock(void *c) {packet *p=c;
#ifdef _WIN32
    EnterCriticalSection(&owner_mutex[p-packets]);
#else
    CHECK(!pthread_mutex_lock(&owner_mutex[p-packets]));
#endif
    CHECK(!p->depth);++p->depth;
}
static void ounlock(void *c) {packet *p=c;CHECK(p->depth==1);--p->depth;
#ifdef _WIN32
    LeaveCriticalSection(&owner_mutex[p-packets]);
#else
    CHECK(!pthread_mutex_unlock(&owner_mutex[p-packets]));
#endif
}
static void dispose(void *c,uint32_t address,void *bytes,size_t span) {
    packet *p=c;CHECK(!depth && !p->depth && !p->freed && bytes==p->bytes && span==128);
    CHECK(address==p->owner.address && p->owner.phase==TSFP_IO_DEAD);
    ++p->freed;++disposed;memset(bytes,0xdd,span);
    /* Release must not hold the queue lock, including when stop drains it. */
    tsfp_io_queue_stop(&queue);
}
static void init(void) {
    unsigned i;CHECK(!depth);memset(packets,0,sizeof(packets));disposed=0;
    CHECK(tsfp_io_queue_init(&queue,entries,3,&queue,qlock,qunlock));
    for(i=0;i<8;++i) {
        tsfp_io_owner_ops ops={&packets[i],olock,ounlock,dispose};
        CHECK(tsfp_io_owner_init(&packets[i].owner,&ops,packets[i].bytes,128,0x1000+i*0x100,128,1,6));
    }
}
static tsfp_io_owner *at(unsigned i) {return &packets[i].owner;}
static void begin(unsigned i) {
    tsfp_io_ticket t={0};CHECK(tsfp_io_owner_dispatch_begin(at(i),&t));
    CHECK(tsfp_io_owner_dispatch_publish(&t));CHECK(tsfp_io_owner_dispatch_end(&t,0));
}
static void complete(unsigned i) {CHECK(tsfp_io_owner_complete(at(i),0,i,NULL,NULL));}
static void finish(void) {
    unsigned i;tsfp_io_owner *selected=(tsfp_io_owner *)1;
    tsfp_io_queue_stop(&queue);
    if(queue.current) {
        tsfp_io_owner *current=queue.current;
        CHECK(tsfp_io_owner_retain(current));CHECK(tsfp_io_queue_next(&queue,current,&selected) && !selected);
        CHECK(tsfp_io_owner_release(current));
    }
    for(i=0;i<8;++i) {
        tsfp_io_owner *o=at(i);
        if(o->phase==TSFP_IO_ACTIVE || o->phase==TSFP_IO_PENDING)complete(i);
        if(o->clients)CHECK(o->clients==1 && tsfp_io_owner_release(o));
        CHECK(packets[i].freed==1);
    }
    CHECK(disposed==8 && !queue.current && !queue.count);
}
static void fifo(void) {
    unsigned i;tsfp_io_owner *selected=(tsfp_io_owner *)1,*old;
    init();CHECK(!tsfp_io_queue_push(&queue,at(0)) && at(0)->clients==1);
    for(i=0;i<8;++i)begin(i);
    CHECK(tsfp_io_queue_push(&queue,at(0)) && tsfp_io_queue_push(&queue,at(1)));
    CHECK(!tsfp_io_queue_push(&queue,at(1)) && at(1)->clients==2);
    CHECK(tsfp_io_queue_push(&queue,at(2)));
    CHECK(!tsfp_io_queue_push(&queue,at(3)) && at(3)->clients==1);
    CHECK(!tsfp_io_queue_push(&queue,at(1)) && at(1)->clients==2);
    CHECK(!tsfp_io_queue_next(&queue,at(7),&selected) && selected==(tsfp_io_owner *)1 && queue.count==3);
    CHECK(tsfp_io_queue_next(&queue,NULL,&selected) && selected==at(0) && selected->clients==3);
    CHECK(!tsfp_io_queue_push(&queue,at(0)));
    CHECK(tsfp_io_queue_push(&queue,at(3))); /* wrapped tail */
    for(i=1;i<5;++i) {
        old=selected;CHECK(tsfp_io_queue_next(&queue,old,&selected) && selected==at(i));
        CHECK(old->clients==2 && tsfp_io_owner_release(old));
        if(i==1)CHECK(tsfp_io_queue_push(&queue,at(4))); /* nonzero wrapped index */
    }
    old=selected;CHECK(tsfp_io_queue_next(&queue,old,&selected) && !selected);
    CHECK(tsfp_io_owner_release(old));
    for(i=0;i<3;++i)CHECK(tsfp_io_queue_push(&queue,at(i+5)));
    CHECK(tsfp_io_queue_next(&queue,NULL,&selected) && selected==at(5));
    CHECK(tsfp_io_owner_release(selected));finish();
}
static void pinned_callback(void) {
    tsfp_io_ticket callback={0};tsfp_io_owner *selected,*empty=(tsfp_io_owner *)1;
    init();begin(0);CHECK(tsfp_io_queue_push(&queue,at(0)));
    CHECK(tsfp_io_queue_next(&queue,NULL,&selected) && selected==at(0));
    CHECK(tsfp_io_owner_forward_begin(selected,&callback));
    CHECK(tsfp_io_owner_dispatch_publish(&callback));
    CHECK(tsfp_io_owner_release(at(0))); /* creator */
    complete(0);CHECK(!packets[0].freed && selected->internal==1);
    CHECK(tsfp_io_queue_next(&queue,selected,&empty) && !empty);
    CHECK(tsfp_io_owner_release(selected));CHECK(!packets[0].freed && !at(0)->clients);
    CHECK(tsfp_io_owner_dispatch_end(&callback,0));CHECK(packets[0].freed==1);
    finish();
}
static void stop_cases(void) {
    tsfp_io_owner *current,*selected;unsigned i;
    init();begin(3);for(i=0;i<3;++i){begin(i);CHECK(tsfp_io_queue_push(&queue,at(i)));}
    CHECK(tsfp_io_queue_next(&queue,NULL,&current) && current==at(0));
    /* A completed waiting owner remains alive until stop releases its queue ref. */
    complete(1);CHECK(tsfp_io_owner_release(at(1)) && !packets[1].freed);
    CHECK(tsfp_io_owner_release(at(2)));CHECK(tsfp_io_owner_pending(at(2)));
    tsfp_io_queue_stop(&queue);CHECK(queue.closed && !queue.count && queue.current==current);
    CHECK(packets[1].freed==1 && !packets[2].freed && !at(2)->clients && at(2)->internal==1);
    CHECK(!tsfp_io_queue_push(&queue,at(3)));
    CHECK(tsfp_io_queue_next(&queue,current,&selected) && !selected);
    CHECK(tsfp_io_owner_release(current));finish();
}
static void failures(void) {
    tsfp_io_owner *selected=(tsfp_io_owner *)1;tsfp_io_queue snapshot;
    init();memcpy(&snapshot,&queue,sizeof(queue));
    CHECK(!tsfp_io_queue_init(&queue,entries,0,&queue,qlock,qunlock));
    CHECK(!tsfp_io_queue_init(&queue,entries,SIZE_MAX,&queue,qlock,qunlock));
    CHECK(!memcmp(&snapshot,&queue,sizeof(queue)));
    begin(0);CHECK(tsfp_io_queue_push(&queue,at(0)));
    at(0)->clients=UINT32_MAX-2;
    CHECK(!tsfp_io_queue_next(&queue,NULL,&selected) && selected==(tsfp_io_owner *)1);
    CHECK(queue.count==1 && !queue.current);at(0)->clients=2;
    at(1)->phase=TSFP_IO_UNCERTAIN;CHECK(!tsfp_io_queue_push(&queue,at(1)));at(1)->phase=TSFP_IO_READY;
    finish();
}
/* Multiple host producers borrow their own stable creator refs. Queue order is
 * lock acquisition order; acceptance requires each identity exactly once. */
#ifdef _WIN32
static DWORD WINAPI producer(void *context) {
#else
static void *producer(void *context) {
#endif
    CHECK(tsfp_io_queue_push(&queue,context));return 0;
}
static void concurrent(void) {
    unsigned i,seen=0;tsfp_io_owner *current=NULL,*selected;
#ifdef _WIN32
    HANDLE workers[8];
#else
    pthread_t workers[8];
#endif
    init();CHECK(tsfp_io_queue_init(&queue,entries,8,&queue,qlock,qunlock));
    for(i=0;i<8;++i)begin(i);
    for(i=0;i<8;++i) {
#ifdef _WIN32
        workers[i]=CreateThread(NULL,0,producer,at(i),0,NULL);CHECK(workers[i]);
#else
        CHECK(!pthread_create(&workers[i],NULL,producer,at(i)));
#endif
    }
    for(i=0;i<8;++i) {
#ifdef _WIN32
        DWORD code;CHECK(WaitForSingleObject(workers[i],10000)==WAIT_OBJECT_0);
        CHECK(GetExitCodeThread(workers[i],&code) && !code);CHECK(CloseHandle(workers[i]));
#else
        CHECK(!pthread_join(workers[i],NULL));
#endif
    }
    CHECK(queue.count==8);
    for(i=0;i<8;++i) {
        unsigned index;
        CHECK(tsfp_io_queue_next(&queue,current,&selected) && selected);
        if(current)CHECK(tsfp_io_owner_release(current));
        current=selected;
        for(index=0;index<8 && selected!=at(index);++index){}
        CHECK(index<8 && !(seen&(1u<<index)));seen|=1u<<index;
    }
    CHECK(seen==255);CHECK(tsfp_io_queue_next(&queue,current,&selected) && !selected);
    CHECK(tsfp_io_owner_release(current));finish();
}
int main(void) {
    unsigned i;
#ifdef _WIN32
    InitializeCriticalSection(&queue_mutex);for(i=0;i<8;++i)InitializeCriticalSection(&owner_mutex[i]);
#else
    CHECK(!pthread_mutex_init(&queue_mutex,NULL));for(i=0;i<8;++i)CHECK(!pthread_mutex_init(&owner_mutex[i],NULL));
#endif
    fifo();pinned_callback();stop_cases();failures();concurrent();
#ifdef _WIN32
    for(i=0;i<8;++i)DeleteCriticalSection(&owner_mutex[i]);DeleteCriticalSection(&queue_mutex);
#else
    for(i=0;i<8;++i)CHECK(!pthread_mutex_destroy(&owner_mutex[i]));CHECK(!pthread_mutex_destroy(&queue_mutex));
#endif
    puts("PASS: owned FIFO/wrap/identity, independent callback pin, reentrant disposal, closed admission and retained pending operations");return 0;
}
