#include "guest_io_queue.h"
#include <string.h>
int tsfp_io_queue_init(tsfp_io_queue *q,tsfp_io_owner **entries,size_t capacity,
    void *context,void (*lock)(void *),void (*unlock)(void *)) {
    tsfp_io_queue value;
    if(!q || !entries || !capacity || capacity>SIZE_MAX/sizeof(*entries) || !lock || !unlock)return 0;
    memset(&value,0,sizeof(value));value.entries=entries;value.capacity=capacity;
    value.context=context;value.lock=lock;value.unlock=unlock;
    memset(entries,0,capacity*sizeof(*entries));*q=value;return 1;
}
/* Avoid head+count overflow even for the largest representable capacity. */
static size_t index_at(const tsfp_io_queue *q,size_t offset) {
    size_t remaining=q->capacity-q->head;
    return offset<remaining?q->head+offset:offset-remaining;
}
int tsfp_io_queue_push(tsfp_io_queue *q,tsfp_io_owner *owner) {
    size_t i;tsfp_io_phase phase;int ok=0;
    if(!q || !owner)return 0;
    q->lock(q->context);
    if(q->closed || q->count==q->capacity || q->current==owner)goto done;
    for(i=0;i<q->count;++i)if(q->entries[index_at(q,i)]==owner)goto done;
    phase=tsfp_io_owner_phase(owner);
    if(phase!=TSFP_IO_ACTIVE && phase!=TSFP_IO_PENDING)goto done;
    if(!tsfp_io_owner_retain(owner))goto done;
    q->entries[index_at(q,q->count)]=owner;++q->count;ok=1;
done:
    q->unlock(q->context);return ok;
}
int tsfp_io_queue_next(tsfp_io_queue *q,tsfp_io_owner *expected,tsfp_io_owner **selected) {
    tsfp_io_owner *next=NULL,*previous;int ok=0;
    if(!q || !selected)return 0;
    q->lock(q->context);
    if(q->current!=expected)goto done;
    if(!q->closed && q->count) {
        next=q->entries[q->head];
        if(!tsfp_io_owner_retain(next))goto done;
        q->entries[q->head]=NULL;
        q->head=q->head==q->capacity-1?0:q->head+1;--q->count;
    }
    previous=q->current;q->current=next;*selected=next;ok=1;
    q->unlock(q->context);
    if(previous)(void)tsfp_io_owner_release(previous);
    return ok;
done:
    q->unlock(q->context);return 0;
}
void tsfp_io_queue_stop(tsfp_io_queue *q) {
    tsfp_io_owner *owner;if(!q)return;
    q->lock(q->context);q->closed=1;
    while(q->count) {
        owner=q->entries[q->head];q->entries[q->head]=NULL;
        q->head=q->head==q->capacity-1?0:q->head+1;--q->count;
        q->unlock(q->context);(void)tsfp_io_owner_release(owner);q->lock(q->context);
    }
    q->unlock(q->context);
}
