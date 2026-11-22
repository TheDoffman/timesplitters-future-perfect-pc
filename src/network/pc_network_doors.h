#ifndef TSFP_PC_NETWORK_DOORS_H
#define TSFP_PC_NETWORK_DOORS_H
#include <stdint.h>
#include <math.h>
#define PC_NETWORK_DOORS_MAX 65u
#define PC_NETWORK_DOORS_BATCH 16u
/* Stable level-table indices and tags, never host pointers or runtime IDs. */
typedef struct PcNetworkDoorState {
    uint32_t slot,key,kind,phase,flags,timer;
    float progress,extra,target,velocity;
} PcNetworkDoorState;
static inline int pc_network_door_valid(const PcNetworkDoorState *s)
{
    return s->slot<PC_NETWORK_DOORS_MAX && s->phase<=3u &&
        !(s->flags&~0x04000000u) && isfinite(s->progress) &&
        s->progress>=0.f && s->progress<=1.f &&
        isfinite(s->target) && s->target>=0.f && s->target<=1.f &&
        isfinite(s->extra) && fabsf(s->extra)<=1000000.f &&
        isfinite(s->velocity) && fabsf(s->velocity)<=1000000.f;
}
void pc_network_doors_context(unsigned role,uint32_t host_ip,uint32_t match);
void pc_network_doors_targets(const uint32_t *addresses,unsigned count);
void pc_network_doors_send(uint32_t tick,const PcNetworkDoorState *states,unsigned count);
unsigned pc_network_doors_receive(PcNetworkDoorState *states,unsigned capacity);
void pc_network_doors_frame(void);
#endif
