/* Original raw charset imports; dispatcher owns20/12-byte guest cleanup. */
#include "guest_text_raw.h"
#include "host_text_codec.h"
#include "host_string_case.h"
static int bridge_raw_arguments(tsfp_guest_vm *vm,uint32_t *a,unsigned count) {
    uint32_t stack=g_esp,bytes=count*4;const unsigned char *frame;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    frame=vm->resolve(vm->host,stack,bytes,0);if(!frame)return 0;
    for(unsigned i=0;i<count;++i)a[i]=(uint32_t)frame[i*4]|((uint32_t)frame[i*4+1]<<8)|((uint32_t)frame[i*4+2]<<16)|((uint32_t)frame[i*4+3]<<24);
    return 1;
}
static void bridge_raw_text(unsigned operation,int query) {
    tsfp_guest_vm vm;tsfp_host_text_codec host;tsfp_text_codec codec;uint32_t a[5];
    if(!bridge_raw_arguments(&vm,a,query?3:5)){g_eax=UINT32_C(0xc000000d);return;}
    tsfp_host_text_codec_init(&host,&codec);
    g_eax=query?tsfp_text_raw_convert(&vm,&codec,tsfp_host_string_case,0,0,a[0],a[1],a[2],operation):
        tsfp_text_raw_convert(&vm,&codec,tsfp_host_string_case,a[0],a[1],a[2],a[3],a[4],operation);
}
static void bridge_RtlMultiByteToUnicodeN(void){bridge_raw_text(TSFP_RAW_TO_UNICODE,0);}
static void bridge_RtlMultiByteToUnicodeSize(void){bridge_raw_text(TSFP_RAW_UNICODE_SIZE,1);}
static void bridge_RtlUnicodeToMultiByteN(void){bridge_raw_text(TSFP_RAW_TO_ANSI,0);}
static void bridge_RtlUnicodeToMultiByteSize(void){bridge_raw_text(TSFP_RAW_ANSI_SIZE,1);}
static void bridge_RtlUpcaseUnicodeToMultiByteN(void){bridge_raw_text(TSFP_RAW_UPPER_ANSI,0);}
