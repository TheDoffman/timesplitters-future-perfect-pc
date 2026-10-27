#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint32_t memory[0x800000/4],calls,commits,dirty=0x10000;
#define WORD_AT(a) memory[(a)/4]
static int guest_writable(uint32_t a,unsigned n) { return a==0x1000 && n<=0x200; }
void sub_001922D0(void) {}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    va_list ap;va_start(ap,count);
    CHECK(commits==1 && fn==sub_001922D0 && count==3);
    CHECK(va_arg(ap,uint32_t)==0x1000);
    CHECK(va_arg(ap,uint32_t)==0x523910);
    CHECK(va_arg(ap,uint32_t)==UINT32_MAX);va_end(ap);calls++;
    return 1; /* A save error must remain owned by the original workflow. */
}
#include "pc_profile_accept.inc"
static void original(void) { commits++; WORD_AT(0x78AD24)=dirty; }
static void setup(void)
{
    memset(memory,0,sizeof memory);calls=commits=0;dirty=0x10000;
    WORD_AT(0x79094C)=101;WORD_AT(0x78AD34)=1;
    WORD_AT(0x74E934)=2;WORD_AT(0x7613E4)=0x1000;
}
int main(void)
{
    setup();pc_profile_accept(original);CHECK(calls==1 && commits==1);
    CHECK(WORD_AT(0x78AD24)==0x10000); /* Don't swallow save failure/dirty state. */
    for(unsigned mode=0;mode<6;mode++) {
        setup();
        if(mode==0)WORD_AT(0x79094C)=4;
        if(mode==1)dirty=0;
        if(mode==2)dirty|=1;
        if(mode==3)WORD_AT(0x78AD34)=2;
        if(mode==4)WORD_AT(0x7613E4)=0;
        if(mode==5)WORD_AT(0x78AD34)=UINT32_MAX;
        pc_profile_accept(original);CHECK(calls==0 && commits==1);
    }
    puts("PASS: accepted profile save routing and guards");
}
