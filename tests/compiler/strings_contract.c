#include "guest_strings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"strings line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[150000],before[150000];static unsigned maps;static uint32_t blocked,readonly;
static uint32_t read32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(unsigned at,unsigned bytes,uint32_t value){unsigned i;for(i=0;i<bytes;++i)ram[at+i]=(unsigned char)(value>>(8*i));}
static void record(unsigned at,unsigned length,unsigned maximum,uint32_t buffer){put(at,2,length);put(at+2,2,maximum);put(at+4,4,buffer);}
static void *resolve(void *context,uint32_t address,uint32_t bytes,int writable) {
    CHECK(context==ram && bytes && address<=UINT32_MAX-(bytes-1) && (writable==0 || writable==1));++maps;
    if(address>=sizeof(ram) || bytes>sizeof(ram)-address || (blocked && address<=blocked && bytes>blocked-address) ||
       (writable && readonly && address<=readonly && bytes>readonly-address))return NULL;
    if(address==100000)return ram+17; /* Distinct guest address, same host storage. */
    return ram+address;
}
static uint32_t upper(uint32_t value,unsigned width){(void)width;return value>='a' && value<='z'?value-32:value;}
static tsfp_guest_vm vm;
static void literals(void) {
    unsigned i;int32_t difference=91;memset(ram,0xa5,sizeof(ram));vm.host=ram;vm.resolve=resolve;
    /* Embedded NUL is data, empty records can have null buffers, and all bytes
     * are compared unsigned. Source MaximumLength is deliberately irrelevant. */
    memcpy(ram+65,"a\0x",3);memcpy(ram+97,"a\0y",3);record(17,3,0,65);record(33,3,0,97);
    CHECK(tsfp_string_compare(&vm,17,33,1,NULL,&difference) && difference==-1);
    record(17,0,0,0);record(33,0,0,0);CHECK(tsfp_string_compare(&vm,17,33,2,NULL,&difference) && !difference);
    record(17,1,1,65);record(33,1,1,97);ram[65]=255;ram[97]=1;CHECK(tsfp_string_compare(&vm,17,33,1,NULL,&difference) && difference==254);
    difference=91;CHECK(!tsfp_string_compare(&vm,17,33,2,NULL,&difference) && difference==91);
    CHECK(!tsfp_string_compare(&vm,17,33,3,NULL,&difference));CHECK(!tsfp_string_compare(&vm,17,33,1,NULL,NULL));
    /* Literal Gregorian-independent NT init boundaries; no length/capacity wrap. */
    memset(ram+1000,'A',65536);CHECK(!tsfp_string_initialize(&vm,17,1000,1));CHECK(ram[17]==254 && ram[18]==255 && ram[19]==255 && ram[20]==255);
    for(i=1000;i<1000+65536;i+=2){ram[i]='A';ram[i+1]=0;}
    CHECK(!tsfp_string_initialize(&vm,17,1000,2));CHECK(ram[17]==252 && ram[18]==255 && ram[19]==254 && ram[20]==255);
    CHECK(!tsfp_string_initialize(&vm,17,0,2));for(i=17;i<25;++i)CHECK(!ram[i]);
    /* Overlapping bodies use memmove before case mapping; no host-width casts. */
    memcpy(ram+65,"abcdef",6);record(17,3,7,67);record(33,6,6,65);
    CHECK(!tsfp_string_update(&vm,17,33,1,TSFP_STRING_UPPER,upper));CHECK(!memcmp(ram+67,"ABCDEF",6) && ram[17]==6);
    memcpy(ram+65,"abc",3);record(17,3,6,65);record(33,3,3,65);
    CHECK(!tsfp_string_update(&vm,17,33,1,TSFP_STRING_APPEND,NULL));CHECK(!memcmp(ram+65,"abcabc",6));
    record(17,6,6,65);memcpy(before,ram,sizeof(ram));CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_APPEND,NULL)==0xc0000023u && !memcmp(before,ram,sizeof(ram)));
    /* Denied full spans and descriptor/body overlap preserve all RAM. */
    record(17,2,8,65);record(33,6,6,97);blocked=101;memcpy(before,ram,sizeof(ram));
    CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_COPY,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));blocked=0;
    readonly=69;CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_COPY,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));readonly=0;
    record(17,0,8,18);memcpy(before,ram,sizeof(ram));CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_COPY,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    record(17,0,8,100000);memcpy(before,ram,sizeof(ram));CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_COPY,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    record(17,4,8,0);memcpy(before,ram,sizeof(ram));CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_APPEND,NULL)==0xc0000023u && !memcmp(before,ram,sizeof(ram)));
    record(33,1,1,97);memcpy(before,ram,sizeof(ram));CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_APPEND,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    record(17,0,8,65);record(33,3,3,97);memcpy(before,ram,sizeof(ram));CHECK(tsfp_string_update(&vm,17,33,2,TSFP_STRING_COPY,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    CHECK(tsfp_string_update(&vm,17,33,1,TSFP_STRING_UPPER,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    CHECK(tsfp_string_initialize(&vm,UINT32_MAX-3,65,1)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    CHECK(tsfp_string_initialize(&vm,17,UINT32_MAX,2)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    CHECK(tsfp_string_initialize(&vm,17,0,3)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    CHECK(tsfp_string_update(&vm,17,33,1,(enum tsfp_string_operation)-1,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    CHECK(tsfp_string_update(&vm,17,33,1,(enum tsfp_string_operation)4,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    CHECK(tsfp_string_update(&vm,17,33,3,TSFP_STRING_COPY,NULL)==0xc000000du && !memcmp(before,ram,sizeof(ram)));
    /* Prefix counts, not a total match count; pattern ignores trailing bytes. */
    memcpy(ram+65,"abxd",4);memcpy(ram+97,"abyd",4);CHECK(tsfp_memory_match(&vm,65,97,4)==2);
    put(65,4,0xdeadbeef);put(69,4,0xdeadbeef);ram[73]=0;CHECK(tsfp_memory_pattern(&vm,65,11,0xdeadbeef)==8);
    maps=0;CHECK(!tsfp_memory_match(&vm,0,0,0) && !tsfp_memory_pattern(&vm,0,3,0));CHECK(!maps);
    CHECK(!tsfp_memory_match(&vm,UINT32_MAX-1,65,4));CHECK(!tsfp_memory_pattern(&vm,UINT32_MAX-1,8,0));
}
int main(void) {
    unsigned char row[1056];size_t count;unsigned cases=0;literals();
#ifdef _WIN32
    CHECK(_setmode(_fileno(stdin),_O_BINARY)!=-1);
#endif
    while((count=fread(row,1,sizeof(row),stdin))!=0) {
        uint32_t op,width,a,b,c,expected,result=0;int valid=1;int32_t difference=0;
        CHECK(count==sizeof(row));op=read32(row);width=read32(row+4);a=read32(row+8);b=read32(row+12);c=read32(row+16);expected=read32(row+20);
        memset(ram,0xa5,sizeof(ram));memcpy(ram,row+32,512);
        switch(op) {
        case 0:result=tsfp_string_initialize(&vm,a,b,width);break;
        case 1:valid=tsfp_string_compare(&vm,a,b,width,c?upper:NULL,&difference);result=(uint32_t)difference;break;
        case 2:result=tsfp_string_update(&vm,a,b,width,TSFP_STRING_COPY,NULL);break;
        case 3:result=tsfp_string_update(&vm,a,b,width,TSFP_STRING_APPEND,NULL);break;
        case 4:result=tsfp_string_update(&vm,a,b,width,TSFP_STRING_UPPER,upper);break;
        case 5:result=tsfp_string_update(&vm,a,b,width,TSFP_STRING_APPEND_RAW,NULL);break;
        case 6:result=tsfp_memory_match(&vm,a,b,c);break;
        case 7:result=tsfp_memory_pattern(&vm,a,b,c);break;
        default:CHECK(0);
        }
        if(result!=expected || valid!=(int)read32(row+24) || memcmp(ram,row+544,512)) {
            fprintf(stderr,"case%u op%u width%u got%08x expected%08x valid%d\n",cases,op,width,result,expected,valid);return 1;
        }
        ++cases;
    }
    CHECK(!ferror(stdin) && cases>10000);printf("PASS: %u independent/native counted-string and memory vectors\n",cases);return 0;
}
