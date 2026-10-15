/* Each explicit wrapper lives in recomp_manual.c so the lifter can find it. */
#include "pc_network.h"
static void pc_network_guest(uint32_t address, void (*original)(void))
{
    uint32_t args[12]; unsigned n,i;
    if(!pc_network_enabled()) { original(); return; }
    n=pc_network_arguments(address);
    for(i=0;i<n;i++) args[i]=*(uint32_t *)((uintptr_t)(g_esp+4+i*4)+g_xbox_mem_offset);
    g_eax=pc_network_call(address,args,g_xbox_mem_offset);
    g_esp+=4+n*4;
}
