/* Original status lookup (numeric ABI facts) and guest-byte diagnostic reader. */
#include "guest_rtl_services.h"
uint32_t tsfp_rtl_status_error(uint32_t status,tsfp_rtl_error_fallback fallback,void *context) {
    static const uint32_t pairs[][2]={
        {0,0},{0x103,997},{0x80000005,234},{0x80000006,18},
        {0xc0000001,31},{0xc0000002,120},{0xc0000008,6},{0xc000000d,87},
        {0xc000000f,2},{0xc0000011,38},{0xc0000017,8},{0xc0000021,1455},
        {0xc0000022,5},{0xc0000034,2},{0xc0000035,183},{0xc000003a,3},
        {0xc000009a,1450},{0xc00000bb,50},{0xc0000120,1223}
    };
    size_t low=0,high=sizeof(pairs)/sizeof(pairs[0]);
    while(low<high) {
        size_t middle=low+(high-low)/2;
        if(pairs[middle][0]==status)return pairs[middle][1];
        if(pairs[middle][0]<status)low=middle+1;else high=middle;
    }
    return fallback?fallback(context,status):317;
}
int tsfp_rtl_text(const tsfp_guest_vm *vm,uint32_t address,char *output,uint32_t capacity) {
    uint32_t i;const unsigned char *p;
    if(!output || capacity<2)return 0;
    if(!vm || !vm->resolve || !address)goto invalid;
    for(i=0;i<capacity-1;++i) {
        if(address>UINT32_MAX-i)goto invalid;
        p=vm->resolve(vm->host,address+i,1,0);if(!p)goto invalid;
        output[i]=(char)*p;if(!*p)return 1;
    }
    output[capacity-2]='~';output[capacity-1]=0;return 2;
invalid:
    output[0]='?';output[1]=0;return 0;
}
