/* Original bounded creation and transactional counted case transformation. */
#include "guest_string_transform.h"
#include <stdlib.h>
#include <string.h>
#define TRANSFORM_INVALID UINT32_C(0xc000000d)
#define TRANSFORM_MEMORY UINT32_C(0xc0000017)
#define TRANSFORM_SMALL UINT32_C(0x80000005)
static unsigned char *transform_span(const tsfp_guest_vm *vm,uint32_t address,uint32_t count,int write) {
    if(!vm || !vm->resolve || !address || !count || address>UINT32_MAX-(count-1))return NULL;
    return vm->resolve(vm->host,address,count,write);
}
static uint32_t transform_read(const unsigned char *p,unsigned count) {
    uint32_t value=0;for(unsigned i=0;i<count;++i)value|=(uint32_t)p[i]<<(i*8);return value;
}
static void transform_write(unsigned char *p,unsigned count,uint32_t value) {
    for(unsigned i=0;i<count;++i)p[i]=(unsigned char)(value>>(i*8));
}
static int transform_overlap(const void *a,uint32_t n,const void *b,uint32_t m) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return n && m && (x<=y?y-x<n:x-y<m);
}
static uint32_t transform_publish(const tsfp_guest_vm *vm,unsigned char *record,const unsigned char old[8],
    const unsigned char *output,uint32_t length,uint32_t bytes,uint8_t allocate) {
    uint32_t buffer=transform_read(old+4,4),owned=0;unsigned char *body=NULL;
    if(allocate) {
        owned=tsfp_heap_allocate(vm->allocations,bytes,16);
        if(!owned)return TRANSFORM_MEMORY;
        buffer=owned;
    }
    if(bytes) {
        body=transform_span(vm,buffer,bytes,1);
        if(!body || transform_overlap(record,8,body,bytes)) {
            if(owned){memcpy(record,old,8);(void)tsfp_heap_release(vm->allocations,owned);}
            return TRANSFORM_INVALID;
        }
        memcpy(body,output,bytes);
    }
    transform_write(record,2,length);
    if(allocate){transform_write(record+2,2,bytes);transform_write(record+4,4,buffer);}
    return 0;
}
int tsfp_string_create(const tsfp_guest_vm *vm,uint32_t destination,uint32_t source) {
    unsigned char *record=transform_span(vm,destination,8,1),old[8],*copy;const unsigned char *input;
    uint32_t length=0,status;
    if(!record || !source)return 0;
    memcpy(old,record,8);
    for(;;) {
        if(source>UINT32_MAX-length)return 0;
        input=transform_span(vm,source+length,2,0);if(!input)return 0;
        if(!transform_read(input,2))break;
        if(length==65532)return 0;
        length+=2;
    }
    input=transform_span(vm,source,length+2,0);if(!input)return 0;
    copy=malloc(length+2);if(!copy)return 0;memcpy(copy,input,length+2);
    status=transform_publish(vm,record,old,copy,length,length+2,1);free(copy);return status==0;
}
uint32_t tsfp_string_transform(const tsfp_guest_vm *vm,tsfp_case_mapper map,uint32_t destination,uint32_t source,uint8_t allocate,int upper) {
    unsigned char *record=transform_span(vm,destination,8,1),old[8],*copy=NULL;
    const unsigned char *src=transform_span(vm,source,8,0),*input;uint32_t length,buffer,status;
    if(!record || !src || !map)return TRANSFORM_INVALID;
    memcpy(old,record,8);length=transform_read(src,2);buffer=transform_read(src+4,4);
    if(length%2)return TRANSFORM_INVALID;
    if(!allocate && length>transform_read(old+2,2))return TRANSFORM_SMALL;
    if(length) {
        input=transform_span(vm,buffer,length,0);if(!input)return TRANSFORM_INVALID;
        copy=malloc(length);if(!copy)return TRANSFORM_MEMORY;memcpy(copy,input,length);
        for(uint32_t i=0;i<length;i+=2)transform_write(copy+i,2,map(transform_read(copy+i,2),2,upper));
    }
    status=transform_publish(vm,record,old,copy,length,length,allocate);free(copy);return status;
}
