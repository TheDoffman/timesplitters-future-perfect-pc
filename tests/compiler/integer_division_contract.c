#define _CRT_SECURE_NO_WARNINGS
/* Real imports against checked, aliased, readonly and truncated guest spans. */
#include "guest_virtual_memory.h"
#include "guest_integer_division.h"
#include <assert.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
static unsigned char ram[256];
static uint32_t g_eax,g_edx,g_esp,g_xbox_kernel_caller=0x11223344;
static unsigned available,frame_bytes,reads,lookups,faults;
static int missing;
static jmp_buf fault_jump;
void tsfp_guest_divide_error(uint32_t site,unsigned reason) {
    assert(site==0x11223344 && !reason);++faults;longjmp(fault_jump,1);
}
static void *resolve(void *host,uint32_t address,uint32_t bytes,int write) {
    uint32_t at=address&UINT32_C(0x7fffffff);(void)host;
    if(!write){++reads;if(at!=64)return NULL;assert(bytes==frame_bytes);if(bytes>available)return NULL;}
    if(!at || at>=sizeof(ram) || bytes>sizeof(ram)-at || (write && at<240 && at+bytes>232))return NULL;
    return ram+at;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    ++lookups;memset(vm,0,sizeof(*vm));if(!missing)vm->resolve=resolve;
}
#include "guest_kernel_division.inc"
static void put(uint32_t at,uint32_t value) {
    for(unsigned i=0;i<4;++i)ram[at+i]=(unsigned char)(value>>(i*8));
}
static void setup(unsigned op,uint64_t a,uint64_t b,uint32_t shift,uint32_t out) {
    memset(ram,0xa5,sizeof(ram));g_eax=0x12345678;g_edx=0x87654321;g_esp=64;
    put(64,(uint32_t)a);put(68,(uint32_t)(a>>32));put(72,(uint32_t)b);
    put(76,op?(uint32_t)(b>>32):out);if(op)put(80,shift);
    frame_bytes=available=op?20:16;reads=lookups=faults=0;missing=0;
}
static void invoke(unsigned op){if(op)bridge_RtlExtendedMagicDivide();else bridge_RtlExtendedLargeIntegerDivide();}
int main(void) {
    unsigned op;uint64_t a,b;uint32_t shift,out;unsigned char before[256];
    tsfp_integer_quotient untouched={UINT64_C(0xa5a5a5a5a5a5a5a5),UINT32_C(0x5a5a5a5a)};
    assert(!tsfp_guest_integer_divide(123,0,&untouched));
    assert(untouched.quotient==UINT64_C(0xa5a5a5a5a5a5a5a5) && untouched.remainder==0x5a5a5a5a);
    assert(!tsfp_guest_integer_divide(123,3,NULL));
    for(op=0;op<2;++op)for(unsigned kind=0;kind<5;++kind) {
        setup(op,UINT64_MAX,3,1,16);
        if(kind==0)g_esp=0;
        if(kind==1)g_esp=UINT32_MAX-frame_bytes+2;
        if(kind==2)--available;
        if(kind==3)missing=1;
        if(kind==4)g_esp=256;
        memcpy(before,ram,sizeof(ram));
        if(setjmp(fault_jump)==0)invoke(op);
        assert(!faults && g_eax==0x12345678 && g_edx==0x87654321 && lookups==1);
        assert(memcmp(before,ram,sizeof(ram))==0);
    }
    while(scanf("%u %" SCNx64 " %" SCNx64 " %" SCNx32 " %" SCNx32,&op,&a,&b,&shift,&out)==5) {
        tsfp_integer_quotient direct={UINT64_C(0xa5a5a5a5a5a5a5a5),UINT32_C(0x5a5a5a5a)};
        if(op)direct.quotient=tsfp_guest_magic_divide(a,b,(uint8_t)shift);
        else (void)tsfp_guest_integer_divide(a,(uint32_t)b,&direct);
        setup(op,a,b,shift,out);if(out&UINT32_C(0x80000000))g_esp|=UINT32_C(0x80000000);
        out=g_esp;
        if(setjmp(fault_jump)==0)invoke(op);
        assert(g_esp==out && lookups==1 && reads==1);
        printf("%016" PRIx64 " %08" PRIx32 " %08" PRIx32 "%08" PRIx32 " %u ",direct.quotient,direct.remainder,g_edx,g_eax,faults);
        for(unsigned i=0;i<sizeof(ram);++i)printf("%02x",(unsigned)ram[i]);
        printf("\n");
    }
    return 0;
}
