/* Original checked counted-string imports; dispatcher owns12/4-byte cleanup. */
#include "host_text_codec.h"
static int bridge_codec_arguments(tsfp_guest_vm *vm,uint32_t *words,unsigned count) {
    uint32_t stack=g_esp,bytes=count*4;const unsigned char *frame;
    xbox_GetGuestVirtualMemory(vm);
    if(!stack || !vm->resolve || stack>UINT32_MAX-(bytes-1))return 0;
    frame=vm->resolve(vm->host,stack,bytes,0);if(!frame)return 0;
    for(unsigned i=0;i<count;++i)words[i]=(uint32_t)frame[i*4]|((uint32_t)frame[i*4+1]<<8)|((uint32_t)frame[i*4+2]<<16)|((uint32_t)frame[i*4+3]<<24);
    return 1;
}
static void bridge_codec_string(unsigned direction) {
    uint32_t a[3];tsfp_guest_vm vm;tsfp_host_text_codec host;tsfp_text_codec codec;
    if(!bridge_codec_arguments(&vm,a,3)){g_eax=UINT32_C(0xc000000d);return;}
    tsfp_host_text_codec_init(&host,&codec);
    g_eax=tsfp_text_string_convert(&vm,&codec,a[0],a[1],(uint8_t)a[2],direction);
}
static void bridge_RtlAnsiStringToUnicodeString(void){bridge_codec_string(TSFP_TEXT_TO_UNICODE);}
static void bridge_RtlUnicodeStringToAnsiString(void){bridge_codec_string(TSFP_TEXT_TO_ANSI);}
static void bridge_codec_free(void) {
    uint32_t a[1];tsfp_guest_vm vm;
    if(bridge_codec_arguments(&vm,a,1))(void)tsfp_text_string_free(&vm,a[0]);
    g_eax=0;
}
static void bridge_RtlFreeUnicodeString(void){bridge_codec_free();}
static void bridge_RtlFreeAnsiString(void){bridge_codec_free();}
