/* Host precision hints and title-owned target selection. No GPU proof. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/graphics/pc_surface_precision.h"
static unsigned char memory[0x900000];
static intptr_t g_xbox_mem_offset;
static uint32_t g_esp;
#define PC_SURFACE_WORD(va) (*(uint32_t *)(memory+(va)))
static PcSurfacePrecision targets;
static unsigned calls,resets,registrations;
static int supported=1;
void hle_d3d8_reset_render_precision(void){resets++;pc_surface_precision_reset(&targets);}
int hle_d3d8_set_render_precision(unsigned slot,uint32_t va)
{ registrations++;return supported && pc_surface_precision_set(&targets,slot,va); }
#include "../src/graphics/pc_surface_precision_guest.h"
static void constructor(void)
{
    unsigned slot=PC_SURFACE_WORD(g_esp+4);calls++;
    if(slot<5)for(unsigned i=0;i<4;i++)PC_SURFACE_WORD(0x74fa20+slot*16+i*4)=100+slot*4+i;
    g_esp+=4;
}
static void construct(unsigned slot)
{
    g_esp=0x1000;PC_SURFACE_WORD(g_esp)=0x12345678;PC_SURFACE_WORD(g_esp+4)=slot;
    pc_surface_precision_guest(constructor);
    assert(g_esp==0x1004 && PC_SURFACE_WORD(0x1000)==0x12345678);
}
int main(int argc,char **argv)
{
    (void)argv;(void)g_xbox_mem_offset;int requested=argc==1;
#ifdef _WIN32
    _putenv_s("TSFP_SURFACE_FLOAT_TEST",requested?"1":"0");
#else
    setenv("TSFP_SURFACE_FLOAT_TEST",requested?"1":"0",1);
#endif
    PC_SURFACE_WORD(0x4b85c8)=0x20000;PC_SURFACE_WORD(0x20000)=0x30000;
    memset(memory+0x30000,0xab,0x10000);
    for(unsigned slot=0;slot<5;slot++)construct(slot);
    assert(calls==5 && registrations==(requested?15:0) && resets==(requested?1:0));
    for(unsigned slot=0;slot<5;slot++)for(unsigned i=0;i<4;i++) {
        unsigned va=0x30000+(100+slot*4+i)*0x34;
        assert(PC_SURFACE_WORD(va)==0xabababab);
        assert(pc_surface_precision_format(&targets,va,6)==(requested && i<3?0x4bu:6u));
        assert(pc_surface_precision_format(&targets,va,0x12)==(requested && i<3?0x4bu:0x12u));
        assert(pc_surface_precision_format(&targets,va,0x0b)==0x0b);
    }
    assert(pc_surface_precision_format(&targets,0,6)==6);
    assert(!pc_surface_precision_set(&targets,15,0x10000));
    assert(!pc_surface_precision_set(&targets,0,0xfffffff0));
    construct(5);assert(calls==6);
    construct(0); /* New pool clears stale slots 1..4. */
    for(unsigned i=3;i<15;i++)assert(!targets.targets[i]);
    pc_surface_precision_reset(&targets);supported=0;construct(0);
    for(unsigned i=0;i<15;i++)assert(!targets.targets[i]);
    supported=1;PC_SURFACE_WORD(0x20000)=0xfffffff0;construct(0);
    for(unsigned i=0;i<15;i++)assert(!targets.targets[i]);
    PC_SURFACE_WORD(0x4b85c8)=0;construct(0);
    puts("PASS: explicit precision, three intermediates, final unchanged, reset, bounds and unsupported backend");
    return 0;
}
