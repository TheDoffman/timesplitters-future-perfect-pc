/* Actual auto-look override with controlled VM maps; existing auto_look.c
 * retains host-VM, controller, menu, and preference coverage. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_PC_GUEST_MEMORY_H
#define TSFP_PC_GUEST_SPAN_H
#define TSFP_PC_CONTROLS_H
#define TSFP_PC_PROMPTS_H
typedef struct PcGuestRegion {uintptr_t base;size_t size;int writable;} PcGuestRegion;
static unsigned char memory[0x800000];
static unsigned queries,span_queries;
static int readable=1,writable=1,split;
static PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    PcGuestRegion region={0};++queries;
    uintptr_t base=(uintptr_t)memory;
    if(readable && address>=base && address-base<sizeof memory) {
        region.base=base;region.size=sizeof memory;region.writable=writable;
        if(split) {
            if(address-base<0x3000)region.size=0x3000;
            else {region.base+=0x3000;region.size-=0x3000;}
        }
    }
    return region;
}
static PcGuestRegion pc_guest_query_span(uintptr_t address,size_t bytes)
{
    if(bytes!=4 && bytes!=0x1504 && bytes!=3) {
        fprintf(stderr,"FAIL: unexpected auto-look access extent\n");exit(1);
    }
    ++span_queries;return pc_guest_query_region(address);
}
/* PRODUCTION_MEMORY_HELPERS */
static void pc_controls_get_mouse(double *s,int *on,int *invert)
{*s=.1;*on=1;*invert=0;}
static int pc_prompts_keyboard_active(void){return 1;}
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_eax,g_esi,g_esp;
/* PRODUCTION_AUTO_LOOK */
#define WORD(a) (*(uint32_t *)(memory+(a)))
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s (queries=%u)\n",#x,queries);exit(1);}}while(0)
static unsigned expected_flag,calls;
static void original(void)
{
    CHECK((memory[0x3002]&8u)==expected_flag);
    memory[0x3002]^=0x80; /* Preserve other original writes when restoring bit 3. */
    ++calls;g_esp+=4;
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)memory;
    WORD(0x7B0C7C)=0x1000;WORD(0x1004)=0;WORD(0x2500)=0x3000;
    memory[0x3002]=0x1f;g_esp=0x9000;
    expected_flag=0;pc_auto_look_camera(original);
    CHECK(queries==1 && span_queries==1 && memory[0x3002]==0x9f && calls==1 && g_esp==0x9004);
    queries=0;readable=0;expected_flag=8;
    pc_auto_look_camera(original);CHECK(queries==1 && memory[0x3002]==0x1f);
    queries=0;readable=1;writable=0;
    pc_auto_look_camera(original);CHECK(queries==2 && memory[0x3002]==0x9f);
    queries=0;writable=1;split=1;expected_flag=0;
    pc_auto_look_camera(original);CHECK(queries==2 && memory[0x3002]==0x1f);
    queries=0;split=0;WORD(0x1004)=1;expected_flag=8;
    pc_auto_look_camera(original);CHECK(queries==1 && memory[0x3002]==0x9f);
    puts("PASS: auto-look query reuse, expiry, split/read-only maps and preference restoration");
    return 0;
}
