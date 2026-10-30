#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"share ABI line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[2048];static uint32_t args[5],g_eax;static unsigned count;
static uint32_t argument(unsigned i) {CHECK(i<count);return args[i];}
#define STACK_ARG(n) argument(n)
static void *bridge_object_output(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(!ctx && bytes==(address==0x201?7u:3u));return !address || address>sizeof(ram)-bytes?NULL:ram+address;
}
#include "guest_kernel_share.inc"
typedef uint32_t ULONG;
/*ACTUAL_CLEANUP*/
int main(void) {
    uint32_t caller[8]={0xfeedface,1,2,3,4,0xcafebabe,0xdeadbeef,0};
    uint32_t sp=4;
    sp+=(uint32_t)stdcall_args_for_ordinal(80);
    CHECK(sp==20 && caller[sp/4]==0xcafebabe && caller[6]==0xdeadbeef);
    CHECK(stdcall_args_for_ordinal(63)==20 && stdcall_args_for_ordinal(78)==8);

    unsigned i;memset(ram,0xa7,sizeof(ram));ram[0x103]=0x81;
    args[0]=1;args[1]=7;args[2]=0x101;args[3]=0x201;count=4;bridge_IoSetShareAccess();
    CHECK(ram[0x201]==1 && ram[0x202]==1 && ram[0x205]==1 && ram[0x103]==0xf3);
    CHECK(ram[0x200]==0xa7 && ram[0x208]==0xa7 && ram[0x100]==0xa7 && ram[0x104]==0xa7);
    args[2]=0x301;args[4]=0x100;count=5;bridge_IoCheckShareAccess();CHECK(!g_eax && ram[0x201]==1);
    args[4]=0x101;bridge_IoCheckShareAccess();CHECK(!g_eax && ram[0x201]==2);
    args[0]=0x301;args[1]=0x201;count=2;bridge_IoRemoveShareAccess();CHECK(ram[0x201]==1);
    args[0]=0x101;bridge_IoRemoveShareAccess();for(i=0;i<7;++i)CHECK(!ram[0x201+i]);
    args[0]=1;args[1]=7;args[2]=0;args[3]=0x201;args[4]=1;count=5;bridge_IoCheckShareAccess();CHECK(g_eax==TSFP_SHARE_BAD_ARGUMENT);
    args[2]=sizeof(ram)-1;bridge_IoCheckShareAccess();CHECK(g_eax==TSFP_SHARE_BAD_ARGUMENT);
    puts("PASS: four-argument Set, five-argument Check, two-argument Remove, BOOLEAN width and full-span mapping");return 0;
}
