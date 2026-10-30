#include "guest_critical.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"critical line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct fake_node {tsfp_critical_record record;uintptr_t owner;uint32_t depth;int disposed;} fake_node;
typedef struct fake_host {
    tsfp_critical_registry registry;tsfp_critical_record *buckets[17];fake_node nodes[10010];
    uintptr_t thread;unsigned created,disposed,acquires,releases;int guard,fail_new,fail_dispose,acquire_stop,release_stop,host_failure;
} fake_host;
static fake_host host;
static void lock(void *p){fake_host *h=p;CHECK(!h->guard);h->guard=1;}
static void unlock(void *p){fake_host *h=p;CHECK(h->guard);h->guard=0;}
static tsfp_critical_record *create(void *p) {
    fake_host *h=p;fake_node *n;CHECK(h->guard);if(h->fail_new)return NULL;
    CHECK(h->created<10010);n=&h->nodes[h->created++];n->record.native=n;return &n->record;
}
static int dispose(void *p,tsfp_critical_record *record) {
    fake_host *h=p;fake_node *n=record->native;CHECK(!h->guard && !n->depth && !n->disposed);++h->disposed;
    if(h->fail_dispose){--h->fail_dispose;return 0;}n->disposed=1;return 1;
}
static int acquire(void *p,tsfp_critical_record *record,int try_only) {
    fake_host *h=p;fake_node *n=record->native;CHECK(!h->guard && !n->disposed);++h->acquires;
    if(h->acquire_stop){h->acquire_stop=0;CHECK(!tsfp_critical_stop(&h->registry) && h->registry.ready && h->registry.active==1);}
    if(h->host_failure)return -1;
    if(n->depth && n->owner!=h->thread)return try_only?0:-1;
    n->owner=h->thread;++n->depth;return 1;
}
static void release(void *p,tsfp_critical_record *record) {
    fake_host *h=p;fake_node *n=record->native;CHECK(!h->guard && !n->disposed && n->owner==h->thread && n->depth);++h->releases;
    if(h->release_stop){h->release_stop=0;CHECK(!tsfp_critical_stop(&h->registry) && h->registry.ready && h->registry.active==1);}
    if(!--n->depth)n->owner=0;
}
static uintptr_t thread(void *p){return ((fake_host *)p)->thread;}
static void init(int single) {
    tsfp_critical_ops ops={0};memset(&host,0,sizeof(host));host.thread=1;
    ops.context=&host;ops.lock=lock;ops.unlock=unlock;ops.create=create;ops.dispose=dispose;ops.acquire=acquire;ops.release=release;ops.thread=thread;
    CHECK(tsfp_critical_init(&host.registry,host.buckets,17,&ops,single));
}
static tsfp_critical_view view(uintptr_t key){tsfp_critical_view v;CHECK(tsfp_critical_snapshot(&host.registry,key,&v));return v;}
int main(void) {
    uintptr_t key=((uintptr_t)1<<(sizeof(uintptr_t)*8-1))|35;unsigned i,prior;tsfp_critical_record *record;tsfp_critical_view v;
    CHECK(!tsfp_critical_stop(&host.registry));init(0);
    CHECK(tsfp_critical_initialize(&host.registry,0)==TSFP_CRITICAL_INVALID);
    CHECK(tsfp_critical_leave(&host.registry,key)==TSFP_CRITICAL_NOT_OWNER && !host.created);
    CHECK(tsfp_critical_initialize(&host.registry,key)==TSFP_CRITICAL_OK && host.created==1);
    CHECK(tsfp_critical_initialize(&host.registry,key)==TSFP_CRITICAL_OK && host.created==1);
    host.acquire_stop=1;CHECK(tsfp_critical_enter(&host.registry,key,0x12345678,1)==TSFP_CRITICAL_OK);
    v=view(key);CHECK(v.depth==1 && v.owner==1 && v.site==0x12345678 && v.held==1);
    CHECK(tsfp_critical_enter(&host.registry,key,0x76543210,0)==TSFP_CRITICAL_OK);v=view(key);CHECK(v.depth==2 && v.site==0x12345678 && v.enters==2);
    CHECK(!tsfp_critical_stop(&host.registry));host.thread=2;prior=host.releases;
    CHECK(tsfp_critical_enter(&host.registry,key,0,1)==TSFP_CRITICAL_BUSY && view(key).depth==2);
    CHECK(tsfp_critical_leave(&host.registry,key)==TSFP_CRITICAL_NOT_OWNER && host.releases==prior);
    CHECK(tsfp_critical_enter(&host.registry,key+17,0,1)==TSFP_CRITICAL_OK);CHECK(tsfp_critical_leave(&host.registry,key+17)==TSFP_CRITICAL_OK);
    host.thread=1;CHECK(tsfp_critical_leave(&host.registry,key)==TSFP_CRITICAL_OK && view(key).depth==1);
    host.release_stop=1;CHECK(tsfp_critical_leave(&host.registry,key)==TSFP_CRITICAL_OK);v=view(key);CHECK(!v.depth && !v.owner && !v.site && !v.held && !v.active);
    CHECK(tsfp_critical_leave(&host.registry,key)==TSFP_CRITICAL_NOT_OWNER);
    host.fail_new=1;prior=host.created;CHECK(tsfp_critical_initialize(&host.registry,9)==TSFP_CRITICAL_NO_MEMORY && host.created==prior && view(key).live==2);host.fail_new=0;
    host.host_failure=1;CHECK(tsfp_critical_enter(&host.registry,key,0,0)==TSFP_CRITICAL_HOST_FAILURE && !view(key).depth);host.host_failure=0;
    CHECK(tsfp_critical_enter(&host.registry,key,7,1)==TSFP_CRITICAL_OK);record=&host.nodes[0].record;record->depth=UINT32_MAX;prior=host.acquires;
    CHECK(tsfp_critical_enter(&host.registry,key,0,1)==TSFP_CRITICAL_LIMIT && host.acquires==prior);record->depth=1;CHECK(tsfp_critical_leave(&host.registry,key)==TSFP_CRITICAL_OK);
    host.registry.active=UINT32_MAX;CHECK(tsfp_critical_initialize(&host.registry,12)==TSFP_CRITICAL_LIMIT);host.registry.active=0;
    host.thread=0;CHECK(tsfp_critical_enter(&host.registry,key,0,0)==TSFP_CRITICAL_INVALID);host.thread=1;
    for(i=0;i<10000;++i)CHECK(tsfp_critical_initialize(&host.registry,0x100000u+(uintptr_t)i*32)==TSFP_CRITICAL_OK);
    CHECK(view(0).live==10002 && tsfp_critical_stop(&host.registry) && host.disposed==host.created);
    CHECK(tsfp_critical_stop(&host.registry) && tsfp_critical_enter(&host.registry,key,0,0)==TSFP_CRITICAL_CLOSED);
    init(1);CHECK(tsfp_critical_enter(&host.registry,13,1,0)==TSFP_CRITICAL_OK);
    CHECK(tsfp_critical_enter(&host.registry,91,2,1)==TSFP_CRITICAL_OK && view(5).depth==2 && view(5).live==1);
    host.thread=2;CHECK(tsfp_critical_enter(&host.registry,57,0,1)==TSFP_CRITICAL_BUSY);host.thread=1;
    CHECK(tsfp_critical_leave(&host.registry,91)==TSFP_CRITICAL_OK && tsfp_critical_leave(&host.registry,13)==TSFP_CRITICAL_OK && tsfp_critical_stop(&host.registry));
    init(0);CHECK(tsfp_critical_initialize(&host.registry,4)==TSFP_CRITICAL_OK && tsfp_critical_initialize(&host.registry,21)==TSFP_CRITICAL_OK);
    host.fail_dispose=1;CHECK(!tsfp_critical_stop(&host.registry));v=view(0);CHECK(!v.ready && !v.live && v.quarantined==1);prior=host.disposed;
    CHECK(!tsfp_critical_stop(&host.registry) && host.disposed==prior);CHECK(tsfp_critical_initialize(&host.registry,99)==TSFP_CRITICAL_CLOSED);
    puts("PASS: shared recursive ownership, aliases/single mode, collisions/10000 locks, API pins, exhaustion, failure and quarantine");return 0;
}
