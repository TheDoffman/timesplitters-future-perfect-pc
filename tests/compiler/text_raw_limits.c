/* Raw counts exceed16-bit descriptors; no guest allocator is needed. */
#include "guest_text_raw.h"
#include "host_text_codec.h"
#include "host_string_case.h"
#include <assert.h>
#include <string.h>
static unsigned char ram[200000];
static void *resolve(void *host,uint32_t a,uint32_t n,int write){(void)host;(void)write;return a && n && a<sizeof(ram) && n<=sizeof(ram)-a?ram+a:NULL;}
static uint32_t count(void){return (uint32_t)ram[16]|((uint32_t)ram[17]<<8)|((uint32_t)ram[18]<<16)|((uint32_t)ram[19]<<24);}
int main(void) {
    tsfp_guest_vm vm;tsfp_text_codec codec;tsfp_host_text_codec host;unsigned char old[4];
    memset(&vm,0,sizeof(vm));vm.resolve=resolve;tsfp_host_text_codec_init(&host,&codec);memset(ram,0xa5,sizeof(ram));memset(ram+10000,'a',32769);
    assert(tsfp_text_raw_convert(&vm,&codec,NULL,0,0,16,10000,32769,TSFP_RAW_UNICODE_SIZE)==0 && count()==65538);
    assert(tsfp_text_raw_convert(&vm,&codec,NULL,100000,65535,16,10000,32769,TSFP_RAW_TO_UNICODE)==0 && count()==65534);
    assert(ram[100000]=='a' && ram[100001]==0 && ram[165532]=='a' && ram[165533]==0 && ram[165534]==0xa5);
    for(unsigned i=0;i<32768;++i){ram[10000+i*2]='a';ram[10001+i*2]=0;}
    assert(tsfp_text_raw_convert(&vm,&codec,NULL,0,0,16,10000,65537,TSFP_RAW_ANSI_SIZE)==0 && count()==32768);
    assert(tsfp_text_raw_convert(&vm,&codec,tsfp_host_string_case,100000,100,16,10000,65537,TSFP_RAW_UPPER_ANSI)==UINT32_C(0x80000005) && count()==100);
    assert(ram[100000]=='A' && ram[100099]=='A');
    memcpy(old,ram+16,4);
    assert(tsfp_text_raw_convert(&vm,&codec,NULL,100000,100,16,10000,2,TSFP_RAW_UPPER_ANSI)==UINT32_C(0xc000000d));
    assert(tsfp_text_raw_convert(&vm,&codec,NULL,0,0,0,0,0,TSFP_RAW_ANSI_SIZE)==UINT32_C(0xc000000d));
    assert(tsfp_text_raw_convert(&vm,&codec,NULL,0,0,16,0,0,99)==UINT32_C(0xc000000d));
    assert(tsfp_text_raw_convert(&vm,NULL,NULL,0,0,16,0,0,0)==UINT32_C(0xc000000d));
    assert(tsfp_text_raw_convert(&vm,&codec,NULL,100000,100,16,UINT32_MAX-2,8,TSFP_RAW_TO_ANSI)==UINT32_C(0xc000000d));
    assert(!memcmp(old,ram+16,4));
    assert(tsfp_text_raw_convert(NULL,&codec,NULL,0,0,0,0,0,TSFP_RAW_TO_UNICODE)==0);
    return 0;
}
