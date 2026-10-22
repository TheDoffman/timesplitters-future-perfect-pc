#ifndef PC_SURFACE_PRECISION_GUEST_H
#define PC_SURFACE_PRECISION_GUEST_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
extern void hle_d3d8_reset_render_precision(void);
extern int hle_d3d8_set_render_precision(unsigned slot,uint32_t va);
static int pc_surface_float_requested(void)
{
    static int checked,enabled;
    if(!checked) {const char *s=getenv("TSFP_SURFACE_FLOAT_TEST");enabled=s && s[0]=='1';checked=1;}
    return enabled;
}
/* The original constructor allocates four guest images per resource. Only
 * the first three are feedback intermediates; image 3 remains the final 8-bit
 * texture. Hints precede the first render-target bind/clear, with no CPU copy. */
static void pc_surface_precision_guest(void (*original)(void))
{
    if(!pc_surface_float_requested()){original();return;}
    uint32_t slot=PC_SURFACE_WORD(g_esp+4);
    if(slot==0)hle_d3d8_reset_render_precision();
    original();
    if(slot>=5)return;
    uint32_t table=PC_SURFACE_WORD(0x4b85c8u);
    if(table<0x10000u || table>0x83fffffcu)return;
    uint32_t base=PC_SURFACE_WORD(table);
    for(unsigned i=0;i<3;i++) {
        uint32_t id=PC_SURFACE_WORD(0x74fa20u+slot*16+i*4);
        uint64_t va=(uint64_t)base+(uint64_t)id*0x34u;
        if(base<0x10000u || va>0x83ffffccu ||
           !hle_d3d8_set_render_precision(slot*3+i,(uint32_t)va)) {
            fprintf(stderr,"[SURFACE-FLOAT] Unsupported target or renderer\n");return;
        }
    }
    fprintf(stderr,"[SURFACE-FLOAT] Registered feedback slot %u\n",slot);
}
#endif
