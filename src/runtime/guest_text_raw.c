/* Original raw conversion, prefix publication and size queries over checked RAM. */
#include "guest_text_raw.h"
#include <stdlib.h>
#include <string.h>
#define RAW_INVALID UINT32_C(0xc000000d)
#define RAW_FAILED UINT32_C(0xc0000001)
#define RAW_MEMORY UINT32_C(0xc0000017)
#define RAW_OVERFLOW UINT32_C(0x80000005)
static unsigned char *raw_span(const tsfp_guest_vm *vm,uint32_t address,uint32_t bytes,int write) {
    if(!vm || !vm->resolve || !address || !bytes || address>UINT32_MAX-(bytes-1))return NULL;
    return vm->resolve(vm->host,address,bytes,write);
}
static void raw_count(unsigned char *out,uint32_t value) {
    for(unsigned i=0;i<4;++i)out[i]=(unsigned char)(value>>(i*8));
}
static int raw_overlap(const void *a,uint32_t n,const void *b,uint32_t m) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return n && m && (x<=y?y-x<n:x-y<m);
}
uint32_t tsfp_text_raw_convert(const tsfp_guest_vm *vm,const tsfp_text_codec *codec,tsfp_case_mapper map,
    uint32_t output,uint32_t capacity,uint32_t count,uint32_t source,uint32_t bytes,unsigned operation) {
    unsigned char *input=NULL,*encoded=NULL,*body=NULL,*result=NULL;
    uint32_t needed=0,written=0,used,status=0,direction;int query,upper;
    if(!codec || !codec->convert || operation>TSFP_RAW_UPPER_ANSI)return RAW_INVALID;
    direction=operation<=TSFP_RAW_UNICODE_SIZE?TSFP_TEXT_TO_UNICODE:TSFP_TEXT_TO_ANSI;
    query=operation==TSFP_RAW_UNICODE_SIZE || operation==TSFP_RAW_ANSI_SIZE;upper=operation==TSFP_RAW_UPPER_ANSI;
    if((query && !count) || (upper && !map))return RAW_INVALID;
    if(count){result=raw_span(vm,count,4,1);if(!result)return RAW_INVALID;}
    if(direction==TSFP_TEXT_TO_ANSI)bytes&=~UINT32_C(1);
    if(bytes) {
        const unsigned char *in=raw_span(vm,source,bytes,0);if(!in)return RAW_INVALID;
        input=malloc(bytes);if(!input)return RAW_MEMORY;memcpy(input,in,bytes);
        if(upper)for(uint32_t i=0;i<bytes;i+=2) {
            uint32_t value=(uint32_t)input[i]|((uint32_t)input[i+1]<<8);value=map(value,2,1);
            input[i]=(unsigned char)value;input[i+1]=(unsigned char)(value>>8);
        }
    }
    if(!codec->convert(codec->host,direction,input,bytes,NULL,0,&needed) ||
       (direction==TSFP_TEXT_TO_UNICODE && needed%2)){status=RAW_FAILED;goto done;}
    if(query){raw_count(result,needed);goto done;}
    used=needed<capacity?needed:capacity;
    if(direction==TSFP_TEXT_TO_UNICODE)used&=~UINT32_C(1);
    else if(used<needed) {
        if(!codec->ansi_prefix){status=RAW_INVALID;goto done;}
        used=codec->ansi_prefix(codec->host,input,bytes,capacity);
        if(used==UINT32_MAX || used>needed || used>capacity){status=RAW_FAILED;goto done;}
    }
    if(used) {
        body=raw_span(vm,output,used,1);
        if(!body || (result && raw_overlap(body,used,result,4))){status=RAW_INVALID;goto done;}
        encoded=malloc(needed);if(!encoded){status=RAW_MEMORY;goto done;}
        if(!codec->convert(codec->host,direction,input,bytes,encoded,needed,&written) || written!=needed){status=RAW_FAILED;goto done;}
        memcpy(body,encoded,used);
    }
    if(result)raw_count(result,used);
    if(upper && used<needed)status=RAW_OVERFLOW;
 done:
    free(encoded);free(input);return status;
}
