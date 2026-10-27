/* Solver history is keyed by guest ownership. No guest memory or render pose is cached. */
#ifndef TSFP_PC_NATIVE_WIND_OWNERS_H
#define TSFP_PC_NATIVE_WIND_OWNERS_H
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "pc_native_wind.h"
#ifndef PC_NATIVE_WIND_REALLOC
#define PC_NATIVE_WIND_REALLOC realloc
#endif
typedef struct PcWindOwner {
    uint32_t owner,data,generation,tick;
    int64_t phase,frequency;
    int clock_valid;
    PcWindHistory axes[2];
} PcWindOwner;
typedef struct PcWindOwners {
    PcWindOwner *entries;
    size_t count,capacity;
    ptrdiff_t memory;
    uint32_t scene;
    int initialized;
} PcWindOwners;
static void pc_wind_owners_context(PcWindOwners *s,ptrdiff_t memory,uint32_t scene)
{
    if(!s->initialized || s->memory!=memory || s->scene!=scene) {
        s->count=0;s->memory=memory;s->scene=scene;s->initialized=1;
    }
}
static size_t pc_wind_owner_index(const PcWindOwners *s,uint32_t owner)
{
    size_t lo=0,hi=s->count;
    while(lo<hi){size_t m=lo+(hi-lo)/2;if(s->entries[m].owner<owner)lo=m+1;else hi=m;}
    return lo;
}
static PcWindOwner *pc_wind_owner_register(PcWindOwners *s,ptrdiff_t memory,
    uint32_t scene,uint32_t owner,uint32_t data,uint32_t generation,int wind)
{
    pc_wind_owners_context(s,memory,scene);
    size_t i=pc_wind_owner_index(s,owner);
    int found=i<s->count && s->entries[i].owner==owner;
    if(!wind) {
        if(found){memmove(s->entries+i,s->entries+i+1,(s->count-i-1)*sizeof(*s->entries));--s->count;}
        return NULL;
    }
    if(!found) {
        if(s->count==s->capacity) {
            size_t capacity=s->capacity?s->capacity*2:16;
            if(capacity<s->capacity || capacity>SIZE_MAX/sizeof(*s->entries))return NULL;
            PcWindOwner *next=(PcWindOwner *)PC_NATIVE_WIND_REALLOC(s->entries,capacity*sizeof(*next));
            if(!next)return NULL;
            s->entries=next;s->capacity=capacity;
        }
        memmove(s->entries+i+1,s->entries+i,(s->count-i)*sizeof(*s->entries));++s->count;
    }
    s->entries[i]=(PcWindOwner){0};s->entries[i].owner=owner;
    s->entries[i].data=data;s->entries[i].generation=generation;
    return s->entries+i;
}
static PcWindOwner *pc_wind_owner_begin(PcWindOwners *s,ptrdiff_t memory,
    uint32_t scene,uint32_t owner,uint32_t data,uint32_t generation,
    uint32_t start_tick,int64_t phase,int64_t frequency)
{
    pc_wind_owners_context(s,memory,scene);
    size_t i=pc_wind_owner_index(s,owner);
    if(i==s->count || s->entries[i].owner!=owner)return NULL;
    PcWindOwner *p=s->entries+i;
    if(p->data!=data || p->generation!=generation ||
       (p->clock_valid && (p->tick!=start_tick || p->phase!=phase || p->frequency!=frequency))) {
        p->axes[0]=(PcWindHistory){0};p->axes[1]=(PcWindHistory){0};p->clock_valid=0;
        p->data=data;p->generation=generation;
    }
    return p;
}
static void pc_wind_owner_invalidate(PcWindOwner *p)
{
    if(p){p->axes[0]=(PcWindHistory){0};p->axes[1]=(PcWindHistory){0};p->clock_valid=0;}
}
static int pc_wind_owner_end(PcWindOwner *p,uint32_t tick,int64_t phase,
    int64_t units,int64_t frequency)
{
    if(!p || frequency<=0 || phase<0 || phase>=frequency || units<0 || units>INT64_MAX-phase) {
        pc_wind_owner_invalidate(p);return 0;
    }
    int64_t end=phase+units;
    p->tick=tick+(uint32_t)(end/frequency);p->phase=end%frequency;
    p->frequency=frequency;p->clock_valid=1;return 1;
}
#endif
