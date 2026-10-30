#include "guest_printf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"printf line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[32768],expected[32768],before[32768];static uint32_t denied,readonly;static unsigned maps,alias;
static uint32_t random_state=239;
static uint32_t random32(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static void put(unsigned at,unsigned bytes,uint64_t value){unsigned i;for(i=0;i<bytes;++i)ram[at+i]=(unsigned char)(value>>(8*i));}
static void *resolve(void *context,uint32_t address,uint32_t bytes,int writable) {
    CHECK(context==ram && address && bytes && address<=UINT32_MAX-(bytes-1) && (writable==0 || writable==1));++maps;
    if(address>=sizeof(ram) || bytes>sizeof(ram)-address || (denied && address<=denied && bytes>denied-address) ||
       (writable && readonly && address<=readonly && bytes>readonly-address))return NULL;
    if(alias && address>=20000 && address<20008 && bytes<=20008-address)return ram+(alias==1?2049:8193)+(address-20000);
    return ram+address;
}
static tsfp_guest_vm vm;
static void init(const char *format){memset(ram,0xa5,sizeof(ram));memcpy(ram+65,format,strlen(format)+1);denied=readonly=0;alias=0;}
static uint32_t run(unsigned capacity){return tsfp_guest_printf(&vm,8193,capacity,65,516,NULL);}
typedef struct allocations {unsigned calls,fail_at,live;} allocations;
static void *resize(void *context,void *p,size_t size) {
    allocations *a=context;void *next;int fresh=p==NULL;CHECK(!memcmp(ram,before,sizeof(ram)));++a->calls;
    if(a->fail_at && a->calls==a->fail_at)return NULL;
    next=realloc(p,size);if(next && fresh)++a->live;return next;
}
static void release(void *context,void *p){allocations *a=context;CHECK(p && a->live);--a->live;free(p);}
static void literal_cases(void) {
    uint64_t bits;double number=3.25;unsigned i;uint32_t calls;char text[6001];
    init("%x:%I64d:%.2f:%u");put(516,4,0xabcdef01);put(520,8,UINT64_C(0xffffffffffffffd6));memcpy(&bits,&number,8);put(528,8,bits);put(536,4,17);
    CHECK(run(128)==strlen("abcdef01:-42:3.25:17") && !strcmp((char *)ram+8193,"abcdef01:-42:3.25:17"));
    init("%.2f:%ld:%u");memcpy(&bits,&number,8);put(516,8,bits);put(524,4,UINT32_C(0xffffffff));put(528,4,21);
    CHECK(run(128)==strlen("3.25:-1:21") && !strcmp((char *)ram+8193,"3.25:-1:21"));
    init("[%.*s]");put(516,4,UINT32_MAX);put(520,4,1025);memcpy(ram+1025,"hello",6);CHECK(run(64)==7 && !strcmp((char *)ram+8193,"[hello]"));
    init("%cX");put(516,4,0);CHECK(run(64)==2 && !ram[8193] && ram[8194]=='X' && !ram[8195]);
    memset(text,'a',6000);text[6000]=0;init("%s");put(516,4,1025);memcpy(ram+1025,text,sizeof(text));CHECK(run(7000)==6000 && !memcmp(ram+8193,text,sizeof(text)));
    init("ab%hhnC%hnD%nE%I64n");put(516,4,2049);put(520,4,2065);put(524,4,2081);put(528,4,2097);
    CHECK(run(64)==5 && !memcmp(ram+8193,"abCDE",6));CHECK(ram[2049]==2 && ram[2050]==0xa5 && ram[2065]==3 && !ram[2066] && ram[2067]==0xa5);
    CHECK(ram[2081]==4 && !ram[2082] && !ram[2083] && !ram[2084]);CHECK(ram[2097]==5);for(i=2098;i<2105;++i)CHECK(!ram[i]);
    /* A later read sees prior staged %n bytes, without publishing them early. */
    init("A%n%s");put(516,4,2049);put(520,4,2049);CHECK(run(64)==2 && ram[8193]=='A' && ram[8194]==1 && !ram[8195] && ram[2049]==1 && !ram[2050]);
    init("A%n%s");alias=1;put(516,4,20000);put(520,4,2049);CHECK(run(64)==2 && ram[8194]==1 && ram[2049]==1 && !ram[2050]);
    init("A%n%s");put(516,4,2049);put(520,4,UINT32_MAX);memcpy(before,ram,sizeof(ram));CHECK(!run(64) && !memcmp(before,ram,sizeof(ram)));
    init("ab%n");put(516,4,8193);memcpy(before,ram,sizeof(ram));CHECK(!run(64) && !memcmp(before,ram,sizeof(ram)));
    init("ab%n");alias=2;put(516,4,20000);memcpy(before,ram,sizeof(ram));CHECK(!run(64) && !memcmp(before,ram,sizeof(ram)));
    init("X:%s");put(516,4,8193);memcpy(ram+8193,"before",7);CHECK(run(64)==8 && !strcmp((char *)ram+8193,"X:before"));
    init("%n");put(516,4,0);CHECK(!run(64) && !ram[8193]);
    init("A%u");CHECK(tsfp_guest_printf(&vm,8193,2,65,0,NULL)==1 && ram[8193]=='A' && !ram[8194]);
    init("A%n");put(516,4,2049);CHECK(!run(1) && !ram[8193] && ram[2049]==0xa5);
    init("hello");memcpy(before,ram,sizeof(ram));calls=maps;CHECK(!run(0) && maps==calls && !memcmp(before,ram,sizeof(ram)));
    init("%I64u");put(516,8,UINT64_MAX);denied=520;memcpy(before,ram,sizeof(ram));CHECK(!run(64) && !memcmp(before,ram,sizeof(ram)));denied=0;
    init("abc");readonly=8195;memcpy(before,ram,sizeof(ram));CHECK(!run(64) && !memcmp(before,ram,sizeof(ram)));readonly=0;
    init("%99999999999999999999d");memcpy(before,ram,sizeof(ram));CHECK(!run(64) && !memcmp(before,ram,sizeof(ram)));
    init("%*d");put(516,4,0x80000000u);put(520,4,17);memcpy(before,ram,sizeof(ram));CHECK(!run(64) && !memcmp(before,ram,sizeof(ram)));
    init("%I64d:%I64u");put(516,8,UINT64_C(0x8000000000000000));put(524,8,UINT64_MAX);
    CHECK(run(128)==strlen("-9223372036854775808:18446744073709551615") && !strcmp((char *)ram+8193,"-9223372036854775808:18446744073709551615"));
    init("%.1f");put(516,8,UINT64_C(0x8000000000000000));CHECK(run(64)==4 && !strcmp((char *)ram+8193,"-0.0"));
    init("%%:%Q:");CHECK(run(64)==5 && !strcmp((char *)ram+8193,"%:%Q:"));
    /* Every allocator failure preserves both destination and count outputs. */
    for(i=1;i<12;++i) {
        allocations state={0};tsfp_printf_allocator allocator={&state,resize,release};uint32_t result;
        init("A%n%s");put(516,4,2049);put(520,4,1025);memset(ram+1025,'b',1000);ram[2025]=0;memcpy(before,ram,sizeof(ram));state.fail_at=i;
        result=tsfp_guest_printf(&vm,8193,2000,65,516,&allocator);CHECK(!state.live);
        if(state.calls>=i)CHECK(!result && !memcmp(before,ram,sizeof(ram)));else CHECK(result==1001 && ram[2049]==1);
    }
}
enum value_kind {SIGNED32,UNSIGNED32,SIGNED64,UNSIGNED64,FLOAT64,STRING,CHARACTER};
typedef struct sample {const char *guest,*native;enum value_kind kind;int stars;} sample;
static const sample samples[]={
    {"%d","%d",SIGNED32,0},{"%ld","%d",SIGNED32,0},{"%I32d","%d",SIGNED32,0},{"%hhd","%hhd",SIGNED32,0},{"%hd","%hd",SIGNED32,0},
    {"%+019d","%+019d",SIGNED32,0},{"%*.*i","%*.*i",SIGNED32,1},{"%#x","%#x",UNSIGNED32,0},{"%#020X","%#020X",UNSIGNED32,0},
    {"%o","%o",UNSIGNED32,0},{"%zu","%u",UNSIGNED32,0},{"%hhu","%hhu",UNSIGNED32,0},{"%hu","%hu",UNSIGNED32,0},
    {"%lld","%lld",SIGNED64,0},{"%I64d","%lld",SIGNED64,0},{"%jd","%lld",SIGNED64,0},{"%*.*I64d","%*.*lld",SIGNED64,1},
    {"%#I64x","%#llx",UNSIGNED64,0},{"%llu","%llu",UNSIGNED64,0},{"%qu","%llu",UNSIGNED64,0},
    {"%.17g","%.17g",FLOAT64,0},{"%+.8e","%+.8e",FLOAT64,0},{"%*.*f","%*.*f",FLOAT64,1},{"%.6Lf","%.6f",FLOAT64,0},{"%a","%a",FLOAT64,0},
    {"%p","%08x",UNSIGNED32,0},{"%#p","%#08x",UNSIGNED32,0},{"%c","%c",CHARACTER,0},{"%*.*s","%*.*s",STRING,1},{"%-30.15s","%-30.15s",STRING,0}
};
int main(void) {
    unsigned iteration;vm.host=ram;vm.resolve=resolve;literal_cases();
    for(iteration=0;iteration<14000;++iteration) {
        const sample *sample_=&samples[iteration%(sizeof(samples)/sizeof(samples[0]))];uint32_t raw=random32(),capacity=random32()%1025,cursor=516;
        uint64_t wide=((uint64_t)random32()<<32)|random32();int32_t signed32;int64_t signed64;double floating;char reference[16384],text[1501];
        int width=(int)(random32()%2001)-1000,precision=(int)(random32()%22)-3,length=-1;uint32_t stored;unsigned i;
        if((wide&UINT64_C(0x7ff0000000000000))==UINT64_C(0x7ff0000000000000))wide^=UINT64_C(0x0010000000000000);
        memcpy(&signed32,&raw,4);memcpy(&signed64,&wide,8);memcpy(&floating,&wide,8);init(sample_->guest);
        for(i=0;i<1500;++i)text[i]=(char)('a'+i%26);text[1500]=0;memcpy(ram+1025,text,sizeof(text));
        if(sample_->stars){put(cursor,4,(uint32_t)width);put(cursor+4,4,(uint32_t)precision);cursor+=8;}
#define RENDER(v) (sample_->stars?snprintf(reference,sizeof(reference),sample_->native,width,precision,(v)):snprintf(reference,sizeof(reference),sample_->native,(v)))
        switch(sample_->kind) {
        case SIGNED32:put(cursor,4,raw);length=RENDER((int)signed32);break;
        case UNSIGNED32:put(cursor,4,raw);length=RENDER((unsigned)raw);break;
        case SIGNED64:put(cursor,8,wide);length=RENDER((long long)signed64);break;
        case UNSIGNED64:put(cursor,8,wide);length=RENDER((unsigned long long)wide);break;
        case FLOAT64:put(cursor,8,wide);length=RENDER(floating);break;
        case STRING:put(cursor,4,1025);length=RENDER(text);break;
        case CHARACTER:put(cursor,4,raw);length=RENDER((int)(raw&255));break;
        }
#undef RENDER
        CHECK(length>=0 && (size_t)length<sizeof(reference));stored=capacity?((uint32_t)length<capacity?(uint32_t)length:capacity-1):0;
        memcpy(expected,ram,sizeof(ram));if(capacity){memcpy(expected+8193,reference,stored);expected[8193+stored]=0;}
        if(run(capacity)!=stored || memcmp(expected,ram,sizeof(ram))){fprintf(stderr,"vector%u %s cap%u width%d precision%d\n",iteration,sample_->guest,capacity,width,precision);return 1;}
    }
    puts("PASS: 14000 typed host-CRT oracle cases, guest ABI/float/long widths, large atoms, embedded NUL, count overlays, allocation failures and guards");return 0;
}
