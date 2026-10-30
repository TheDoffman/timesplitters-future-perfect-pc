/* Original byte-oriented numeric grammar, rendering and checked publication. */
#include "guest_numbers.h"
#include <string.h>
#define NUMBER_BAD UINT32_C(0xc000000d)
#define NUMBER_OVERFLOW UINT32_C(0x80000005)
typedef struct number_reader {
    const tsfp_guest_vm *vm;
    const unsigned char *body;
    uint32_t address,units;
    int counted;
} number_reader;
static void *number_span(const tsfp_guest_vm *vm,uint64_t address,uint32_t bytes,int write) {
    if(!vm || !vm->resolve || !address || !bytes || address>UINT32_MAX || bytes-1>UINT32_MAX-address)return NULL;
    return vm->resolve(vm->host,(uint32_t)address,bytes,write);
}
static uint32_t number_read(const unsigned char *p,unsigned bytes) {
    uint32_t value=0;
    for(unsigned i=0;i<bytes;++i)value|=(uint32_t)p[i]<<(i*8);
    return value;
}
static void number_write(unsigned char *p,unsigned bytes,uint32_t value) {
    for(unsigned i=0;i<bytes;++i)p[i]=(unsigned char)(value>>(i*8));
}
static int number_base(uint32_t base) {return base==0 || base==2 || base==8 || base==10 || base==16;}
static int number_unit(const number_reader *r,uint64_t index,uint32_t *value) {
    const unsigned char *p;
    if(r->counted) {
        if(index>=r->units){*value=0;return 1;}
        *value=number_read(r->body+(size_t)index*2,2);return 1;
    }
    p=number_span(r->vm,(uint64_t)r->address+index,1,0);
    if(!p)return 0;
    *value=*p;return 1;
}
static int number_parse(const number_reader *r,uint32_t base,uint32_t *result) {
    uint64_t at=0;uint32_t c,next,value=0;int negative=0;
    if(!number_unit(r,at,&c))return 0;
    while(c && c<=32){++at;if(!number_unit(r,at,&c))return 0;}
    if(c=='+' || c=='-') {negative=c=='-';++at;if(!number_unit(r,at,&c))return 0;}
    if(!base) {
        base=10;
        if(c=='0') {
            if(!number_unit(r,at+1,&next))return 0;
            if(next=='x')base=16;else if(next=='o')base=8;else if(next=='b')base=2;
            if(base!=10){at+=2;if(!number_unit(r,at,&c))return 0;}
        }
    }
    for(;;) {
        uint32_t digit;
        if(c>='0' && c<='9')digit=c-'0';
        else if(c>='a' && c<='f')digit=c-'a'+10;
        else if(c>='A' && c<='F')digit=c-'A'+10;
        else break;
        if(digit>=base)break;
        value=value*base+digit;
        ++at;if(!number_unit(r,at,&c))return 0;
    }
    *result=negative?0u-value:value;return 1;
}
uint32_t tsfp_number_from_ansi(const tsfp_guest_vm *vm,uint32_t source,uint32_t base,uint32_t destination) {
    unsigned char *out;uint32_t value;number_reader r={0};
    if(!source || !number_base(base))return NUMBER_BAD;
    out=number_span(vm,destination,4,1);if(!out)return NUMBER_BAD;
    r.vm=vm;r.address=source;
    if(!number_parse(&r,base,&value))return NUMBER_BAD;
    number_write(out,4,value);return 0;
}
uint32_t tsfp_number_from_unicode(const tsfp_guest_vm *vm,uint32_t source,uint32_t base,uint32_t destination) {
    const unsigned char *record;unsigned char *out;uint32_t length,address,value=0;number_reader r={0};
    record=number_span(vm,source,8,0);out=number_span(vm,destination,4,1);
    if(!record || !out)return NUMBER_BAD;
    length=number_read(record,2);address=number_read(record+4,4);
    if(length&1)return NUMBER_BAD;
    r.vm=vm;r.counted=1;r.units=length/2;
    if(length){r.body=number_span(vm,address,length,0);if(!r.body)return NUMBER_BAD;}
    if(!number_base(base)){number_write(out,4,0);return NUMBER_BAD;}
    if(!number_parse(&r,base,&value))return NUMBER_BAD;
    number_write(out,4,value);return length?0:NUMBER_BAD;
}
static unsigned number_digits(uint32_t value,uint32_t base,unsigned char *output) {
    static const unsigned char alphabet[]="0123456789ABCDEF";
    unsigned char reverse[32];unsigned count=0;
    if(!base)base=10;
    do{reverse[count++]=alphabet[value%base];value/=base;}while(value);
    for(unsigned i=0;i<count;++i)output[i]=reverse[count-1-i];
    return count;
}
uint32_t tsfp_number_to_ansi(const tsfp_guest_vm *vm,uint32_t value,uint32_t base,uint32_t capacity,uint32_t destination) {
    unsigned char text[33],*out;unsigned count,bytes;
    if(!destination || !number_base(base))return NUMBER_BAD;
    count=number_digits(value,base,text);
    if(capacity<count)return NUMBER_OVERFLOW;
    bytes=count+(capacity>count?1u:0u);text[count]=0;
    out=number_span(vm,destination,bytes,1);if(!out)return NUMBER_BAD;
    memcpy(out,text,bytes);return 0;
}
static int number_overlap(const void *a,size_t an,const void *b,size_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y?y-x<an:x-y<bn;
}
uint32_t tsfp_number_to_unicode(const tsfp_guest_vm *vm,uint32_t value,uint32_t base,uint32_t destination) {
    unsigned char text[32],*record,*out;unsigned count,bytes;uint32_t capacity,address;
    if(!number_base(base))return NUMBER_BAD;
    record=number_span(vm,destination,8,1);if(!record)return NUMBER_BAD;
    capacity=number_read(record+2,2);address=number_read(record+4,4);
    count=number_digits(value,base,text);bytes=(count+1)*2;
    if(capacity<bytes)return NUMBER_OVERFLOW;
    out=number_span(vm,address,bytes,1);if(!out || number_overlap(record,8,out,bytes))return NUMBER_BAD;
    for(unsigned i=0;i<count;++i)number_write(out+i*2,2,text[i]);
    number_write(out+count*2,2,0);number_write(record,2,count*2);return 0;
}
