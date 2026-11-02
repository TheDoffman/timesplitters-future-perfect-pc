/* Execute original provider/import against bounded guest RAM with exact frames. */
#include "guest_memory_ops.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static unsigned char ram[512];
static uint32_t g_eax,g_edx,g_esp,g_ecx;
static unsigned lookups,expected_frame,available,expect_frame;
static int missing;
static uint32_t read32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void *resolve(void *host,uint32_t address,uint32_t bytes,int write) {
    uint32_t at=address&UINT32_C(0x7fffffff);(void)host;
    if(expect_frame) {
        expect_frame=0;assert(!write && bytes==expected_frame);
        if(at!=32 || bytes>available)return NULL;
    }
    if(!at || !bytes || at>=sizeof(ram) || bytes>sizeof(ram)-at)return NULL;
    if(at<480 && at+bytes>464)return NULL;
    if(write && at<416 && at+bytes>400)return NULL;
    return ram+at;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    ++lookups;memset(vm,0,sizeof(*vm));if(!missing)vm->resolve=resolve;expect_frame=1;
}
#include "guest_kernel_memory_ops.inc"
static unsigned words(unsigned op){return op==4 || op==5?0:op==1 || op==6?2:3;}
static void invoke(unsigned op) {
    switch(op) {
        case 0:bridge_RtlFillMemory();break;case 1:bridge_RtlZeroMemory();break;
        case 2:bridge_RtlFillMemoryUlong();break;case 3:bridge_RtlMoveMemory();break;
        case 4:bridge_RtlUlongByteSwap();break;case 5:bridge_RtlUshortByteSwap();break;
        case 6:bridge_RtlMapGenericMask();break;default:assert(0);
    }
}
static void setup(unsigned op,uint32_t stack) {
    g_eax=0x12345678;g_edx=0x87654321;g_ecx=0x12345678;g_esp=stack;lookups=expect_frame=0;missing=0;
    expected_frame=available=words(op)*4;
}
static void write32(uint32_t value){unsigned char out[4];for(unsigned i=0;i<4;++i)out[i]=(unsigned char)(value>>(i*8));assert(fwrite(out,1,4,stdout)==4);}
int main(void) {
    unsigned char request[528],initial[512];tsfp_guest_vm vm;memset(&vm,0,sizeof(vm));vm.resolve=resolve;
#ifdef _WIN32
    assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
    memset(ram,0xa5,sizeof(ram));memcpy(initial,ram,sizeof(ram));
    assert(tsfp_memory_fill(NULL,0,0,42,1));assert(tsfp_memory_fill(NULL,UINT32_MAX,3,42,4));
    assert(tsfp_memory_move(NULL,0,0,0));assert(!tsfp_memory_fill(NULL,16,1,42,1));
    assert(!tsfp_memory_fill(&vm,16,16,42,2));assert(!tsfp_memory_move(NULL,16,32,1));
    assert(!tsfp_memory_map_mask(NULL,16,64));assert(tsfp_memory_byte_swap(123,3)==0);
    assert(memcmp(ram,initial,sizeof(ram))==0);
    for(unsigned op=0;op<7;++op)for(unsigned kind=0;kind<5;++kind) {
        setup(op,kind==0?0:kind==1?UINT32_MAX-words(op)*4+2:kind==4?512:32);
        if(kind==2)--available;if(kind==3)missing=1;
        invoke(op);assert(g_eax==(op==4?0x78563412u:op==5?0x7856u:0u) && g_edx==0x87654321 && g_ecx==0x12345678 && lookups==(op==4 || op==5?0u:1u));
        assert(g_esp==(kind==0?0:kind==1?UINT32_MAX-words(op)*4+2:kind==4?512u:32u));
        assert(memcmp(ram,initial,sizeof(ram))==0);
    }
    while(fread(request,1,sizeof(request),stdin)==sizeof(request)) {
        uint32_t op=read32(request),a=read32(request+4),b=read32(request+8),c=read32(request+12),direct;
        assert(op<7);memcpy(initial,request+16,sizeof(initial));memcpy(ram,initial,sizeof(ram));expect_frame=0;
        switch(op) {
            case 0:direct=(uint32_t)tsfp_memory_fill(&vm,a,b,c,1);break;
            case 1:direct=(uint32_t)tsfp_memory_fill(&vm,a,b,0,1);break;
            case 2:direct=(uint32_t)tsfp_memory_fill(&vm,a,b,c,4);break;
            case 3:direct=(uint32_t)tsfp_memory_move(&vm,a,b,c);break;
            case 4:direct=tsfp_memory_byte_swap(a,4);break;
            case 5:direct=tsfp_memory_byte_swap(a,2);break;
            default:direct=(uint32_t)tsfp_memory_map_mask(&vm,a,b);break;
        }
        write32(direct);assert(fwrite(ram,1,sizeof(ram),stdout)==sizeof(ram));
        memcpy(ram,initial,sizeof(ram));setup(op,a&UINT32_C(0x80000000)?UINT32_C(0x80000020):32);
        g_ecx=a;invoke(op);assert(g_edx==0x87654321 && g_ecx==a && lookups==(op==4 || op==5?0u:1u) && !expect_frame);
        assert(g_esp==(a&UINT32_C(0x80000000)?UINT32_C(0x80000020):32));
        write32(g_eax);assert(fwrite(ram,1,sizeof(ram),stdout)==sizeof(ram));
    }
    return 0;
}
