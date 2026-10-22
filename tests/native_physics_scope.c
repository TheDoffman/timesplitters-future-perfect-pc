/* Asset-free checks of RNG isolation, including unrelated draws and nesting. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned observations;
#define PC_NATIVE_PHYSICS_OBSERVE() (++observations)
/* PHYSICS POLICY */
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
static unsigned total_calls,returned,steps,whole,step_index;
#define WORD(address) (*(uint32_t *)(g_xbox_mem_offset+(address)))
static void original_random(void)
{
    WORD(0x4e7950)+=3;WORD(0x4e7954)+=5;WORD(0x7497c8)++;
    returned=WORD(0x4e7950);total_calls++;
}
static void nested_solver(void)
{
    uint32_t seed=WORD(0x4e7950),count=WORD(0x7497c8);
    for(unsigned i=1;i<=2;i++) {
        pc_native_physics_random_guest(original_random);
        assert(returned==seed+3*i);
        assert(WORD(0x4e7950)==seed && WORD(0x7497c8)==count);
    }
}
static void nested_update(void) {pc_native_physics_solver_guest(nested_solver);}
static void solver(void)
{
    int isolated=g_tsfp_native_timing_active && step_index>=whole;
    uint32_t seed=WORD(0x4e7950),high=WORD(0x4e7954),count=WORD(0x7497c8);
    pc_native_physics_random_guest(original_random);
    assert(returned==seed+3);
    assert(WORD(0x7497c8)==count+(isolated?0:1));
    /* Other gameplay calls stay on the shared generator in either mode. */
    original_random();
    if(g_tsfp_native_timing_active) {
        uint32_t saved=WORD(0x4e7944);WORD(0x4e7944)=0;
        pc_native_physics_guest(nested_update);WORD(0x4e7944)=saved;
    }
    pc_native_physics_random_guest(original_random);
    assert(returned==seed+(isolated?6:9));
    pc_native_physics_random_guest(original_random);
    assert(returned==seed+(isolated?9:12));
    unsigned committed=isolated?1:4;
    assert(WORD(0x4e7950)==seed+committed*3 &&
           WORD(0x4e7954)==high+committed*5 && WORD(0x7497c8)==count+committed);
}
static void update(void)
{ for(step_index=0;step_index<steps;step_index++)pc_native_physics_solver_guest(solver); }
int main(void)
{
    void *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    for(unsigned native=0;native<2;native++)for(whole=0;whole<5;whole++)for(steps=0;steps<5;steps++) {
        g_tsfp_native_timing_active=native;
        WORD(0x4e7950)=1;WORD(0x4e7954)=100;WORD(0x7497c8)=0;WORD(0x4e7944)=whole;
        total_calls=0;observations=0;pc_native_physics_guest(update);
        unsigned committed=steps+3*(native && whole<steps?whole:steps);
        assert(WORD(0x7497c8)==committed && pc_physics_scope==NULL);
        assert(total_calls==steps*(native?6:4));
        assert(observations==steps*(native?2:1)); /* Includes Original and nested calls. */
        pc_native_physics_random_guest(original_random);
        assert(WORD(0x7497c8)==committed+1); /* outside scope remains original */
    }
    free(memory);puts("PASS: original/native RNG, extra primary steps, nesting, unrelated draws and scope cleanup");return 0;
}
