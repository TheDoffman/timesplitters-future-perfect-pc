/* Shared transaction policy: classify completed slide results, retain original
 * discrete/paused modes, and never let recursion leak a previous clear result. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
static unsigned char memory[0x800000];
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
#include "../src/pc_native_walk_guest.h"
static uint32_t result,returned;
static unsigned calls;
static int expected;
static void sweep(void) {calls++;result=returned;}
static void nested(void) {assert(!pc_native_walk_accept_clear_slide());assert(!pc_walk_scope);}
static void root(void)
{
    assert((pc_walk_scope!=NULL)==expected && !pc_native_walk_accept_clear_slide());
    returned=6;pc_native_walk_sweep_guest(sweep,0xc55ebu,&result);
    assert(result==6 && !pc_native_walk_accept_clear_slide());
    returned=0;pc_native_walk_sweep_guest(sweep,0xc5781u,&result);
    assert(result==0 && pc_native_walk_accept_clear_slide()==expected);
    float *delta=(float *)(memory+0x4e794c),saved=*delta;
    *delta=1;pc_native_walk_guest(nested);*delta=saved;
    assert(pc_native_walk_accept_clear_slide()==expected);
    returned=2;pc_native_walk_sweep_guest(sweep,0xc58b0u,&result);
    assert(!pc_native_walk_accept_clear_slide());
    returned=0;pc_native_walk_sweep_guest(sweep,0xc4b6du,&result);
    assert(!pc_native_walk_accept_clear_slide());
    pc_native_walk_sweep_guest(sweep,0xc58b0u,&result);
    assert(pc_native_walk_accept_clear_slide()==expected);
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    const float values[]={1,.5f,60.f/144.f,60.f/180.f,.25f,0,-1,2,NAN,INFINITY};
    for(int native=0;native<2;native++)for(unsigned i=0;i<sizeof values/sizeof values[0];i++) {
        g_tsfp_native_timing_active=native;*(float *)(memory+0x4e794c)=values[i];
        expected=native && values[i]>0 && values[i]<1;
        assert(pc_native_walk_fractional()==expected);
        unsigned before=calls;pc_native_walk_guest(root);assert(calls-before==5);
        assert(!pc_walk_scope && !pc_native_walk_accept_clear_slide());
    }
    returned=0;pc_native_walk_sweep_guest(sweep,0xc5781u,&result);
    assert(!pc_native_walk_accept_clear_slide());
    puts("PASS: fractional clear-slide scope, blocked replacement, recursion, interval guards and no retained state");
    return 0;
}
