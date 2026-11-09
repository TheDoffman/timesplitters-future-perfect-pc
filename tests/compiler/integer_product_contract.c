#define _CRT_SECURE_NO_WARNINGS
/* Execute the real product import with guarded little-endian guest frames. */
#include "guest_virtual_memory.h"
#include "guest_integer_product.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static unsigned char ram[64];
static uint32_t g_eax,g_edx,g_esp;
static unsigned available,reads,lookups;
static int missing;
static void *resolve(void *host,uint32_t address,uint32_t bytes,int write) {
    uint32_t at=address&UINT32_C(0x7fffffff);(void)host;
    ++reads;assert(bytes==12 && !write);
    if(at!=16 || bytes>available)return NULL;
    return ram+at;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    ++lookups;memset(vm,0,sizeof(*vm));if(!missing)vm->resolve=resolve;
}
#include "guest_kernel_product.inc"
static void setup(uint64_t a,uint32_t b,uint32_t stack) {
    memset(ram,0xa5,sizeof(ram));g_eax=0x12345678;g_edx=0x87654321;g_esp=stack;
    for(unsigned i=0;i<8;++i)ram[16+i]=(unsigned char)(a>>(i*8));
    for(unsigned i=0;i<4;++i)ram[24+i]=(unsigned char)(b>>(i*8));
    available=12;reads=lookups=0;missing=0;
}
int main(void) {
    uint64_t a;uint32_t b;unsigned char before[64];
    for(unsigned kind=0;kind<5;++kind) {
        setup(UINT64_MAX,UINT32_MAX,kind==0?0:kind==1?UINT32_MAX-10:16);
        if(kind==2)available=11;
        if(kind==3)missing=1;
        if(kind==4)g_esp=17;
        memcpy(before,ram,sizeof(ram));bridge_RtlExtendedIntegerMultiply();
        assert(g_eax==0x12345678 && g_edx==0x87654321 && lookups==1);
        assert(reads==(kind==2 || kind==4?1u:0u));
        assert(g_esp==(kind==0?0:kind==1?UINT32_MAX-10:kind==4?17u:16u));
        assert(memcmp(before,ram,sizeof(ram))==0);
    }
    while(scanf("%" SCNx64 " %" SCNx32,&a,&b)==2) {
        uint64_t direct=tsfp_guest_integer_product(a,b);
        uint32_t stack=(b&1)?UINT32_C(0x80000010):16;
        setup(a,b,stack);memcpy(before,ram,sizeof(ram));bridge_RtlExtendedIntegerMultiply();
        assert(g_esp==stack && reads==1 && lookups==1 && memcmp(before,ram,sizeof(ram))==0);
        printf("%016" PRIx64 " %08" PRIx32 "%08" PRIx32 "\n",direct,g_edx,g_eax);
    }
    return 0;
}
