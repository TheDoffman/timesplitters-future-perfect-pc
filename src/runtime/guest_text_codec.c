/* Original checked counted conversions and guest heap ownership composition. */
#include "guest_text_codec.h"
#include <stdlib.h>
#include <string.h>
#define TEXT_INVALID UINT32_C(0xc000000d)
#define TEXT_TOO_LONG UINT32_C(0xc00000f0)
#define TEXT_MEMORY UINT32_C(0xc0000017)
#define TEXT_OVERFLOW UINT32_C(0x80000005)
#define TEXT_FAILED UINT32_C(0xc0000001)
static unsigned char *text_span(const tsfp_guest_vm *vm,uint32_t address,uint32_t bytes,int write) {
    if(!vm || !vm->resolve || !address || !bytes || address>UINT32_MAX-(bytes-1))return NULL;
    return vm->resolve(vm->host,address,bytes,write);
}
static uint32_t text_read(const unsigned char *p,unsigned bytes) {
    uint32_t value=0;for(unsigned i=0;i<bytes;++i)value|=(uint32_t)p[i]<<(i*8);return value;
}
static void text_write(unsigned char *p,unsigned bytes,uint32_t value) {
    for(unsigned i=0;i<bytes;++i)p[i]=(unsigned char)(value>>(i*8));
}
static int text_overlap(const void *a,uint32_t n,const void *b,uint32_t m) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return n && m && (x<=y?y-x<n:x-y<m);
}
uint32_t tsfp_text_string_convert(const tsfp_guest_vm *vm,const tsfp_text_codec *codec,uint32_t destination,uint32_t source,uint8_t allocate,unsigned direction) {
    unsigned char *dst,*src,*input=NULL,*output=NULL,*body;unsigned char old[8];
    uint32_t length,source_buffer,capacity,buffer,needed=0,written=0,used,terminator,status=0,owned=0,actual;
    if(!codec || !codec->convert || direction>TSFP_TEXT_TO_ANSI)return TEXT_INVALID;
    dst=text_span(vm,destination,8,1);src=text_span(vm,source,8,0);
    if(!dst || !src)return TEXT_INVALID;
    memcpy(old,dst,8);length=text_read(src,2);source_buffer=text_read(src+4,4);
    capacity=text_read(dst+2,2);buffer=text_read(dst+4,4);terminator=direction==TSFP_TEXT_TO_UNICODE?2u:1u;
    if(direction==TSFP_TEXT_TO_ANSI && length%2)return TEXT_INVALID;
    if(length) {
        unsigned char *in=text_span(vm,source_buffer,length,0);if(!in)return TEXT_INVALID;
        input=malloc(length);if(!input)return TEXT_MEMORY;memcpy(input,in,length);
    }
    if(!codec->convert(codec->host,direction,input,length,NULL,0,&needed)){status=TEXT_FAILED;goto done;}
    if(needed>UINT16_MAX-terminator){status=TEXT_TOO_LONG;goto done;}
    used=needed;
    if(!allocate && needed+terminator>capacity) {
        if(direction==TSFP_TEXT_TO_UNICODE || !capacity){status=TEXT_OVERFLOW;goto done;}
        if(!codec->ansi_prefix){status=TEXT_INVALID;goto done;}
        used=codec->ansi_prefix(codec->host,input,length,capacity-1);
        if(used==UINT32_MAX || used>capacity-1 || used>needed){status=TEXT_FAILED;goto done;}
        status=TEXT_OVERFLOW;
    }
    output=malloc(needed+terminator);if(!output){status=TEXT_MEMORY;goto done;}
    if(!codec->convert(codec->host,direction,input,length,output,needed,&written) || written!=needed){status=TEXT_FAILED;goto done;}
    memset(output+used,0,terminator);
    actual=used+terminator;
    if(allocate) {
        owned=tsfp_heap_allocate(vm->allocations,needed+terminator,16);
        if(!owned){status=TEXT_MEMORY;goto done;}
        buffer=owned;capacity=needed+terminator;
    }
    body=text_span(vm,buffer,actual,1);
    if(!body || text_overlap(dst,8,body,actual)){status=TEXT_INVALID;goto done;}
    memcpy(body,output,actual);text_write(dst,2,used);
    if(allocate){text_write(dst+2,2,capacity);text_write(dst+4,4,buffer);owned=0;}
 done:
    if(owned){if(memcmp(dst,old,8))memcpy(dst,old,8);(void)tsfp_heap_release(vm->allocations,owned);}
    free(output);free(input);return status;
}
int tsfp_text_string_free(const tsfp_guest_vm *vm,uint32_t descriptor) {
    unsigned char *record,*body;uint32_t buffer,owned;
    if(!descriptor)return 1;
    record=text_span(vm,descriptor,8,1);if(!record)return 0;
    buffer=text_read(record+4,4);if(!buffer)return 1;
    if(!vm->allocations)return 0;
    owned=tsfp_allocation_remaining(vm->allocations,buffer);if(!owned)return 0;
    body=text_span(vm,buffer,owned,-1);if(!body || text_overlap(record,8,body,owned))return 0;
    if(!tsfp_heap_release(vm->allocations,buffer))return 0;
    memset(record,0,8);return 1;
}
