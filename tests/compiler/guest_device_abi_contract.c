/* Original synthetic caller stack; no game instructions/bytes. */
#include "guest_device_factory.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"device ABI line%d %s\n",__LINE__,#x);exit(1);}}while(0)
typedef uint32_t ULONG;
static tsfp_device_factory bridge_devices;
static volatile uint32_t words[10];static uint32_t g_eax,g_esp,observed_output;
static unsigned allowed,reads,creates,deletes,expected_exclusive;
static int failed;
static uint32_t argument(unsigned i) {CHECK(i<allowed);++reads;return words[g_esp/4+i];}
#define STACK_ARG(i) argument(i)
int tsfp_device_factory_create(tsfp_device_factory *f,uint32_t driver,uint32_t extension,uint32_t name,uint32_t type,int exclusive,uint32_t target) {
    CHECK(f==&bridge_devices && driver==0x10203040 && extension==0x170 && name==0x201);
    CHECK(type==0x3a && exclusive==(int)expected_exclusive && target==0x801);++creates;
    if(failed)return TSFP_DEVICE_COLLISION;
    observed_output=0x20406080;return TSFP_DEVICE_OK;
}
int tsfp_device_factory_delete(tsfp_device_factory *f,uint32_t body) {
    CHECK(f==&bridge_devices && body==0x20406080);++deletes;return 1;
}
#include "guest_kernel_devices.inc"
/*ACTUAL_CLEANUP*/
int main(void) {
    unsigned i;
    for(i=0;i<128;++i) {
        words[0]=0xfeedface;words[1]=0x10203040;words[2]=0x170;words[3]=0x201;
        words[4]=0x3a;words[5]=i&1?0x107:0x100;words[6]=0x801;
        words[7]=0xcafebabe;words[8]=0xdeadbeef;expected_exclusive=i&1;failed=(i%3)==0;
        observed_output=0xcccccccc;g_eax=0xffffffff;g_esp=4;allowed=6;
        bridge_IoCreateDevice();g_esp+=(uint32_t)stdcall_args_for_ordinal(65);
        CHECK(g_esp==28 && words[g_esp/4]==0xcafebabe && words[0]==0xfeedface && words[8]==0xdeadbeef);
        CHECK(g_eax==(failed?0xc0000035u:0) && observed_output==(failed?0xccccccccu:0x20406080u));
        words[0]=0xfeedface;words[1]=0x20406080;words[2]=0xcafebabe;words[3]=0xdeadbeef;
        g_esp=4;allowed=1;g_eax=0xffffffff;
        bridge_IoDeleteDevice();g_esp+=(uint32_t)stdcall_args_for_ordinal(68);
        CHECK(g_esp==8 && words[g_esp/4]==0xcafebabe && words[0]==0xfeedface && words[3]==0xdeadbeef && !g_eax);
    }
    CHECK(reads==896 && creates==128 && deletes==128);
    puts("PASS: candidate device adapters and actual dispatcher cleanup preserve six/one-argument caller frames");return 0;
}
