/* Original synthetic caller stack. Xbox ordinal81 has one pointer argument;
 * a preserved caller register immediately follows it, as observed in the
 * supported title. No game instructions or bytes are embedded in this fixture. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t ULONG;
typedef unsigned char BOOLEAN;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"packet ABI line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static volatile uint32_t words[8];
static uint32_t g_eax,g_esp;
static unsigned reads,calls,invalid_reads;
static uint32_t argument(unsigned index) {
    CHECK(index<4);if(index)++invalid_reads;
    ++reads;return words[g_esp/4+index];
}
#define STACK_ARG(n) argument(n)
#define XBOX_TO_NATIVE(n) ((void *)(uintptr_t)(n))
static void xbox_IoStartNextPacket(void *device,BOOLEAN legacy_cancelable) {
    CHECK((uintptr_t)device==0x10203040 && !legacy_cancelable);++calls;
}
/*ACTUAL_BRIDGE*/
/*ACTUAL_CLEANUP*/
int main(void) {
    unsigned i;
    for(i=0;i<128;++i) {
        /* A consumed return address precedes one argument. Caller epilogue
         * restores its own saved register next; consuming two loses it. */
        words[0]=0xfeedface;words[1]=0x10203040;words[2]=0xcafebabe;words[3]=0xdeadbeef;
        g_esp=4;g_eax=0xffffffff;
        bridge_IoStartNextPacket();g_esp+=(uint32_t)stdcall_args_for_ordinal(81);
        CHECK(g_esp==8 && words[g_esp/4]==0xcafebabe && g_eax==0);
        CHECK(words[0]==0xfeedface && words[3]==0xdeadbeef);
    }
    CHECK(reads==128 && calls==128 && !invalid_reads);
    puts("PASS: one guest argument and preserved caller register through actual packet bridge");return 0;
}
