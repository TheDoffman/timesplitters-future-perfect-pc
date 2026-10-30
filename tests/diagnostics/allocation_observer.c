#include "guest_allocation.h"
#include <assert.h>
static unsigned n;static uint32_t events[8][3];
static void observe(uint32_t kind,uint32_t address,uint32_t bytes){assert(n<8);events[n][0]=kind;events[n][1]=address;events[n++][2]=bytes;}
int main(void){
    static unsigned char memory[65536];tsfp_allocations state={0};tsfp_allocation_record records[16];
    tsfp_allocation_config cfg={0};cfg.ram=memory;cfg.ram_size=sizeof memory;cfg.heap_begin=4096;cfg.reserve_begin=8192;cfg.observer=observe;
    assert(tsfp_allocations_init(&state,&cfg,records,16,0,0));assert(n==0);
    uint32_t first=tsfp_heap_allocate(&state,100,4);assert(first&&n==1&&events[0][0]==1&&events[0][2]==100);
    assert(!tsfp_heap_release(&state,first+1)&&n==1);
    assert(tsfp_heap_release(&state,first)&&n==2&&events[1][0]==257&&events[1][1]==first&&events[1][2]==100);
    assert(!tsfp_heap_release(&state,first)&&n==2);
    assert(tsfp_heap_allocate(&state,100,4)==first&&n==3);
    assert(!tsfp_heap_allocate(&state,100000,4)&&n==3);
    tsfp_allocations_stop(&state);return 0;
}
