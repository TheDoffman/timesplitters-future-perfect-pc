/* Original ordinal281 adapter. Dispatcher owns the 12-byte cleanup. */
#include "guest_integer_product.h"
static void bridge_RtlExtendedIntegerMultiply(void) {
    uint32_t stack=g_esp,words[3];tsfp_guest_vm vm;
    const unsigned char *frame;uint64_t result;
    xbox_GetGuestVirtualMemory(&vm);
    /* This return type has no NTSTATUS channel. Invalid frames leave registers
     * unchanged; do not publish an error code as a successful product. */
    if(!stack || !vm.resolve || stack>UINT32_MAX-11)return;
    frame=vm.resolve(vm.host,stack,12,0);if(!frame)return;
    for(unsigned i=0;i<3;++i)words[i]=(uint32_t)frame[i*4]|((uint32_t)frame[i*4+1]<<8)|((uint32_t)frame[i*4+2]<<16)|((uint32_t)frame[i*4+3]<<24);
    result=tsfp_guest_integer_product(((uint64_t)words[1]<<32)|words[0],words[2]);
    g_eax=(uint32_t)result;g_edx=(uint32_t)(result>>32);
}
