#ifndef TSFP_VISIBILITY_QUERIES_H
#define TSFP_VISIBILITY_QUERIES_H
#include <stdint.h>
#include <string.h>
#include <math.h>
/* Original SDK indices wrap at0x4000. Cache completed answers until reuse;
 * keep a bounded pool of asynchronous native objects, rather than allocating
 * one object per frame or returning invented positive/zero visibility. */
#define TSFP_VISIBILITY_IDS 16384u
#define TSFP_VISIBILITY_SLOTS 32u
typedef struct TsfpVisibilityOps {
    void *host;
    void *(*create)(void *);
    void (*destroy)(void *,void *);
    int (*begin)(void *,void *);
    int (*end)(void *,void *);
    int (*result)(void *,void *,uint64_t *);
} TsfpVisibilityOps;
typedef struct TsfpVisibilityEntry {uint32_t pending,ready,pixels;} TsfpVisibilityEntry;
typedef struct TsfpVisibilitySlot {void *query;int32_t owner;double scale;} TsfpVisibilitySlot;
typedef struct TsfpVisibilityQueries {
    TsfpVisibilityEntry entries[TSFP_VISIBILITY_IDS];
    TsfpVisibilitySlot slots[TSFP_VISIBILITY_SLOTS];
    uint32_t active;
} TsfpVisibilityQueries;
static void tsfp_visibility_reset(TsfpVisibilityQueries *s,const TsfpVisibilityOps *o) {
    if(s->active)o->end(o->host,s->slots[s->active-1].query);
    for(unsigned i=0;i<TSFP_VISIBILITY_SLOTS;i++)
        if(s->slots[i].query)o->destroy(o->host,s->slots[i].query);
    memset(s,0,sizeof(*s));
}
static int tsfp_visibility_result(TsfpVisibilityQueries *,const TsfpVisibilityOps *,uint32_t,uint32_t *);
static int tsfp_visibility_begin(TsfpVisibilityQueries *s,const TsfpVisibilityOps *o,double scale) {
    if(s->active || !isfinite(scale) || scale<=0)return 0;
    for(unsigned i=0;i<TSFP_VISIBILITY_SLOTS;i++) {
        TsfpVisibilitySlot *slot=&s->slots[i];
        if(slot->owner==-1) {
            uint64_t unused;
            int result=o->result(o->host,slot->query,&unused);
            if(result) {
                if(result<0){o->destroy(o->host,slot->query);slot->query=NULL;}
                slot->owner=0;
            }
        }
        if(slot->owner>0) {
            uint32_t completed;
            (void)tsfp_visibility_result(s,o,(uint32_t)slot->owner-1,&completed);
        }
        if(slot->owner)continue;
        if(!slot->query)slot->query=o->create(o->host);
        if(!slot->query)return 0;
        if(!o->begin(o->host,slot->query)) {
            o->destroy(o->host,slot->query);slot->query=NULL;return 0;
        }
        slot->scale=scale;s->active=i+1;return 1;
    }
    return 0;
}
static int tsfp_visibility_end(TsfpVisibilityQueries *s,const TsfpVisibilityOps *o,uint32_t index,int accepted) {
    if(accepted && index<TSFP_VISIBILITY_IDS) {
        TsfpVisibilityEntry *entry=&s->entries[index];
        if(entry->pending)s->slots[entry->pending-1].owner=-1;
        entry->pending=entry->ready=0;
    }
    if(!s->active)return 0;
    uint32_t slot_id=s->active;s->active=0;
    TsfpVisibilitySlot *slot=&s->slots[slot_id-1];
    int ended=o->end(o->host,slot->query);
    if(!accepted || !ended || index>=TSFP_VISIBILITY_IDS) {
        slot->owner=-1;return 0;
    }
    s->entries[index].pending=slot_id;s->entries[index].ready=0;
    slot->owner=(int32_t)index+1;return 1;
}
/* Only1 writes pixels. Pending0/error-1 preserve the caller's output. */
static int tsfp_visibility_result(TsfpVisibilityQueries *s,const TsfpVisibilityOps *o,uint32_t index,uint32_t *pixels) {
    if(index>=TSFP_VISIBILITY_IDS || !pixels)return -1;
    TsfpVisibilityEntry *entry=&s->entries[index];
    if(entry->ready){*pixels=entry->pixels;return 1;}
    if(!entry->pending)return -1;
    TsfpVisibilitySlot *slot=&s->slots[entry->pending-1];uint64_t samples=0;
    int result=o->result(o->host,slot->query,&samples);
    if(!result)return 0;
    entry->pending=0;slot->owner=0;
    if(result<0){o->destroy(o->host,slot->query);slot->query=NULL;return -1;}
    double normalized=(double)samples/slot->scale;
    entry->pixels=normalized>=UINT32_MAX?UINT32_MAX:(uint32_t)(normalized+.5);
    entry->ready=1;*pixels=entry->pixels;return 1;
}
#endif
