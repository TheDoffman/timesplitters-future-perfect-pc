/* Original checked string creation/case imports; fixed guest ABI frames. */
#include "guest_string_transform.h"
#include "host_string_case.h"
static int bridge_transform_arguments(tsfp_guest_vm *vm,uint32_t *a,unsigned count) {
    uint32_t stack=g_esp,bytes=count*4;const unsigned char *frame;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    frame=vm->resolve(vm->host,stack,bytes,0);if(!frame)return 0;
    for(unsigned i=0;i<count;++i)a[i]=(uint32_t)frame[i*4]|((uint32_t)frame[i*4+1]<<8)|((uint32_t)frame[i*4+2]<<16)|((uint32_t)frame[i*4+3]<<24);
    return 1;
}
static void bridge_RtlCreateUnicodeString(void) {
    tsfp_guest_vm vm;uint32_t a[2];
    g_eax=bridge_transform_arguments(&vm,a,2)?(uint32_t)tsfp_string_create(&vm,a[0],a[1]):0;
}
static void bridge_transform_string(int upper) {
    tsfp_guest_vm vm;uint32_t a[3];
    g_eax=bridge_transform_arguments(&vm,a,3)?tsfp_string_transform(&vm,tsfp_host_string_case,a[0],a[1],(uint8_t)a[2],upper):UINT32_C(0xc000000d);
}
static void bridge_transform_unit(unsigned width,int upper) {
    tsfp_guest_vm vm;uint32_t a[1];
    g_eax=bridge_transform_arguments(&vm,a,1)?tsfp_host_string_case(a[0],width,upper):0;
}
static void bridge_RtlUpcaseUnicodeString(void){bridge_transform_string(1);}
static void bridge_RtlDowncaseUnicodeString(void){bridge_transform_string(0);}
static void bridge_RtlUpperChar(void){bridge_transform_unit(1,1);}
static void bridge_RtlLowerChar(void){bridge_transform_unit(1,0);}
static void bridge_RtlUpcaseUnicodeChar(void){bridge_transform_unit(2,1);}
static void bridge_RtlDowncaseUnicodeChar(void){bridge_transform_unit(2,0);}
