#include "guest_name_service.h"
#include <string.h>
static const void *input(tsfp_guest_name_service *s,uint32_t address,uint32_t bytes) {
    if(!address || !bytes || address>UINT32_MAX-(bytes-1)) return NULL;
    return s->resolve_input(s->context,address,bytes);
}
static uint32_t word(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
uint32_t tsfp_guest_name_reference(tsfp_guest_name_service *s,uint32_t descriptor,
    uint32_t attributes,uint32_t expected_type,uint32_t parse_context,uint32_t output) {
    const void *source,*name;unsigned char data[8];
    uint32_t length,capacity,address,result;uint64_t key=0;int status;
    if(!s || !s->names || !s->references || !s->references->objects ||
       s->names->objects!=s->references->objects || !s->resolve_input ||
       !s->references->resolve_output) return TSFP_OBJECT_BAD_PARAMETER;
    if((attributes&~UINT32_C(0x40)) || parse_context) return TSFP_OBJECT_BAD_PARAMETER;
    if(!output || output>UINT32_MAX-3 ||
       !s->references->resolve_output(s->references->context,output,4)) return TSFP_OBJECT_BAD_ADDRESS;
    source=input(s,descriptor,8);if(!source) return TSFP_OBJECT_BAD_ADDRESS;
    memcpy(data,source,8);
    length=(uint32_t)data[0]|((uint32_t)data[1]<<8);
    capacity=(uint32_t)data[2]|((uint32_t)data[3]<<8);address=word(data+4);
    if(!length || length>capacity) return TSFP_NAME_BAD_DESCRIPTOR;
    name=input(s,address,length);if(!name) return TSFP_OBJECT_BAD_ADDRESS;
    status=tsfp_guest_names_lookup(s->names,name,length,
        (attributes&0x40)?TSFP_NAME_IGNORE_ASCII_CASE:0,&key);
    if(status==TSFP_NAME_MISSING || status==TSFP_NAME_CLOSED) return TSFP_NAME_NOT_FOUND;
    if(status!=TSFP_NAME_OK) return TSFP_OBJECT_BAD_HANDLE;
    result=tsfp_guest_object_reference_key(s->references,key,expected_type,output);
    (void)tsfp_guest_objects_dereference(s->references->objects,key);
    return result;
}
