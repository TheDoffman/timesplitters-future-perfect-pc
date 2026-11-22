#ifndef TSFP_PC_NETWORK_SYNC_H
#define TSFP_PC_NETWORK_SYNC_H
#include <stdint.h>
#include <string.h>
/* Local measurements only. No remote clock comparison or inferred packet loss. */
#define PC_NETWORK_SYNC_FIELDS 16
typedef struct PcNetworkSync {
    uint64_t packets,accepted,duplicates,reordered,coalesced,intervals,jitter_sum;
    uint64_t arrival[16],interval[16],pending[16];
    unsigned char seen[16],have_interval[16];
    uint64_t gap_max,queue_max;
    uint32_t session;
} PcNetworkSync;
static inline void pc_network_sync_context(PcNetworkSync *s){
    uint32_t serial=s->session+1u;memset(s,0,sizeof *s);s->session=serial;
}
static inline void pc_network_sync_arrival(PcNetworkSync *s,unsigned slot,uint64_t now,int32_t delta,int pending){
    if(s->seen[slot] && delta<=0){if(!delta)s->duplicates++;else s->reordered++;return;}
    if(s->seen[slot] && now>=s->arrival[slot]){
        uint64_t gap=now-s->arrival[slot];if(gap>s->gap_max)s->gap_max=gap;
        if(s->have_interval[slot]){s->jitter_sum+=gap>s->interval[slot]?gap-s->interval[slot]:s->interval[slot]-gap;s->intervals++;}
        s->interval[slot]=gap;s->have_interval[slot]=1;
    }
    s->seen[slot]=1;s->arrival[slot]=s->pending[slot]=now;s->accepted++;
    if(pending)s->coalesced++;
}
static inline void pc_network_sync_consume(PcNetworkSync *s,unsigned slot,uint64_t now){
    if(now>=s->pending[slot] && now-s->pending[slot]>s->queue_max)s->queue_max=now-s->pending[slot];
}
static inline int32_t pc_network_sync_number(uint64_t n){return n>INT32_MAX?INT32_MAX:(int32_t)n;}
#endif
