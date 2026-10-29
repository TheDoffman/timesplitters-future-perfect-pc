#include "guest_name_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"name ABI line %d: %s\n",__LINE__,#c);exit(1); } } while(0)
static tsfp_guest_objects objects,other;
static tsfp_guest_handle_record handles[4],other_handles[4];
static tsfp_guest_object_record records[4],other_records[4];
static tsfp_guest_names names;
static tsfp_guest_object_services references;
static tsfp_guest_name_service service;
static uint32_t guest_args[5],g_eax,argument_reads;
static uint32_t stack_arg(unsigned index) {
    CHECK(index==argument_reads && index<5);++argument_reads;return guest_args[index];
}
#define STACK_ARG(index) stack_arg(index)
#define bridge_name_service service
#include "guest_kernel_names.inc"
#undef bridge_name_service
#undef STACK_ARG
static uint32_t call_name(tsfp_guest_name_service *s,uint32_t descriptor,
    uint32_t attrs,uint32_t type,uint32_t parse,uint32_t output) {
    CHECK(s==&service);
    guest_args[0]=descriptor;guest_args[1]=attrs;guest_args[2]=type;
    guest_args[3]=parse;guest_args[4]=output;argument_reads=0;g_eax=0xdeadbeef;
    bridge_ObReferenceObjectByName();CHECK(argument_reads==5);return g_eax;
}
static unsigned char memory[512];
static uint32_t descriptor_end,name_end,output_end;
static unsigned reads,name_allocations,name_frees,host_closes,body_allocations,body_frees;
static unsigned fail_body,fail_signal,remove_during_body,roots_released;
static uint64_t root_key;
static uint32_t handle;
static void put32(unsigned char *p,uint32_t v) {
    unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));
}
static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static const void *read_span(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==memory);++reads;
    if(address>=0x11 && address<=descriptor_end && bytes<=descriptor_end-address) return memory+address;
    if(address>=0x41 && address<=name_end && bytes<=name_end-address) return memory+address;
    return NULL;
}
static void *write_span(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==memory);
    return address>=0x81 && address<=output_end && bytes<=output_end-address?memory+address:NULL;
}
static int close_host(void *ctx,uintptr_t host) { CHECK(ctx==&objects && host==42);++host_closes;return 1; }
static int free_body(void *ctx,uint32_t address,uint32_t kind,uint32_t flags) {
    CHECK(ctx==&objects && address==0x180 && kind==TSFP_KIND_EVENT_MANUAL && flags==0);++body_frees;return 1;
}
static void *allocate_name(void *ctx,size_t bytes) {
    void *p=malloc(bytes);CHECK(ctx==&names && p);++name_allocations;return p;
}
static void dispose_name(void *ctx,void *p) { CHECK(ctx==&names && p);++name_frees;free(p); }
static void release_roots(void) {
    CHECK(!roots_released);roots_released=1;
    CHECK(tsfp_guest_objects_close(&objects,handle));CHECK(tsfp_guest_objects_dereference(&objects,root_key));
}
static uint32_t create_body(void *ctx,const tsfp_guest_object_lease *lease) {
    CHECK(ctx==memory && lease->host==42 && lease->kind==TSFP_KIND_EVENT_MANUAL);
    if(fail_body) return 0;
    if(remove_during_body) {
        remove_during_body=0;
        CHECK(tsfp_guest_names_remove(&names,"Resource",8,0,root_key)==TSFP_NAME_OK);
        release_roots();CHECK(!host_closes);
    }
    ++body_allocations;return 0x180;
}
static void discard_body(void *ctx,uint32_t body) { CHECK(ctx==memory && body==0x180);++body_frees; }
static int refresh(void *ctx,const tsfp_guest_object_lease *lease,uint32_t body) {
    CHECK(ctx==memory && lease->host==42 && body==0x180);return !fail_signal;
}
static void initialize(void) {
    memset(memory,0xa7,sizeof(memory));
    reads=name_allocations=name_frees=host_closes=body_allocations=body_frees=0;
    fail_body=fail_signal=remove_during_body=roots_released=0;
    descriptor_end=0x19;name_end=0x49;output_end=0x85;
    CHECK(tsfp_guest_objects_init(&objects,handles,records,4,close_host,free_body,&objects));
    CHECK(tsfp_guest_objects_init(&other,other_handles,other_records,4,close_host,free_body,&objects));
    CHECK(tsfp_guest_names_init(&names,&objects,4,allocate_name,dispose_name,&names));
    CHECK(tsfp_guest_objects_adopt(&objects,42,TSFP_KIND_EVENT_MANUAL,0,&handle));
    CHECK(tsfp_guest_objects_reference(&objects,handle,&root_key));
    CHECK(tsfp_guest_names_bind(&names,"Resource",8,0,root_key)==TSFP_NAME_OK);
    memset(&references,0,sizeof(references));references.objects=&objects;references.context=memory;
    references.resolve_output=write_span;references.create_body=create_body;references.discard_body=discard_body;
    references.refresh_signal=refresh;references.type_id[TSFP_KIND_EVENT_MANUAL]=0x1234;
    service.names=&names;service.references=&references;service.context=memory;service.resolve_input=read_span;
    memory[0x11]=8;memory[0x12]=0;memory[0x13]=64;memory[0x14]=0;put32(memory+0x15,0x41);
    memcpy(memory+0x41,"Resource",8);
}
static void finish(void) {
    tsfp_guest_names_stop(&names);if(!roots_released) release_roots();
    CHECK(host_closes==1 && name_allocations==name_frees && body_allocations==body_frees);
}
static void failure(uint32_t descriptor,uint32_t attrs,uint32_t type,uint32_t parse,uint32_t output,uint32_t wanted) {
    unsigned char before[sizeof(memory)];uint32_t refs=records[0].refs;
    memcpy(before,memory,sizeof(memory));
    CHECK(call_name(&service,descriptor,attrs,type,parse,output)==wanted);
    CHECK(!memcmp(before,memory,sizeof(memory)) && records[0].refs==refs && !records[0].leases);
}
static void success(unsigned reentrant) {
    unsigned char before[sizeof(memory)];size_t i;
    initialize();remove_during_body=reentrant;
    memcpy(memory+0x41,"rEsOuRcE",8);memcpy(before,memory,sizeof(memory));
    CHECK(call_name(&service,0x11,0x40,0x1234,0,0x81)==TSFP_OBJECT_SUCCESS);
    CHECK(get32(memory+0x81)==0x180 && body_allocations==1);
    for(i=0;i<sizeof(memory);++i)if(i<0x81 || i>=0x85)CHECK(memory[i]==before[i]);
    if(!reentrant) { CHECK(tsfp_guest_names_remove(&names,"Resource",8,0,root_key)==TSFP_NAME_OK);release_roots(); }
    CHECK(!host_closes && !body_frees);
    CHECK(tsfp_guest_object_dereference_body(&references,0x180));
    CHECK(host_closes==1 && body_frees==1);finish();
}
static void input_failures(void) {
    initialize();
    failure(0,0,0,0,0x81,TSFP_OBJECT_BAD_ADDRESS);
    failure(UINT32_MAX-3,0,0,0,0x81,TSFP_OBJECT_BAD_ADDRESS);
    descriptor_end=0x15;failure(0x11,0,0,0,0x81,TSFP_OBJECT_BAD_ADDRESS);descriptor_end=0x19;
    memory[0x11]=0;failure(0x11,0,0,0,0x81,TSFP_NAME_BAD_DESCRIPTOR);memory[0x11]=8;
    memory[0x13]=7;failure(0x11,0,0,0,0x81,TSFP_NAME_BAD_DESCRIPTOR);memory[0x13]=64;
    put32(memory+0x15,0);failure(0x11,0,0,0,0x81,TSFP_OBJECT_BAD_ADDRESS);
    put32(memory+0x15,UINT32_MAX-2);failure(0x11,0,0,0,0x81,TSFP_OBJECT_BAD_ADDRESS);
    put32(memory+0x15,0x41);name_end=0x42;failure(0x11,0,0,0,0x81,TSFP_OBJECT_BAD_ADDRESS);name_end=0x49;
    reads=0;output_end=0x84;failure(0x11,0,0,0,0x81,TSFP_OBJECT_BAD_ADDRESS);CHECK(!reads);output_end=0x85;
    failure(0x11,0,0,0,0,TSFP_OBJECT_BAD_ADDRESS);
    failure(0x11,0,0,0,UINT32_MAX-1,TSFP_OBJECT_BAD_ADDRESS);
    failure(0x11,0x80,0,0,0x81,TSFP_OBJECT_BAD_PARAMETER);
    failure(0x11,0,0,1,0x81,TSFP_OBJECT_BAD_PARAMETER);
    names.objects=&other;failure(0x11,0,0,0,0x81,TSFP_OBJECT_BAD_PARAMETER);names.objects=&objects;
    service.resolve_input=NULL;failure(0x11,0,0,0,0x81,TSFP_OBJECT_BAD_PARAMETER);service.resolve_input=read_span;
    memcpy(memory+0x41,"resource",8);failure(0x11,0,0,0,0x81,TSFP_NAME_NOT_FOUND);
    failure(0x11,0x40,0x9999,0,0x81,TSFP_OBJECT_WRONG_TYPE);
    fail_body=1;failure(0x11,0x40,0x1234,0,0x81,TSFP_OBJECT_NO_MEMORY);fail_body=0;
    fail_signal=1;failure(0x11,0x40,0x1234,0,0x81,TSFP_OBJECT_HOST_FAILURE);
    CHECK(body_allocations==1 && !body_frees);finish();
}
static void missing_and_key(void) {
    initialize();CHECK(tsfp_guest_names_stop(&names)==1);
    failure(0x11,0,0,0,0x81,TSFP_NAME_NOT_FOUND);
    CHECK(tsfp_guest_object_reference_key(&references,0,0,0x81)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_object_reference_key(&references,root_key,0x1234,0x81)==TSFP_OBJECT_SUCCESS);
    CHECK(tsfp_guest_object_dereference_body(&references,get32(memory+0x81)));finish();
}
int main(void) {
    success(0);success(1);input_failures();missing_and_key();
    puts("PASS: guest counted-name spans, type/attribute checks and reference rollback");return 0;
}
