/* Original guest-width counted-string implementation; no native pointer records. */
#include "guest_strings.h"
#include <string.h>
#define STRING_INVALID UINT32_C(0xc000000d)
#define STRING_SMALL UINT32_C(0xc0000023)
typedef struct string_record {uint32_t length,maximum,buffer;} string_record;
static uint32_t string_read(const unsigned char *p,unsigned bytes) {
    uint32_t value=0;unsigned i;for(i=0;i<bytes;++i)value|=(uint32_t)p[i]<<(8*i);return value;
}
static void string_write(unsigned char *p,unsigned bytes,uint32_t value) {
    unsigned i;for(i=0;i<bytes;++i)p[i]=(unsigned char)(value>>(8*i));
}
static void *string_span(const tsfp_guest_vm *vm,uint32_t address,uint32_t bytes,int writable) {
    if(!vm || !vm->resolve || !address || !bytes || address>UINT32_MAX-(bytes-1))return NULL;
    return vm->resolve(vm->host,address,bytes,writable);
}
static int string_record_read(const tsfp_guest_vm *vm,uint32_t address,string_record *record) {
    const unsigned char *p=string_span(vm,address,8,0);if(!p)return 0;
    record->length=string_read(p,2);record->maximum=string_read(p+2,2);record->buffer=string_read(p+4,4);return 1;
}
/* Limit is a whole-unit byte length. Initializers clamp without scanning beyond
 * the largest representable descriptor; raw append requires a real terminator. */
static int string_measure(const tsfp_guest_vm *vm,uint32_t address,unsigned width,uint32_t limit,int require_nul,uint32_t *length) {
    uint32_t offset=0;const unsigned char *p;
    for(;;) {
        if(offset==limit && !require_nul){*length=offset;return 1;}
        if(address>UINT32_MAX-offset)return 0;
        p=string_span(vm,address+offset,width,0);if(!p)return 0;
        if(!string_read(p,width)){*length=offset;return 1;}
        if(offset==limit)return -1;
        offset+=width;
    }
}
uint32_t tsfp_string_initialize(const tsfp_guest_vm *vm,uint32_t destination,uint32_t source,unsigned width) {
    unsigned char *out,result[8]={0};uint32_t length=0;
    if(width!=1 && width!=2)return STRING_INVALID;
    out=string_span(vm,destination,8,1);if(!out)return STRING_INVALID;
    if(source) {
        uint32_t limit=width==1?65534:65532;
        if(string_measure(vm,source,width,limit,0,&length)!=1)return STRING_INVALID;
        string_write(result,2,length);string_write(result+2,2,length+width);string_write(result+4,4,source);
    }
    memcpy(out,result,8);return 0;
}
int tsfp_string_compare(const tsfp_guest_vm *vm,uint32_t first,uint32_t second,unsigned width,tsfp_string_case map,int32_t *result) {
    string_record a,b;const unsigned char *pa=NULL,*pb=NULL;uint32_t i,common;int32_t difference;
    if(!result || (width!=1 && width!=2) || !string_record_read(vm,first,&a) || !string_record_read(vm,second,&b))return 0;
    if(a.length%width || b.length%width)return 0;
    if(a.length && !(pa=string_span(vm,a.buffer,a.length,0)))return 0;
    if(b.length && !(pb=string_span(vm,b.buffer,b.length,0)))return 0;
    common=a.length<b.length?a.length:b.length;difference=(int32_t)(a.length/width)-(int32_t)(b.length/width);
    for(i=0;i<common;i+=width) {
        uint32_t x=string_read(pa+i,width),y=string_read(pb+i,width);
        if(map){x=map(x,width);y=map(y,width);}
        if(x!=y){difference=(int32_t)x-(int32_t)y;break;}
    }
    *result=difference;return 1;
}
static int string_output_overlap(const void *a,size_t an,const void *b,size_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return an && bn && (x<=y?y-x<an:x-y<bn);
}
uint32_t tsfp_string_update(const tsfp_guest_vm *vm,uint32_t destination,uint32_t source,unsigned width,enum tsfp_string_operation operation,tsfp_string_case map) {
    string_record d,s={0};unsigned char *header,*body=NULL;const unsigned char *input=NULL;
    uint32_t copy,offset=0,total,write_bytes,i;int measured;
    if((width!=1 && width!=2) || (unsigned)operation>(unsigned)TSFP_STRING_APPEND_RAW ||
       !string_record_read(vm,destination,&d))return STRING_INVALID;
    header=string_span(vm,destination,8,1);if(!header)return STRING_INVALID;
    if(operation==TSFP_STRING_APPEND_RAW) {
        if(width!=2)return STRING_INVALID;
        if(!source)return 0;
        measured=string_measure(vm,source,2,65534,1,&s.length);
        if(measured!=1)return measured<0?STRING_SMALL:STRING_INVALID;
        s.buffer=source;
    } else if(source) {
        if(!string_record_read(vm,source,&s))return STRING_INVALID;
    } else {
        if(operation==TSFP_STRING_APPEND)return STRING_INVALID;
        string_write(header,2,0);return 0;
    }
    if(s.length%width)return STRING_INVALID;
    if(operation==TSFP_STRING_APPEND || operation==TSFP_STRING_APPEND_RAW) {
        if(d.length%width || d.length>d.maximum)return STRING_INVALID;
        offset=d.length;
        if(s.length>d.maximum-offset)return STRING_SMALL;
        copy=s.length;
        if(!copy && operation==TSFP_STRING_APPEND)return 0;
    } else {
        copy=s.length<d.maximum?s.length:d.maximum;copy-=copy%width;
    }
    if(operation==TSFP_STRING_UPPER && !map)return STRING_INVALID;
    if(s.length && !(input=string_span(vm,s.buffer,s.length,0)))return STRING_INVALID;
    total=offset+copy;write_bytes=copy;
    /* Native Unicode routines add a terminator only when two spare bytes exist. */
    if(width==2 && d.maximum-total>=2)write_bytes+=2;
    if(write_bytes) {
        if(!d.buffer || d.buffer>UINT32_MAX-offset)return STRING_INVALID;
        body=string_span(vm,d.buffer+offset,write_bytes,1);if(!body)return STRING_INVALID;
        if(string_output_overlap(header,8,body,write_bytes))return STRING_INVALID;
    }
    if(copy)memmove(body,input,copy);
    if(operation==TSFP_STRING_UPPER)for(i=0;i<copy;i+=width)string_write(body+i,width,map(string_read(body+i,width),width));
    if(write_bytes>copy)string_write(body+copy,2,0);
    string_write(header,2,total);return 0;
}
uint32_t tsfp_memory_match(const tsfp_guest_vm *vm,uint32_t first,uint32_t second,uint32_t bytes) {
    const unsigned char *a,*b;uint32_t i;
    if(!bytes)return 0;
    a=string_span(vm,first,bytes,0);b=string_span(vm,second,bytes,0);if(!a || !b)return 0;
    for(i=0;i<bytes;++i){if(a[i]!=b[i])break;}return i;
}
uint32_t tsfp_memory_pattern(const tsfp_guest_vm *vm,uint32_t address,uint32_t bytes,uint32_t pattern) {
    const unsigned char *p;uint32_t i;bytes&=~UINT32_C(3);if(!bytes)return 0;
    p=string_span(vm,address,bytes,0);if(!p)return 0;
    for(i=0;i<bytes;i+=4){if(string_read(p+i,4)!=pattern)break;}return i;
}
