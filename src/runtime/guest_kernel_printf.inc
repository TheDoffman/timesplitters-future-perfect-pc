/* Original cdecl guest formatting imports; the caller retains stack cleanup. */
#include "guest_printf.h"
static int bridge_printf_frame(tsfp_guest_vm *vm,uint32_t *words,unsigned count,uint64_t *tail) {
    uint32_t stack=g_esp,bytes=count*4;const unsigned char *p;unsigned i;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    p=vm->resolve(vm->host,stack,bytes,0);if(!p)return 0;
    for(i=0;i<count;++i)words[i]=(uint32_t)p[i*4]|((uint32_t)p[i*4+1]<<8)|((uint32_t)p[i*4+2]<<16)|((uint32_t)p[i*4+3]<<24);
    *tail=(uint64_t)stack+bytes;return 1;
}
static void bridge_RtlSnprintf(void) {
    uint32_t words[3];uint64_t tail;tsfp_guest_vm vm;
    if(!bridge_printf_frame(&vm,words,3,&tail) || tail>UINT32_MAX){g_eax=0;return;}
    g_eax=tsfp_guest_printf(&vm,words[0],words[1],words[2],(uint32_t)tail,NULL);
}
static void bridge_RtlSprintf(void) {
    uint32_t words[2];uint64_t tail;tsfp_guest_vm vm;
    if(!bridge_printf_frame(&vm,words,2,&tail) || tail>UINT32_MAX){g_eax=0;return;}
    g_eax=tsfp_guest_printf(&vm,words[0],65536,words[1],(uint32_t)tail,NULL);
}
static void bridge_RtlVsnprintf(void) {
    uint32_t words[4];uint64_t tail;tsfp_guest_vm vm;
    if(!bridge_printf_frame(&vm,words,4,&tail) || !words[3]){g_eax=0;return;}
    g_eax=tsfp_guest_printf(&vm,words[0],words[1],words[2],words[3],NULL);
}
static void bridge_RtlVsprintf(void) {
    uint32_t words[3];uint64_t tail;tsfp_guest_vm vm;
    if(!bridge_printf_frame(&vm,words,3,&tail) || !words[2]){g_eax=0;return;}
    g_eax=tsfp_guest_printf(&vm,words[0],65536,words[1],words[2],NULL);
}
