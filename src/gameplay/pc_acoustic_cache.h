/* Guest-thread acoustic scheduling; no device callback or guest reads here. */
#ifndef PC_ACOUSTIC_CACHE_H
#define PC_ACOUSTIC_CACHE_H
#include "pc_acoustic_scene.h"
#define PC_ACOUSTIC_CACHE_SLOTS 256
#define PC_ACOUSTIC_REFRESH_US 100000u
#define PC_ACOUSTIC_MAX_AGE_US 250000u
typedef struct PcAcousticCacheEntry {
    uint32_t key,table,portals;
    unsigned rooms,count,listener_room,source_room,valid;
    uint64_t time,seen,pending;
    PcAcousticResult result;
    float anchor[3];
} PcAcousticCacheEntry;
typedef struct PcAcousticCache {
    PcAcousticCacheEntry entries[PC_ACOUSTIC_CACHE_SLOTS];
    uint64_t budget_time,serial;
    unsigned budget;
} PcAcousticCache;
static inline PcAcousticCacheEntry *pc_acoustic_cache_entry(PcAcousticCache *cache,uint32_t key)
{
    unsigned first=(key>>4)&(PC_ACOUSTIC_CACHE_SLOTS-1);
    PcAcousticCacheEntry *oldest=&cache->entries[first];
    /* Guest allocation patterns can give many voices the same low address
       bits. Probe before eviction so those voices keep their refresh tickets. */
    for(unsigned i=0;i<PC_ACOUSTIC_CACHE_SLOTS;i++) {
        PcAcousticCacheEntry *e=&cache->entries[(first+i)&(PC_ACOUSTIC_CACHE_SLOTS-1)];
        if(!e->key || e->key==key)return e;
        if(e->seen<oldest->seen)oldest=e;
    }
    return oldest;
}
static inline PcAcousticResult pc_acoustic_cache_direction(const PcAcousticCacheEntry *e,
    const float *listener,const float *source)
{
    PcAcousticResult result=e->result;
    /* Only diffraction has a fixed world anchor (the first opening). A fully
       blocked source still follows its current emitter, not its previous ray. */
    for(unsigned i=0;i<3;i++)result.direction[i]=(result.room>0?e->anchor[i]:source[i])-listener[i];
    return result;
}
/* Return 1 to spend a query now; otherwise return a bounded cached result or
   neutral fallback. Current room identities are checked even during the TTL. */
static inline int pc_acoustic_cache_request(PcAcousticCache *cache,uint32_t key,
    uint32_t table,uint32_t portals,unsigned rooms,unsigned count,unsigned lr,unsigned sr,
    uint64_t now,const float *listener,const float *source,PcAcousticResult *out)
{
    memset(out,0,sizeof *out);
    for(unsigned i=0;i<3;i++)out->direction[i]=source[i]-listener[i];
    PcAcousticCacheEntry *e=pc_acoustic_cache_entry(cache,key);
    if(e->key!=key || e->table!=table || e->portals!=portals || e->rooms!=rooms || e->count!=count) {
        memset(e,0,sizeof *e);e->key=key;e->table=table;e->portals=portals;e->rooms=rooms;e->count=count;
    }
    if(e->listener_room!=lr || e->source_room!=sr) {
        e->valid=0;e->listener_room=lr;e->source_room=sr;
    }
    e->seen=now;
    if(e->valid && now-e->time<PC_ACOUSTIC_REFRESH_US) {
        *out=pc_acoustic_cache_direction(e,listener,source);return 0;
    }
    if(!e->pending)e->pending=++cache->serial;
    if(now-cache->budget_time>=10000u){cache->budget_time=now;cache->budget=0;}
    /* Oldest outstanding, recently requested voice wins. Voices which stop
       requesting cannot hold the queue indefinitely. This prevents stable
       commit order from assigning every refresh to the first few voices. */
    int oldest=1;
    for(unsigned i=0;i<PC_ACOUSTIC_CACHE_SLOTS;i++) {
        PcAcousticCacheEntry *other=&cache->entries[i];
        if(other->pending && other->pending<e->pending && now-other->seen<PC_ACOUSTIC_REFRESH_US){oldest=0;break;}
    }
    if(cache->budget<2 && oldest){cache->budget++;return 1;}
    if(e->valid && now-e->time<=PC_ACOUSTIC_MAX_AGE_US)
        *out=pc_acoustic_cache_direction(e,listener,source);
    return 0;
}
static inline void pc_acoustic_cache_store(PcAcousticCache *cache,uint32_t key,uint64_t now,
    const float *listener,PcAcousticResult result)
{
    PcAcousticCacheEntry *e=pc_acoustic_cache_entry(cache,key);
    e->result=result;e->time=now;e->pending=0;e->valid=1;
    for(unsigned i=0;i<3;i++)e->anchor[i]=listener[i]+result.direction[i];
}
#endif
