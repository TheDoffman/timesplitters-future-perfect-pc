/* Original fixed-frame numeric imports; dispatcher retains stdcall cleanup. */
#include "guest_numbers.h"
static int bridge_number_arguments(tsfp_guest_vm *vm,uint32_t *words,unsigned count) {
    uint32_t stack=g_esp,bytes=count*4;const unsigned char *p;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    p=vm->resolve(vm->host,stack,bytes,0);if(!p)return 0;
    for(unsigned i=0;i<count;++i)words[i]=(uint32_t)p[i*4]|((uint32_t)p[i*4+1]<<8)|((uint32_t)p[i*4+2]<<16)|((uint32_t)p[i*4+3]<<24);
    return 1;
}
static void bridge_RtlCharToInteger(void) {
    uint32_t a[3];tsfp_guest_vm vm;
    g_eax=bridge_number_arguments(&vm,a,3)?tsfp_number_from_ansi(&vm,a[0],a[1],a[2]):UINT32_C(0xc000000d);
}
static void bridge_RtlUnicodeStringToInteger(void) {
    uint32_t a[3];tsfp_guest_vm vm;
    g_eax=bridge_number_arguments(&vm,a,3)?tsfp_number_from_unicode(&vm,a[0],a[1],a[2]):UINT32_C(0xc000000d);
}
static void bridge_RtlIntegerToChar(void) {
    uint32_t a[4];tsfp_guest_vm vm;
    g_eax=bridge_number_arguments(&vm,a,4)?tsfp_number_to_ansi(&vm,a[0],a[1],a[2],a[3]):UINT32_C(0xc000000d);
}
static void bridge_RtlIntegerToUnicodeString(void) {
    uint32_t a[3];tsfp_guest_vm vm;
    g_eax=bridge_number_arguments(&vm,a,3)?tsfp_number_to_unicode(&vm,a[0],a[1],a[2]):UINT32_C(0xc000000d);
}
