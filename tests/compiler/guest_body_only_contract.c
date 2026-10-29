#include "guest_objects.h"
#include "guest_object_names.h"
#include "guest_allocation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"body owner line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static tsfp_guest_objects objects;static tsfp_guest_handle_record handles[2];
static tsfp_guest_object_record records[2];static tsfp_allocations allocations;
static tsfp_allocation_record heap_records[8];static unsigned char ram[65536];
static unsigned host_closes,body_attempts,body_frees,name_live;static int body_result;
static int close_host(void *ctx,uintptr_t host) {CHECK(ctx==&objects && host==0x777);++host_closes;return 1;}
static int free_body(void *ctx,uint32_t body,uint32_t kind,uint32_t flags) {
    uint64_t other=0xfeed;
    CHECK(ctx==&objects && kind==0x42 && flags==0x10 && !objects.lock);++body_attempts;
    CHECK(!tsfp_guest_objects_reference_dispatcher(&objects,body,&other) && other==0xfeed);
    if(body_result!=1)return body_result;
    CHECK(tsfp_heap_release(&allocations,body));++body_frees;return 1;
}
static uint32_t body(void) {
    uint32_t address=tsfp_heap_allocate(&allocations,96,16);CHECK(address);ram[address]=0xa7;return address;
}
static void init(void) {
    tsfp_allocation_config config;memset(&config,0,sizeof(config));
    memset(&allocations,0,sizeof(allocations));config.ram=ram;config.ram_size=sizeof(ram);
    config.heap_begin=0x1000;config.reserve_begin=0x8000;
    CHECK(tsfp_allocations_init(&allocations,&config,heap_records,8,NULL,0));
    CHECK(tsfp_guest_objects_init(&objects,handles,records,2,close_host,free_body,&objects));
    host_closes=body_attempts=body_frees=name_live=0;body_result=1;
}
static void *name_allocate(void *ctx,size_t bytes) {void *p;CHECK(ctx==&objects);p=malloc(bytes);CHECK(p);++name_live;return p;}
static void name_free(void *ctx,void *p) {CHECK(ctx==&objects && name_live);--name_live;free(p);}
static void lifetime(void) {
    uint32_t address,token;uint64_t key,other;tsfp_guest_object_lease lease={0};init();address=body();
    CHECK(tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&key));
    CHECK(ram[address]==0xa7 && !host_closes && !body_frees);
    CHECK(tsfp_guest_objects_acquire_key(&objects,key,&lease));
    CHECK(!lease.host && lease.dispatcher==address && lease.kind==0x42 && lease.flags==0x10);
    CHECK(tsfp_guest_objects_open_key(&objects,key,&token));
    CHECK(tsfp_guest_objects_reference_dispatcher(&objects,address,&other) && other==key);
    CHECK(tsfp_guest_objects_dereference(&objects,key));CHECK(tsfp_guest_objects_close(&objects,token));
    CHECK(tsfp_guest_objects_dereference(&objects,other));CHECK(!body_frees && !host_closes);
    CHECK(tsfp_guest_objects_release(&lease));CHECK(body_frees==1 && !host_closes && !allocations.heap.count);
    CHECK(!tsfp_guest_objects_retain(&objects,key));CHECK(!tsfp_guest_objects_reference_dispatcher(&objects,address,&other));
    address=body();CHECK(tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&other));
    CHECK(other!=key && !tsfp_guest_objects_retain(&objects,key));CHECK(tsfp_guest_objects_dereference(&objects,other));
}
static void failures_and_mixed(void) {
    uint32_t address,other,token;uint64_t key,out=0xfeed;tsfp_guest_objects bare;
    tsfp_guest_handle_record bare_handles[1];tsfp_guest_object_record bare_records[1];
    init();address=body();
    CHECK(tsfp_guest_objects_init(&bare,bare_handles,bare_records,1,close_host,NULL,&objects));
    CHECK(!tsfp_guest_objects_adopt_body(&bare,address,0x42,0x10,&out) && out==0xfeed);
    CHECK(!tsfp_guest_objects_adopt_body(&objects,0,0x42,0x10,&out) && out==0xfeed);
    CHECK(tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&key));
    CHECK(!tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&out) && out==0xfeed);
    CHECK(tsfp_guest_objects_adopt(&objects,0x777,0x99,0,&token));other=body();
    CHECK(!tsfp_guest_objects_adopt_body(&objects,other,0x42,0x10,&out) && out==0xfeed);
    CHECK(tsfp_allocation_remaining(&allocations,other)==96 && !body_frees);
    CHECK(tsfp_heap_release(&allocations,other));CHECK(tsfp_guest_objects_close(&objects,token));
    CHECK(host_closes==1 && !body_frees);CHECK(tsfp_guest_objects_dereference(&objects,key));
    CHECK(body_frees==1 && host_closes==1 && !allocations.heap.count);
}
static void namespace_lifetime(void) {
    const char name[]="\\Device\\TestDevice";tsfp_guest_names names;tsfp_guest_object_lease lease={0};
    uint32_t address;uint64_t key,found;init();address=body();
    CHECK(tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&key));
    CHECK(tsfp_guest_names_init(&names,&objects,2,name_allocate,name_free,&objects));
    CHECK(tsfp_guest_names_bind(&names,name,sizeof(name)-1,0,key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_objects_dereference(&objects,key));
    CHECK(tsfp_guest_names_lookup(&names,name,sizeof(name)-1,0,&found)==TSFP_NAME_OK && found==key);
    CHECK(tsfp_guest_names_remove(&names,name,sizeof(name)-1,0,key)==TSFP_NAME_OK);
    CHECK(!body_frees && !host_closes && !name_live);
    CHECK(tsfp_guest_objects_acquire_key(&objects,found,&lease));
    CHECK(tsfp_guest_objects_dereference(&objects,found));CHECK(!body_frees);
    CHECK(tsfp_guest_objects_release(&lease));CHECK(body_frees==1 && !host_closes);
    CHECK(tsfp_guest_names_stop(&names)==0 && !allocations.heap.count);
}
static void cleanup_states(void) {
    uint32_t address;uint64_t key,out=0xfeed;init();address=body();
    CHECK(tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&key));body_result=0;
    CHECK(tsfp_guest_objects_dereference(&objects,key));CHECK(body_attempts==1 && !body_frees && !host_closes);
    CHECK(!tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&out) && out==0xfeed);
    body_result=1;CHECK(!tsfp_guest_objects_collect(&objects));CHECK(body_attempts==2 && body_frees==1);
    init();address=body();CHECK(tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&key));body_result=-1;
    CHECK(tsfp_guest_objects_dereference(&objects,key));CHECK(body_attempts==1);
    CHECK(tsfp_guest_objects_collect(&objects)==1 && body_attempts==1 && !host_closes);
    CHECK(!tsfp_guest_objects_adopt_body(&objects,address,0x42,0x10,&out) && out==0xfeed);
    CHECK(tsfp_allocation_remaining(&allocations,address)==96); /* Accounted uncertainty. */
}
#ifdef _WIN32
static HANDLE start_gate;static uint32_t shared_body;static uint64_t race_keys[4];
static int race_ok[4];static unsigned ids[4]={0,1,2,3};
static DWORD WINAPI publish_body(void *context) {
    unsigned id=*(unsigned *)context;
    CHECK(WaitForSingleObject(start_gate,5000)==WAIT_OBJECT_0);
    race_keys[id]=0xfeed;
    race_ok[id]=tsfp_guest_objects_adopt_body(&objects,shared_body,0x42,0x10,&race_keys[id]);
    return 0;
}
static void adoption_races(void) {
    unsigned trial,i,winners;uint64_t key=0;HANDLE threads[4];
    for(trial=0;trial<50;++trial) {
        init();shared_body=body();start_gate=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(start_gate);
        for(i=0;i<4;++i) {threads[i]=CreateThread(NULL,0,publish_body,&ids[i],0,NULL);CHECK(threads[i]);}
        CHECK(SetEvent(start_gate));CHECK(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);
        winners=0;
        for(i=0;i<4;++i) {
            if(race_ok[i]) {++winners;key=race_keys[i];}else CHECK(race_keys[i]==0xfeed);
            CHECK(CloseHandle(threads[i]));
        }
        CHECK(CloseHandle(start_gate));CHECK(winners==1 && !host_closes && !body_frees);
        CHECK(tsfp_guest_objects_dereference(&objects,key));CHECK(body_frees==1 && !allocations.heap.count);
    }
}
#endif
int main(void) {
    lifetime();failures_and_mixed();namespace_lifetime();
#ifdef _WIN32
    adoption_races();
#endif
    cleanup_states();
    puts("PASS: guest-only object adoption, namespace/lease lifetime and cleanup without a native handle");return 0;
}
