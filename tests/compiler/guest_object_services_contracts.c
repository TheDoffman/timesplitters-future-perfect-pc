#include "guest_object_services.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"contract line %d: %s\n",__LINE__,#c);exit(1); } } while(0)

typedef struct fixture {
    tsfp_guest_objects objects;
    tsfp_guest_handle_record handles[4];
    tsfp_guest_object_record records[4];
    tsfp_guest_object_services service;
    unsigned char memory[1024];
    unsigned creates,discards,frees,closes,refreshes,probes;
    uint32_t next_body,reenter_token,close_during_create;
    uint64_t thread_key,process_key;
    int fail_create,fail_refresh;
} fixture;
static uint32_t readword(const void *data) {
    const unsigned char *p=(const unsigned char *)data;
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void *resolve(void *context,uint32_t address,uint32_t bytes) {
    fixture *f=(fixture *)context;++f->probes;
    if(address<16 || address>sizeof(f->memory) || bytes>sizeof(f->memory)-address) return NULL;
    return f->memory+address;
}
static int close_host(void *context,uintptr_t host) {
    fixture *f=(fixture *)context;CHECK(host);++f->closes;return 1;
}
static int free_body(void *context,uint32_t body,uint32_t kind,uint32_t flags) {
    fixture *f=(fixture *)context;CHECK(body>=128 && body<sizeof(f->memory));
    CHECK(kind>TSFP_KIND_UNKNOWN && kind<TSFP_KIND_COUNT && !flags);++f->frees;return 1;
}
static void discard(void *context,uint32_t body) {
    fixture *f=(fixture *)context;CHECK(body>=128 && body<sizeof(f->memory));++f->discards;
}
static uint64_t current(void *context,uint32_t handle) {
    fixture *f=(fixture *)context;
    CHECK(handle==UINT32_MAX || handle==UINT32_MAX-1);
    return handle==UINT32_MAX?f->process_key:f->thread_key;
}
static uint32_t create(void *context,const tsfp_guest_object_lease *lease) {
    fixture *f=(fixture *)context;uint32_t body,token;
    CHECK(lease->owner==&f->objects && lease->host);++f->creates;
    if(f->fail_create) return 0;
    body=f->next_body;f->next_body+=64;CHECK(f->next_body<=sizeof(f->memory));
    memset(f->memory+body,0,64);
    if(f->close_during_create) {
        token=f->close_during_create;f->close_during_create=0;
        CHECK(tsfp_guest_object_close(&f->service,token)==0 && f->closes==0);
    }
    if(f->reenter_token) {
        token=f->reenter_token;f->reenter_token=0;
        CHECK(tsfp_guest_object_reference(&f->service,token,0,24)==0);
    }
    return body;
}
static int refresh(void *context,const tsfp_guest_object_lease *lease,uint32_t body) {
    fixture *f=(fixture *)context;
    CHECK(lease->kind==TSFP_KIND_THREAD || lease->kind==TSFP_KIND_EVENT_MANUAL);
    CHECK(lease->host && body>=128);++f->refreshes;
    return !f->fail_refresh;
}
static void init(fixture *f,size_t capacity) {
    unsigned i;
    memset(f,0,sizeof(*f));memset(f->memory,0xa5,sizeof(f->memory));f->next_body=128;
    CHECK(tsfp_guest_objects_init(&f->objects,f->handles,f->records,capacity,close_host,free_body,f));
    f->service.objects=&f->objects;f->service.context=f;
    f->service.resolve_output=resolve;f->service.current_key=current;
    f->service.create_body=create;f->service.discard_body=discard;f->service.refresh_signal=refresh;
    for(i=1;i<TSFP_KIND_COUNT;++i) f->service.type_id[i]=0x1000+i*4;
    f->service.type_id[TSFP_KIND_EVENT_AUTO]=f->service.type_id[TSFP_KIND_EVENT_MANUAL];
}
static uint32_t adopt(fixture *f,uint32_t kind) {
    uint32_t handle=0;CHECK(tsfp_guest_objects_adopt(&f->objects,1+kind,kind,0,&handle));return handle;
}
static void abi_and_lifetime(void) {
    fixture f;uint32_t handle,body;unsigned i;init(&f,4);handle=adopt(&f,TSFP_KIND_EVENT_MANUAL);
    CHECK(tsfp_guest_object_reference(&f.service,handle,0,0)==TSFP_OBJECT_BAD_ADDRESS);
    CHECK(tsfp_guest_object_reference(&f.service,handle,0,UINT32_MAX-2)==TSFP_OBJECT_BAD_ADDRESS);
    CHECK(tsfp_guest_object_reference(&f.service,handle,0,1022)==TSFP_OBJECT_BAD_ADDRESS);
    CHECK(f.creates==0 && f.records[0].refs==0 && f.records[0].leases==0);
    CHECK(tsfp_guest_object_reference(&f.service,handle,f.service.type_id[TSFP_KIND_THREAD],17)==TSFP_OBJECT_WRONG_TYPE);
    CHECK(f.creates==0 && f.memory[17]==0xa5);
    CHECK(tsfp_guest_object_reference(&f.service,handle,f.service.type_id[TSFP_KIND_EVENT_MANUAL],17)==0);
    body=readword(f.memory+17);CHECK(body==128 && f.creates==1 && f.refreshes==1);
    CHECK(f.memory[16]==0xa5 && f.memory[21]==0xa5);
    CHECK(tsfp_guest_object_reference_body(&f.service,body));
    CHECK(!tsfp_guest_object_reference_body(&f.service,body+1));
    CHECK(tsfp_guest_object_close(&f.service,handle)==0 && f.closes==0);
    CHECK(tsfp_guest_object_reference(&f.service,handle,0,17)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_object_close(&f.service,handle)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body));CHECK(f.closes==0);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body));CHECK(f.closes==1 && f.frees==1);
    CHECK(!tsfp_guest_object_dereference_body(&f.service,body));
    CHECK(!tsfp_guest_object_reference_body(&f.service,body));
    for(i=1;i<16;++i) CHECK(f.memory[i]==0xa5);
}
static void errors_and_publication(void) {
    fixture f;uint32_t handle,body;init(&f,4);handle=adopt(&f,TSFP_KIND_THREAD);
    f.fail_create=1;CHECK(tsfp_guest_object_reference(&f.service,handle,0,17)==TSFP_OBJECT_NO_MEMORY);
    CHECK(f.records[0].refs==0 && f.records[0].leases==0 && f.memory[17]==0xa5);
    f.fail_create=0;f.fail_refresh=1;
    CHECK(tsfp_guest_object_reference(&f.service,handle,0,17)==TSFP_OBJECT_HOST_FAILURE);
    CHECK(f.records[0].refs==0 && f.records[0].leases==0 && f.memory[17]==0xa5);
    f.fail_refresh=0;CHECK(tsfp_guest_object_reference(&f.service,handle,0,17)==0);
    CHECK(f.creates==2);body=readword(f.memory+17);
    CHECK(tsfp_guest_object_close(&f.service,handle)==0);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body));CHECK(f.closes==1 && f.frees==1);

    init(&f,4);handle=adopt(&f,TSFP_KIND_EVENT_AUTO);f.reenter_token=handle;
    CHECK(tsfp_guest_object_reference(&f.service,handle,0,17)==0);
    body=readword(f.memory+17);
    CHECK(body==readword(f.memory+24) && body==192);
    CHECK(f.creates==2 && f.discards==1 && f.refreshes==0 && f.records[0].refs==2);
    CHECK(tsfp_guest_object_close(&f.service,handle)==0);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body));CHECK(!f.closes);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body));CHECK(f.closes==1 && f.frees==1);

    init(&f,4);handle=adopt(&f,TSFP_KIND_EVENT_MANUAL);f.close_during_create=handle;
    CHECK(tsfp_guest_object_reference(&f.service,handle,0,17)==0);body=readword(f.memory+17);
    CHECK(f.closes==0 && f.records[0].handles==0 && f.records[0].refs==1 && f.refreshes==1);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body));CHECK(f.closes==1 && f.frees==1);
}
static void pseudo_identity(void) {
    fixture f;uint32_t a,b,body_a,body_b,duplicate;uint64_t stale;init(&f,4);
    a=adopt(&f,TSFP_KIND_THREAD);b=adopt(&f,TSFP_KIND_THREAD);
    CHECK(tsfp_guest_objects_reference(&f.objects,a,&f.thread_key));stale=f.thread_key;
    CHECK(tsfp_guest_object_reference(&f.service,UINT32_MAX-1,0,17)==0);body_a=readword(f.memory+17);
    CHECK(tsfp_guest_object_reference(&f.service,a,0,21)==0);CHECK(readword(f.memory+21)==body_a);
    CHECK(tsfp_guest_object_duplicate(&f.service,UINT32_MAX-1,3,25)==0);duplicate=readword(f.memory+25);
    CHECK(duplicate!=a && duplicate!=b);
    CHECK(tsfp_guest_object_close(&f.service,UINT32_MAX-1)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_objects_dereference(&f.objects,f.thread_key));
    CHECK(tsfp_guest_objects_reference(&f.objects,b,&f.thread_key));
    CHECK(tsfp_guest_object_reference(&f.service,UINT32_MAX-1,0,29)==0);body_b=readword(f.memory+29);
    CHECK(body_a!=body_b);
    CHECK(tsfp_guest_object_reference(&f.service,UINT32_MAX,0,33)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_object_reference(&f.service,0xbeef0001,0,33)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_object_reference(&f.service,0,0,33)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_object_close(&f.service,a)==0);CHECK(tsfp_guest_object_close(&f.service,duplicate)==0);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body_a));CHECK(f.closes==0);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body_a));CHECK(f.closes==1);
    CHECK(!tsfp_guest_objects_open_key(&f.objects,stale,&duplicate));
    CHECK(tsfp_guest_object_close(&f.service,b)==0);
    CHECK(tsfp_guest_object_dereference_body(&f.service,body_b));CHECK(f.closes==1);
    CHECK(tsfp_guest_objects_dereference(&f.objects,f.thread_key));CHECK(f.closes==2 && f.frees==2);
}
static void duplicates_and_capacity(void) {
    fixture f;uint32_t a,b;init(&f,1);a=adopt(&f,TSFP_KIND_FILE);
    CHECK(tsfp_guest_object_duplicate(&f.service,a,1,1023)==TSFP_OBJECT_BAD_ADDRESS);
    CHECK(f.handles[0].used==1);
    CHECK(tsfp_guest_object_duplicate(&f.service,a,4,17)==TSFP_OBJECT_BAD_PARAMETER);
    CHECK(tsfp_guest_object_duplicate(&f.service,a,2,17)==TSFP_OBJECT_NO_MEMORY);
    CHECK(f.memory[17]==0xa5 && f.handles[0].used==1);
    CHECK(tsfp_guest_object_duplicate(&f.service,a,3,17)==0);b=readword(f.memory+17);CHECK(a!=b);
    CHECK(tsfp_guest_object_duplicate(&f.service,a,0,21)==TSFP_OBJECT_BAD_HANDLE);
    CHECK(tsfp_guest_object_close(&f.service,b)==0 && f.closes==1);
    init(&f,1);f.handles[0].generation=1023;a=adopt(&f,TSFP_KIND_FILE);
    CHECK(tsfp_guest_object_duplicate(&f.service,a,1,17)==TSFP_OBJECT_NO_MEMORY);
    CHECK(f.closes==1 && f.handles[0].exhausted && f.memory[17]==0xa5);
    CHECK(tsfp_guest_object_close(&f.service,a)==TSFP_OBJECT_BAD_HANDLE);
}
int main(void) {
    abi_and_lifetime();errors_and_publication();pseudo_identity();duplicates_and_capacity();
    puts("PASS: guest object ABI, type checks, pseudo identity, publication and ownership");return 0;
}
