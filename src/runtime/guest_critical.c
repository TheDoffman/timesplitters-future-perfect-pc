/* Original shared identity/ownership/lifetime policy for recursive host locks. */
#include "guest_critical.h"
#include <string.h>
static uintptr_t critical_key(const tsfp_critical_registry *r,uintptr_t key){return r->single && key?1:key;}
static tsfp_critical_record *critical_find(tsfp_critical_registry *r,uintptr_t key) {
    tsfp_critical_record *p=r->buckets[(key^(key>>7))%r->bucket_count];
    while(p && p->key!=key)p=p->next;return p;
}
static enum tsfp_critical_result critical_begin(tsfp_critical_registry *r,uintptr_t key,int create,tsfp_critical_record **out) {
    tsfp_critical_record *p;size_t bucket;
    if(!r->ready)return TSFP_CRITICAL_CLOSED;
    if(r->active==UINT32_MAX)return TSFP_CRITICAL_LIMIT;
    p=critical_find(r,key);
    if(!p && create) {
        p=r->ops.create(r->ops.context);if(!p)return TSFP_CRITICAL_NO_MEMORY;
        bucket=(key^(key>>7))%r->bucket_count;
        p->key=key;p->owner=0;p->depth=p->site=0;p->next=r->buckets[bucket];
        r->buckets[bucket]=p;++r->live;
    }
    if(!p)return TSFP_CRITICAL_NOT_OWNER;
    ++r->active;*out=p;return TSFP_CRITICAL_OK;
}
int tsfp_critical_init(tsfp_critical_registry *r,tsfp_critical_record **buckets,size_t count,const tsfp_critical_ops *ops,int single) {
    if(!r || !buckets || !count || count>SIZE_MAX/sizeof(*buckets) || !ops || !ops->lock || !ops->unlock ||
       !ops->create || !ops->dispose || !ops->acquire || !ops->release || !ops->thread)return 0;
    memset(r,0,sizeof(*r));memset(buckets,0,count*sizeof(*buckets));r->ops=*ops;r->buckets=buckets;r->bucket_count=count;
    r->single=single!=0;r->ready=r->configured=1;return 1;
}
enum tsfp_critical_result tsfp_critical_initialize(tsfp_critical_registry *r,uintptr_t key) {
    enum tsfp_critical_result result;tsfp_critical_record *p;
    if(!r || !r->configured || !key)return TSFP_CRITICAL_INVALID;
    key=critical_key(r,key);r->ops.lock(r->ops.context);result=critical_begin(r,key,1,&p);
    if(result==TSFP_CRITICAL_OK)--r->active;
    r->ops.unlock(r->ops.context);return result;
}
enum tsfp_critical_result tsfp_critical_enter(tsfp_critical_registry *r,uintptr_t key,uint32_t site,int try_only) {
    enum tsfp_critical_result result;tsfp_critical_record *p;uintptr_t thread;int acquired;
    if(!r || !r->configured || !key)return TSFP_CRITICAL_INVALID;
    thread=r->ops.thread(r->ops.context);if(!thread)return TSFP_CRITICAL_INVALID;
    key=critical_key(r,key);r->ops.lock(r->ops.context);result=critical_begin(r,key,1,&p);
    if(result!=TSFP_CRITICAL_OK){r->ops.unlock(r->ops.context);return result;}
    if(p->owner==thread && p->depth==UINT32_MAX){--r->active;r->ops.unlock(r->ops.context);return TSFP_CRITICAL_LIMIT;}
    r->ops.unlock(r->ops.context);acquired=r->ops.acquire(r->ops.context,p,try_only!=0);
    r->ops.lock(r->ops.context);
    if(acquired==1) {
        if((p->owner && p->owner!=thread) || p->depth==UINT32_MAX) {
            r->ops.release(r->ops.context,p);result=TSFP_CRITICAL_HOST_FAILURE;
        } else {
            if(!p->depth){++r->held;p->site=site;}
            p->owner=thread;++p->depth;
            if(r->enters!=UINT64_MAX)++r->enters;
        }
    } else {
        result=acquired==0 && try_only?TSFP_CRITICAL_BUSY:TSFP_CRITICAL_HOST_FAILURE;
        if(result==TSFP_CRITICAL_BUSY && r->busy!=UINT64_MAX)++r->busy;
    }
    --r->active;r->ops.unlock(r->ops.context);return result;
}
enum tsfp_critical_result tsfp_critical_leave(tsfp_critical_registry *r,uintptr_t key) {
    enum tsfp_critical_result result;tsfp_critical_record *p;uintptr_t thread;
    if(!r || !r->configured || !key)return TSFP_CRITICAL_INVALID;
    thread=r->ops.thread(r->ops.context);if(!thread)return TSFP_CRITICAL_INVALID;
    key=critical_key(r,key);r->ops.lock(r->ops.context);result=critical_begin(r,key,0,&p);
    if(result!=TSFP_CRITICAL_OK){r->ops.unlock(r->ops.context);return result;}
    if(p->owner!=thread || !p->depth){--r->active;r->ops.unlock(r->ops.context);return TSFP_CRITICAL_NOT_OWNER;}
    --p->depth;if(!p->depth){p->owner=0;p->site=0;--r->held;}
    r->ops.unlock(r->ops.context);r->ops.release(r->ops.context,p);
    r->ops.lock(r->ops.context);--r->active;if(r->leaves!=UINT64_MAX)++r->leaves;r->ops.unlock(r->ops.context);return TSFP_CRITICAL_OK;
}
int tsfp_critical_snapshot(tsfp_critical_registry *r,uintptr_t key,tsfp_critical_view *output) {
    tsfp_critical_view view={0};tsfp_critical_record *p=NULL;
    if(!r || !r->configured || !output)return 0;
    key=critical_key(r,key);r->ops.lock(r->ops.context);
    if(key)p=critical_find(r,key);
    view.live=r->live;view.held=r->held;view.quarantined=r->quarantined;view.active=r->active;view.ready=r->ready;
    view.enters=r->enters;view.leaves=r->leaves;view.busy=r->busy;
    if(p){view.found=1;view.depth=p->depth;view.owner=p->owner;view.site=p->site;}
    r->ops.unlock(r->ops.context);*output=view;return 1;
}
int tsfp_critical_stop(tsfp_critical_registry *r) {
    tsfp_critical_record *list=NULL,*p,*next;size_t i;int ok=1;
    if(!r || !r->configured)return 0;
    r->ops.lock(r->ops.context);
    if(!r->ready){ok=r->quarantined==0;r->ops.unlock(r->ops.context);return ok;}
    if(r->active || r->held){r->ops.unlock(r->ops.context);return 0;}
    r->ready=0;r->live=0;
    for(i=0;i<r->bucket_count;++i) {
        p=r->buckets[i];r->buckets[i]=NULL;
        while(p){next=p->next;p->next=list;list=p;p=next;}
    }
    r->ops.unlock(r->ops.context);
    for(p=list;p;p=next) {
        next=p->next;
        if(!r->ops.dispose(r->ops.context,p)) {
            r->ops.lock(r->ops.context);p->next=r->quarantine;r->quarantine=p;++r->quarantined;r->ops.unlock(r->ops.context);ok=0;
        }
    }
    return ok;
}
