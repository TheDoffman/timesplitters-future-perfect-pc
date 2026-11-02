/* Original RTL memory/bit imports; dispatcher retains8/12-byte cleanup; swaps use ECX/zero cleanup. */
#include "guest_memory_ops.h"
static int bridge_memory_arguments(tsfp_guest_vm *vm,uint32_t *words,unsigned count) {
    uint32_t stack=g_esp,bytes=count*4;const unsigned char *frame;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    frame=vm->resolve(vm->host,stack,bytes,0);if(!frame)return 0;
    for(unsigned i=0;i<count;++i)words[i]=(uint32_t)frame[i*4]|((uint32_t)frame[i*4+1]<<8)|((uint32_t)frame[i*4+2]<<16)|((uint32_t)frame[i*4+3]<<24);
    return 1;
}
static void bridge_RtlFillMemory(void) {
    uint32_t a[3];tsfp_guest_vm vm;
    if(bridge_memory_arguments(&vm,a,3))(void)tsfp_memory_fill(&vm,a[0],a[1],a[2],1);
    g_eax=0;
}
static void bridge_RtlZeroMemory(void) {
    uint32_t a[2];tsfp_guest_vm vm;
    if(bridge_memory_arguments(&vm,a,2))(void)tsfp_memory_fill(&vm,a[0],a[1],0,1);
    g_eax=0;
}
static void bridge_RtlFillMemoryUlong(void) {
    uint32_t a[3];tsfp_guest_vm vm;
    if(bridge_memory_arguments(&vm,a,3))(void)tsfp_memory_fill(&vm,a[0],a[1],a[2],4);
    g_eax=0;
}
static void bridge_RtlMoveMemory(void) {
    uint32_t a[3];tsfp_guest_vm vm;
    if(bridge_memory_arguments(&vm,a,3))(void)tsfp_memory_move(&vm,a[0],a[1],a[2]);
    g_eax=0;
}
static void bridge_RtlMapGenericMask(void) {
    uint32_t a[2];tsfp_guest_vm vm;
    if(bridge_memory_arguments(&vm,a,2))(void)tsfp_memory_map_mask(&vm,a[0],a[1]);
    g_eax=0;
}
static void bridge_RtlUlongByteSwap(void) {g_eax=tsfp_memory_byte_swap(g_ecx,4);}
static void bridge_RtlUshortByteSwap(void) {g_eax=tsfp_memory_byte_swap(g_ecx,2);}
