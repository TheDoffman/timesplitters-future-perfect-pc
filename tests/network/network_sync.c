#include "network/pc_network_sync.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    PcNetworkSync s={0};pc_network_sync_context(&s);assert(s.session==1);
    pc_network_sync_arrival(&s,2,100,0,0);
    pc_network_sync_arrival(&s,2,150,3,1);
    pc_network_sync_arrival(&s,2,230,3,1);
    assert(s.accepted==3 && s.coalesced==2 && s.gap_max==80 && s.intervals==1 && s.jitter_sum==30);
    pc_network_sync_arrival(&s,2,240,0,1);pc_network_sync_arrival(&s,2,250,-3,1);
    assert(s.duplicates==1 && s.reordered==1 && s.arrival[2]==230 && s.accepted==3);
    pc_network_sync_consume(&s,2,275);assert(s.queue_max==45);
    pc_network_sync_consume(&s,2,100);assert(s.queue_max==45);
    pc_network_sync_arrival(&s,3,1000,0,0);assert(s.gap_max==80);
    /* A timestamp of zero and tick rollover are ordinary first observations. */
    pc_network_sync_arrival(&s,4,0,0,0);pc_network_sync_arrival(&s,4,50,3,0);
    assert(s.arrival[4]==50 && s.accepted==6);
    assert(pc_network_sync_number(UINT64_MAX)==INT32_MAX && pc_network_sync_number(123)==123);
    pc_network_sync_context(&s);assert(s.session==2 && !s.accepted && !s.seen[2] && !s.queue_max);
    puts("PASS: per-actor jitter, coalescing, stale packets, queue age and context reset");return 0;
}
