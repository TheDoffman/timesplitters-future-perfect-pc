/* Asset-free shared scope contract; original decoder/eligibility integration
 * lives in native_player_trigger.c and requires the supported executable. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
#include "../src/gameplay/pc_native_weapon_guest.h"
#define WORD(a) (*(uint32_t *)(g_xbox_mem_offset+(a)))
#define FLOAT(a) (*(float *)(g_xbox_mem_offset+(a)))
static unsigned observed,calls;
static float elapsed;
static void consumer(void)
{
    observed=WORD(0x4e7944);elapsed=FLOAT(0x4e794c);calls++;
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x500000);
    assert(g_xbox_mem_offset);
    for(int native=0;native<2;native++)
    for(unsigned caller=0;caller<2;caller++)
    for(unsigned held=0;held<2;held++)
    for(unsigned whole=0;whole<4;whole++)
    for(unsigned advancing=0;advancing<2;advancing++) {
        g_tsfp_native_timing_active=native;
        WORD(0x1000)=caller?0x1c2e6d:0x1c38b0;
        WORD(0x1010)=held;
        WORD(0x4e7944)=whole;FLOAT(0x4e794c)=advancing?.25f:0;
        pc_native_player_trigger_guest(consumer,0x1000);
        assert(observed==(native && caller && !held && !whole && advancing?1:whole));
        assert(elapsed==(advancing?.25f:0));
        assert(WORD(0x4e7944)==whole && FLOAT(0x4e794c)==elapsed);
    }
    assert(calls==64);
    free((void *)(uintptr_t)g_xbox_mem_offset);
    puts("PASS: fresh request scope, held/caller exclusions, pause, Original and clock restoration");
}
