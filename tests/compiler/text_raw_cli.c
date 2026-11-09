/* Execute original raw charset service and real imports with guarded RAM. */
#include "guest_text_raw.h"
#include "host_text_codec.h"
#include "host_string_case.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static unsigned char ram[1024];static uint32_t g_eax,g_edx,g_ecx,g_esp,mode,test_codepage;
static unsigned lookups,frame_size,available,expect_frame;static int missing;
static int (*base_convert)(void *,unsigned,const unsigned char *,uint32_t,unsigned char *,uint32_t,uint32_t *);
static uint32_t (*base_prefix)(void *,const unsigned char *,uint32_t,uint32_t);
static uint32_t read32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void write32(uint32_t value){unsigned char b[4];for(unsigned i=0;i<4;++i)b[i]=(unsigned char)(value>>(i*8));assert(fwrite(b,1,4,stdout)==4);}
static void *resolve(void *host,uint32_t a,uint32_t n,int write) {
    uint32_t at=a&UINT32_C(0x7fffffff);(void)host;
    if(expect_frame){expect_frame=0;assert(!write && n==frame_size);if(at!=64 || n>available)return NULL;}
    if(!at || !n || at>=sizeof(ram) || n>sizeof(ram)-at || (at<496 && at+n>480))return NULL;
    if(write==1 && at<416 && at+n>400)return NULL;
    return ram+at;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm){++lookups;memset(vm,0,sizeof(*vm));if(!missing)vm->resolve=resolve;expect_frame=1;}
static int fault_convert(void *host,unsigned dir,const unsigned char *source,uint32_t n,unsigned char *out,uint32_t cap,uint32_t *written) {
    if((!out && (mode&1)) || (out && (mode&2)))return 0;
    if(!out && (mode&16) && dir==TSFP_TEXT_TO_UNICODE){*written=UINT32_MAX;return 1;}
    {int result=base_convert(host,dir,source,n,out,cap,written);if(result && out && (mode&8))++*written;if(result && out && (mode&32) && *written)--*written;return result;}
}
static uint32_t fault_prefix(void *host,const unsigned char *source,uint32_t n,uint32_t cap){return mode&4?UINT32_MAX:base_prefix(host,source,n,cap);}
static void fixture_codec(tsfp_host_text_codec *host,tsfp_text_codec *codec) {
    tsfp_host_text_codec_init(host,codec);if(test_codepage)host->codepage=test_codepage;
    base_convert=codec->convert;base_prefix=codec->ansi_prefix;codec->convert=fault_convert;codec->ansi_prefix=fault_prefix;
}
#define tsfp_host_text_codec_init fixture_codec
#include "guest_kernel_text_raw.inc"
#undef tsfp_host_text_codec_init
static void invoke(unsigned op) {
    if(op==0)bridge_RtlMultiByteToUnicodeN();else if(op==1)bridge_RtlMultiByteToUnicodeSize();
    else if(op==2)bridge_RtlUnicodeToMultiByteN();else if(op==3)bridge_RtlUnicodeToMultiByteSize();else bridge_RtlUpcaseUnicodeToMultiByteN();
}
static void setup(unsigned op,uint32_t stack,uint32_t flags) {
    mode=flags;g_eax=0x12345678;g_edx=0x87654321;g_ecx=0xabcdef01;g_esp=stack;lookups=expect_frame=0;missing=0;
    frame_size=available=op==1 || op==3?12:20;
}
int main(int argc,char **argv) {
    unsigned char packet[1052],before[1024];int bridge=argc>=2 && !strcmp(argv[1],"bridge");if(argc>2)test_codepage=(uint32_t)strtoul(argv[2],NULL,10);
#ifdef _WIN32
    assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
    for(unsigned op=0;op<5;++op)for(unsigned bad=0;bad<5;++bad) {
        unsigned n=op==1 || op==3?12:20;uint32_t stack=bad==0?0:bad==1?UINT32_MAX-(n-2):bad==4?1024:64;
        setup(op,stack,0);memset(ram,0xa5,sizeof(ram));memcpy(before,ram,sizeof(ram));if(bad==2)--available;if(bad==3)missing=1;
        invoke(op);assert(g_eax==UINT32_C(0xc000000d) && g_edx==0x87654321 && g_ecx==0xabcdef01 && g_esp==stack && lookups==1);assert(!memcmp(before,ram,sizeof(ram)));
    }
    while(fread(packet,1,sizeof(packet),stdin)==sizeof(packet)) {
        uint32_t op=read32(packet),out=read32(packet+4),cap=read32(packet+8),count=read32(packet+12),source=read32(packet+16),bytes=read32(packet+20),flags=read32(packet+24),result;
        assert(op<5);setup(op,flags&64?UINT32_C(0x80000040):64,flags);memcpy(ram,packet+28,sizeof(ram));
        if(bridge){invoke(op);result=g_eax;assert(lookups==1 && !expect_frame);}
        else {tsfp_guest_vm vm;tsfp_host_text_codec host;tsfp_text_codec codec;memset(&vm,0,sizeof(vm));vm.resolve=resolve;fixture_codec(&host,&codec);
            result=tsfp_text_raw_convert(&vm,&codec,tsfp_host_string_case,out,cap,count,source,bytes,op);assert(!lookups);}
        assert(g_edx==0x87654321 && g_ecx==0xabcdef01 && g_esp==(flags&64?UINT32_C(0x80000040):64));
        write32(result);assert(fwrite(ram,1,sizeof(ram),stdout)==sizeof(ram));
    }
    return 0;
}
