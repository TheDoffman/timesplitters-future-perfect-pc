#include "guest_io_pool.h"
#include <string.h>
struct tsfp_io_pool_node {
    tsfp_io_owner owner;tsfp_io_pool *pool;uint32_t address;
    int cleanup;tsfp_io_pool_node *next;
};
static void owner_lock(void *context) {
    tsfp_io_pool_node *n=(tsfp_io_pool_node *)context;n->pool->ops.lock(n->pool->ops.context);
}
static void owner_unlock(void *context) {
    tsfp_io_pool_node *n=(tsfp_io_pool_node *)context;n->pool->ops.unlock(n->pool->ops.context);
}
static void drop_metadata(tsfp_io_pool_node *n) {
    tsfp_io_pool *p=n->pool;
    p->ops.lock(p->ops.context);--p->live;p->ops.unlock(p->ops.context);
    p->ops.metadata_free(p->ops.context,n);
}
static int cleanup(tsfp_io_pool_node *n) {
    tsfp_io_pool *p=n->pool;int result=p->ops.deallocate(p->ops.context,n->address);
    if(result==1) {drop_metadata(n);return 1;}
    p->ops.lock(p->ops.context);n->cleanup=result==0?0:-1;
    n->next=p->failed;p->failed=n;p->ops.unlock(p->ops.context);return 0;
}
static void owner_dispose(void *context,uint32_t address,void *bytes,size_t span) {
    tsfp_io_pool_node *n=(tsfp_io_pool_node *)context;
    (void)address;(void)bytes;(void)span;(void)cleanup(n);
}
int tsfp_io_pool_init(tsfp_io_pool *p,tsfp_io_registry_entry *entries,size_t count,const tsfp_io_pool_ops *ops) {
    tsfp_io_pool candidate;
    if(!p || !ops || !ops->lock || !ops->unlock || !ops->registry_lock || !ops->registry_unlock ||
       !ops->metadata_allocate || !ops->metadata_free || !ops->allocate || !ops->resolve || !ops->deallocate)return 0;
    memset(&candidate,0,sizeof(candidate));candidate.ops=*ops;
    if(!tsfp_io_registry_init(&candidate.registry,entries,count,ops->context,ops->registry_lock,ops->registry_unlock))return 0;
    *p=candidate;return 1;
}
uint32_t tsfp_io_pool_allocate(tsfp_io_pool *p,uint32_t stacks) {
    tsfp_io_pool_node *n;tsfp_io_owner_ops ops;uint32_t bytes,address;void *mapping;
    if(!p || stacks>TSFP_IO_MAX_STACKS)return 0;
    bytes=TSFP_IO_REQUEST_BYTES+stacks*TSFP_IO_STACK_BYTES;
    p->ops.lock(p->ops.context);
    if(p->closed || p->live>=p->registry.count) {p->ops.unlock(p->ops.context);return 0;}
    ++p->live;p->ops.unlock(p->ops.context);
    n=(tsfp_io_pool_node *)p->ops.metadata_allocate(p->ops.context,sizeof(*n));
    if(!n) {
        p->ops.lock(p->ops.context);--p->live;p->ops.unlock(p->ops.context);return 0;
    }
    memset(n,0,sizeof(*n));n->pool=p;
    address=p->ops.allocate(p->ops.context,bytes);n->address=address;
    if(!address) {drop_metadata(n);return 0;}
    mapping=p->ops.resolve(p->ops.context,address,bytes);
    ops.context=n;ops.lock=owner_lock;ops.unlock=owner_unlock;ops.dispose=owner_dispose;
    if(!mapping || !tsfp_io_owner_init(&n->owner,&ops,mapping,bytes,address,bytes,stacks,TSFP_IO_REQUEST_KIND)) {
        (void)cleanup(n);return 0;
    }
    if(!tsfp_io_registry_publish(&p->registry,&n->owner)) {
        tsfp_io_owner_release(&n->owner);return 0;
    }
    /* Retain the creator until registration completes. This release can dispose
     * metadata if a concurrent stop already detached the registry reference. */
    tsfp_io_owner_release(&n->owner);return address;
}
void tsfp_io_pool_stop(tsfp_io_pool *p) {
    if(!p)return;
    p->ops.lock(p->ops.context);p->closed=1;p->ops.unlock(p->ops.context);
    tsfp_io_registry_stop(&p->registry);
}
int tsfp_io_pool_retry_one(tsfp_io_pool *p) {
    tsfp_io_pool_node **link,*n;if(!p)return 0;
    p->ops.lock(p->ops.context);link=&p->failed;
    while(*link && (*link)->cleanup!=0)link=&(*link)->next;
    n=*link;if(n)*link=n->next;p->ops.unlock(p->ops.context);
    return n?cleanup(n):0;
}
void tsfp_io_pool_counts(tsfp_io_pool *p,size_t *live,size_t *retained,size_t *uncertain) {
    size_t a=0,b=0;tsfp_io_pool_node *n;if(!p || !live || !retained || !uncertain)return;
    p->ops.lock(p->ops.context);
    for(n=p->failed;n;n=n->next) {if(n->cleanup==0)++a;else ++b;}
    *live=p->live;*retained=a;*uncertain=b;p->ops.unlock(p->ops.context);
}
