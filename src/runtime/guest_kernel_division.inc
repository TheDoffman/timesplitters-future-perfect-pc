/* Original arithmetic imports; dispatcher owns16/20-byte stdcall cleanup. */
#include "guest_integer_division.h"
/* Existing project-owned fatal boundary; guest SEH remains separate work. */
extern void tsfp_guest_divide_error(uint32_t site,unsigned reason);
static int bridge_division_arguments(tsfp_guest_vm *vm,uint32_t *words,unsigned count) {
    uint32_t stack=g_esp,bytes=count*4;const unsigned char *frame;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    frame=vm->resolve(vm->host,stack,bytes,0);if(!frame)return 0;
    for(unsigned i=0;i<count;++i)words[i]=(uint32_t)frame[i*4]|((uint32_t)frame[i*4+1]<<8)|((uint32_t)frame[i*4+2]<<16)|((uint32_t)frame[i*4+3]<<24);
    return 1;
}
static void bridge_RtlExtendedLargeIntegerDivide(void) {
    uint32_t words[4];tsfp_guest_vm vm;tsfp_integer_quotient result;unsigned char *out=NULL;
    if(!bridge_division_arguments(&vm,words,4))return;
    if(!words[2]){tsfp_guest_divide_error(g_xbox_kernel_caller,0);return;}
    if(words[3]) {
        if(words[3]>UINT32_MAX-3)return;
        out=vm.resolve(vm.host,words[3],4,1);if(!out)return;
    }
    (void)tsfp_guest_integer_divide(((uint64_t)words[1]<<32)|words[0],words[2],&result);
    if(out)for(unsigned i=0;i<4;++i)out[i]=(unsigned char)(result.remainder>>(i*8));
    g_eax=(uint32_t)result.quotient;g_edx=(uint32_t)(result.quotient>>32);
}
static void bridge_RtlExtendedMagicDivide(void) {
    uint32_t words[5];tsfp_guest_vm vm;uint64_t quotient;
    if(!bridge_division_arguments(&vm,words,5))return;
    quotient=tsfp_guest_magic_divide(((uint64_t)words[1]<<32)|words[0],((uint64_t)words[3]<<32)|words[2],(uint8_t)words[4]);
    g_eax=(uint32_t)quotient;g_edx=(uint32_t)(quotient>>32);
}
