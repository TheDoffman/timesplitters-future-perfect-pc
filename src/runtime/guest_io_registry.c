#include "guest_io_registry.h"
#include <string.h>
int tsfp_io_registry_init(tsfp_io_registry *r,tsfp_io_registry_entry *entries,size_t count,
    void *context,void (*lock)(void *),void (*unlock)(void *)) {
    tsfp_io_registry candidate;
    if(!r || !entries || !count || count>SIZE_MAX/sizeof(*entries) || !lock || !unlock)return 0;
    memset(&candidate,0,sizeof(candidate));candidate.entries=entries;candidate.count=count;
    candidate.context=context;candidate.lock=lock;candidate.unlock=unlock;
    memset(entries,0,count*sizeof(*entries));*r=candidate;return 1;
}
int tsfp_io_registry_publish(tsfp_io_registry *r,tsfp_io_owner *owner) {
    size_t i,free_slot;int ok=0;if(!r || !owner)return 0;
    r->lock(r->context);free_slot=r->count;
    if(!r->closed) {
        for(i=0;i<r->count;++i) {
            if(r->entries[i].owner==owner || (r->entries[i].owner && r->entries[i].address==owner->address))break;
            if(!r->entries[i].owner && free_slot==r->count)free_slot=i;
        }
        if(i==r->count && free_slot<r->count && tsfp_io_owner_retain(owner)) {
            r->entries[free_slot].address=owner->address;r->entries[free_slot].owner=owner;ok=1;
        }
    }
    r->unlock(r->context);return ok;
}
tsfp_io_owner *tsfp_io_registry_acquire(tsfp_io_registry *r,uint32_t address) {
    size_t i;tsfp_io_owner *owner=NULL;if(!r || !address)return NULL;
    r->lock(r->context);
    if(!r->closed)for(i=0;i<r->count;++i) {
        if(r->entries[i].owner && r->entries[i].address==address) {
            if(tsfp_io_owner_retain(r->entries[i].owner))owner=r->entries[i].owner;
            break;
        }
    }
    r->unlock(r->context);return owner;
}
int tsfp_io_registry_remove(tsfp_io_registry *r,uint32_t address) {
    size_t i;int found;tsfp_io_owner *owner=NULL;if(!r || !address)return 0;
    r->lock(r->context);
    for(i=0;i<r->count;++i)if(r->entries[i].owner && r->entries[i].address==address) {
        owner=r->entries[i].owner;r->entries[i].owner=NULL;r->entries[i].address=0;break;
    }
    r->unlock(r->context);
    found=owner!=NULL;
    if(owner)tsfp_io_owner_release(owner);
    return found;
}
void tsfp_io_registry_stop(tsfp_io_registry *r) {
    size_t i;tsfp_io_owner *owner;if(!r)return;
    r->lock(r->context);r->closed=1;r->unlock(r->context);
    for(;;) {
        owner=NULL;r->lock(r->context);
        for(i=0;i<r->count;++i)if(r->entries[i].owner) {
            owner=r->entries[i].owner;r->entries[i].owner=NULL;r->entries[i].address=0;break;
        }
        r->unlock(r->context);
        if(!owner)break;
        tsfp_io_owner_release(owner);
    }
}
