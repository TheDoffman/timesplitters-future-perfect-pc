/* Original guest counted-string adapters; mapped inputs are borrowed only
 * during this synchronous call, then copied into the project namespace. */
#include "guest_kernel_symlinks.h"
static uint32_t bridge_symlink_status(int result) {
    switch(result) {
    case TSFP_SYMLINK_OK:return 0;
    case TSFP_SYMLINK_RESOURCE:return UINT32_C(0xc000009a);
    case TSFP_SYMLINK_COLLISION:return UINT32_C(0xc0000035);
    case TSFP_SYMLINK_MISSING:return UINT32_C(0xc0000034);
    case TSFP_SYMLINK_CLOSED:return UINT32_C(0xc0000056);
    default:return UINT32_C(0xc000000d);
    }
}
static uint32_t bridge_symlink_word(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int bridge_symlink_string(uint32_t address,const void **bytes,size_t *length) {
    tsfp_guest_vm vm;const unsigned char *header;uint32_t n,max,buffer;
    if(!address || address>UINT32_MAX-7)return 0;
    xbox_GetGuestVirtualMemory(&vm);header=vm.resolve(vm.host,address,8,0);
    if(!header)return 0;
    n=(uint32_t)header[0]|((uint32_t)header[1]<<8);max=(uint32_t)header[2]|((uint32_t)header[3]<<8);
    buffer=bridge_symlink_word(header+4);
    if(!n || n>max || !buffer || buffer>UINT32_MAX-(n-1))return 0;
    *bytes=vm.resolve(vm.host,buffer,n,0);if(!*bytes)return 0;
    *length=n;return 1;
}
static void bridge_IoCreateSymbolicLink(void) {
    uint32_t name=STACK_ARG(0),target=STACK_ARG(1);const void *a,*b;size_t n,m;
    if(!bridge_symlink_string(name,&a,&n) || !bridge_symlink_string(target,&b,&m)) {
        g_eax=UINT32_C(0xc000000d);return;
    }
    g_eax=bridge_symlink_status(tsfp_kernel_symlink_create(a,n,b,m));
}
static void bridge_IoDeleteSymbolicLink(void) {
    uint32_t name=STACK_ARG(0);const void *a;size_t n;
    if(!bridge_symlink_string(name,&a,&n)) {g_eax=UINT32_C(0xc000000d);return;}
    g_eax=bridge_symlink_status(tsfp_kernel_symlink_remove(a,n));
}
