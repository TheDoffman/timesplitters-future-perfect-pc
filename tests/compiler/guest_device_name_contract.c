/* Typed namespace integration with synthetic body storage, not DEVICE_OBJECT
 * serialization or a title-driver fixture. Uses real project heap/ownership. */
#include "guest_name_service.h"
#include "guest_allocation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"device name line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned char ram[65536];static tsfp_allocations memory;
static tsfp_allocation_record heap_records[4];static tsfp_guest_objects objects;
static tsfp_guest_handle_record handles[2];static tsfp_guest_object_record records[2];
static unsigned freed,names_live;static uint32_t body;
static int close_host(void *ctx,uintptr_t host) {(void)ctx;(void)host;CHECK(0);return -1;}
static int free_body(void *ctx,uint32_t address,uint32_t kind,uint32_t flags) {
    CHECK(ctx==&objects && address==body && kind==TSFP_KIND_DEVICE && !flags && !freed);
    CHECK(tsfp_heap_release(&memory,address));++freed;return 1;
}
static void *name_allocate(void *ctx,size_t bytes) {void *p;CHECK(ctx==&objects);p=malloc(bytes);CHECK(p);++names_live;return p;}
static void name_free(void *ctx,void *p) {CHECK(ctx==&objects && names_live);--names_live;free(p);}
static void *mapping(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==ram);if(!address || address>=sizeof(ram) || bytes>sizeof(ram)-address)return NULL;
    return ram+address;
}
static const void *input(void *ctx,uint32_t address,uint32_t bytes) {return mapping(ctx,address,bytes);}
static void word(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static uint32_t value(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void) {
    const char name[]="\\Device\\TestDevice";tsfp_allocation_config config;
    tsfp_guest_names names;tsfp_guest_object_services refs;tsfp_guest_name_service service;uint64_t key;
    memset(&config,0,sizeof(config));config.ram=ram;config.ram_size=sizeof(ram);
    config.heap_begin=0x1000;config.reserve_begin=0x8000;
    CHECK(tsfp_allocations_init(&memory,&config,heap_records,4,NULL,0));
    CHECK(tsfp_guest_objects_init(&objects,handles,records,2,close_host,free_body,&objects));
    body=tsfp_heap_allocate(&memory,96,16);CHECK(body==0x1000);
    CHECK(tsfp_guest_objects_adopt_body(&objects,body,TSFP_KIND_DEVICE,0,&key));
    CHECK(tsfp_guest_names_init(&names,&objects,2,name_allocate,name_free,&objects));
    CHECK(tsfp_guest_names_bind(&names,name,sizeof(name)-1,0,key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_objects_dereference(&objects,key)); /* Namespace is now the owner. */
    memset(&refs,0,sizeof(refs));refs.objects=&objects;refs.context=ram;refs.resolve_output=mapping;
    refs.type_id[TSFP_KIND_DEVICE]=0xf0000040;refs.type_id[TSFP_KIND_EVENT_MANUAL]=0xf0000080;
    service.names=&names;service.references=&refs;service.context=ram;service.resolve_input=input;
    memcpy(ram+0x101,name,sizeof(name)-1);ram[0x301]=(unsigned char)(sizeof(name)-1);
    ram[0x303]=(unsigned char)(sizeof(name)-1);word(ram+0x305,0x101);
    memset(ram+0x400,0xcc,6);
    CHECK(tsfp_guest_name_reference(&service,0x301,0,refs.type_id[TSFP_KIND_EVENT_MANUAL],0,0x401)==TSFP_OBJECT_WRONG_TYPE);
    CHECK(value(ram+0x401)==0xcccccccc && !freed);
    CHECK(tsfp_guest_name_reference(&service,0x301,0x40,refs.type_id[TSFP_KIND_DEVICE],0,0x401)==TSFP_OBJECT_SUCCESS);
    CHECK(value(ram+0x401)==body && ram[0x400]==0xcc && ram[0x405]==0xcc);
    CHECK(tsfp_guest_names_remove(&names,name,sizeof(name)-1,0,key)==TSFP_NAME_OK);
    CHECK(!names_live && !freed && tsfp_allocation_remaining(&memory,body)==96);
    memset(ram+0x400,0xcc,6);
    CHECK(tsfp_guest_name_reference(&service,0x301,0,refs.type_id[TSFP_KIND_DEVICE],0,0x401)==TSFP_NAME_NOT_FOUND);
    CHECK(value(ram+0x401)==0xcccccccc);
    CHECK(tsfp_guest_object_dereference_body(&refs,body));CHECK(freed==1 && !memory.heap.count);
    CHECK(tsfp_guest_names_stop(&names)==0);
    puts("PASS: typed device namespace returns owned guest body with no native handle");return 0;
}
