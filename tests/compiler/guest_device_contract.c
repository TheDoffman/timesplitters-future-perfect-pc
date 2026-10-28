/* Original transaction/lifetime fixture. Default marker mode isolates owner
 * policy; TSFP_REAL_DEVICE_BODY composes the explicit guest body constructor. */
#include "guest_device_factory.h"
#include "guest_allocation.h"
#include "guest_device_body.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"factory line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static tsfp_device_factory factory;static tsfp_guest_objects objects;
static tsfp_guest_handle_record handles[8];static tsfp_guest_object_record records[8];
static tsfp_guest_names names;static tsfp_allocations memory;static tsfp_allocation_record heap_records[16];
static unsigned char ram[65536];static int depth,initialized,meta_fail,alloc_fail,map_fail,init_fail,name_fail,stop_during_init,free_result;
static unsigned metadata_live,name_live,allocations,frees,marks,initializations;
#ifdef TSFP_REAL_DEVICE_BODY
#define BODY_TYPE 3
#define DELETE_OFFSET 0x1f
#define DELETE_MARKER 1
#else
#define BODY_TYPE 0xd0
#define DELETE_OFFSET 1
#define DELETE_MARKER 0xd1
#endif
static void lock(void *ctx) {CHECK(ctx==&factory && !depth);++depth;}
static void unlock(void *ctx) {CHECK(ctx==&factory && depth==1);--depth;}
static void counts(size_t live,size_t retained,size_t uncertain) {
    size_t a=99,b=99,c=99;tsfp_device_factory_counts(&factory,&a,&b,&c);
    CHECK(a==live && b==retained && c==uncertain && a==metadata_live);
}
static void *metadata_allocate(void *ctx,size_t bytes) {
    void *p;CHECK(ctx==&factory && !depth);if(meta_fail)return NULL;p=malloc(bytes);CHECK(p);++metadata_live;return p;
}
static void metadata_free(void *ctx,void *p) {CHECK(ctx==&factory && !depth && metadata_live);--metadata_live;free(p);}
static void *name_allocate(void *ctx,size_t bytes) {
    void *p;CHECK(ctx==&factory && !depth);if(name_fail)return NULL;p=malloc(bytes);CHECK(p);++name_live;return p;
}
static void name_free(void *ctx,void *p) {CHECK(ctx==&factory && !depth && name_live);--name_live;free(p);}
static uint32_t allocate(void *ctx,uint32_t bytes) {
    CHECK(ctx==&factory && !depth);++allocations;return alloc_fail?0:tsfp_heap_allocate(&memory,bytes,16);
}
static int deallocate(void *ctx,uint32_t body) {
    size_t a,b,c;CHECK(ctx==&factory && !depth);++frees;
    tsfp_device_factory_counts(&factory,&a,&b,&c);CHECK(a>=1);
    if(free_result!=1)return free_result;
    CHECK(tsfp_heap_release(&memory,body));return 1;
}
static void *output(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==&factory && !depth);
    if(!address || address>=sizeof(ram) || bytes>sizeof(ram)-address || (map_fail && address>=0x2000))return NULL;
    return ram+address;
}
static const void *input(void *ctx,uint32_t address,uint32_t bytes) {return output(ctx,address,bytes);}
static int expected_exclusive=1;
static int initialize_body(void *ctx,uint32_t body,void *p,uint32_t bytes,uint32_t driver,uint32_t ext,uint32_t type,int named,int exclusive) {
    uint32_t i;size_t a,b,c;CHECK(ctx==&factory && !depth && p==ram+body);
    CHECK(bytes==72+ext && driver==0x201 && type==0x3a && exclusive==expected_exclusive && (named==0 || named==1));
    for(i=0;i<bytes;++i)CHECK(!((unsigned char *)p)[i]);
    tsfp_device_factory_counts(&factory,&a,&b,&c);CHECK(a>=1);++initializations;
    if(stop_during_init)tsfp_device_factory_stop(&factory);
    if(init_fail)return 0;
#ifdef TSFP_REAL_DEVICE_BODY
    return tsfp_device_body_initialize(p,bytes,body,driver,ext,type,named,exclusive);
#else
    ((unsigned char *)p)[0]=BODY_TYPE;return 1;
#endif
}
static void mark_deleted(void *ctx,uint32_t body,void *p,uint32_t bytes) {
    CHECK(ctx==&factory && !depth && p==ram+body && bytes>=72 && ((unsigned char *)p)[0]==BODY_TYPE);
#ifdef TSFP_REAL_DEVICE_BODY
    CHECK(tsfp_device_body_mark_deleted(p,bytes));
#else
    ((unsigned char *)p)[DELETE_OFFSET]=DELETE_MARKER;
#endif
    ++marks;
    CHECK(!tsfp_device_factory_delete(&factory,body)); /* Creator was already claimed. */
}
static int close_host(void *ctx,uintptr_t host) {(void)ctx;(void)host;CHECK(0);return -1;}
static int free_body(void *ctx,uint32_t body,uint32_t kind,uint32_t flags) {
    CHECK(ctx==&factory && !depth);return tsfp_device_factory_dispose(&factory,body,kind,flags);
}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static uint32_t value(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static const char device_name[]="\\Device\\TestUnit";
static void init(void) {
    tsfp_device_factory_ops ops;tsfp_allocation_config config;
    if(initialized) {counts(0,0,0);CHECK(!name_live && !memory.heap.count);CHECK(!tsfp_guest_names_stop(&names));}
    memset(&memory,0,sizeof(memory));memset(ram,0xa7,sizeof(ram));memset(&config,0,sizeof(config));
    config.ram=ram;config.ram_size=sizeof(ram);config.heap_begin=0x2000;config.reserve_begin=0x8000;
    CHECK(tsfp_allocations_init(&memory,&config,heap_records,16,NULL,0));
    CHECK(tsfp_guest_objects_init(&objects,handles,records,8,close_host,free_body,&factory));
    CHECK(tsfp_guest_names_init(&names,&objects,8,name_allocate,name_free,&factory));
    memset(&ops,0,sizeof(ops));ops.context=&factory;ops.lock=lock;ops.unlock=unlock;
    ops.metadata_allocate=metadata_allocate;ops.metadata_free=metadata_free;ops.allocate=allocate;ops.deallocate=deallocate;
    ops.input=input;ops.output=output;ops.initialize=initialize_body;ops.mark_deleted=mark_deleted;
    CHECK(tsfp_device_factory_init(&factory,&objects,&names,72,2,&ops));
    meta_fail=alloc_fail=map_fail=init_fail=name_fail=stop_during_init=0;free_result=1;
    metadata_live=name_live=allocations=frees=marks=initializations=0;initialized=1;
    memcpy(ram+0x101,device_name,sizeof(device_name)-1);
    ram[0x301]=(unsigned char)(sizeof(device_name)-1);ram[0x302]=0;
    ram[0x303]=ram[0x301];ram[0x304]=0;put(ram+0x305,0x101);
    memset(ram+0x800,0xcc,6);counts(0,0,0);
}
static int create(int named,uint32_t ext) {return tsfp_device_factory_create(&factory,0x201,ext,named?0x301:0,0x3a,7,0x801);}
static void unchanged(void) {CHECK(value(ram+0x801)==0xcccccccc && ram[0x800]==0xcc && ram[0x805]==0xcc);}
static void failures(void) {
    init();CHECK(tsfp_device_factory_create(&factory,0x201,UINT32_MAX,0,0x3a,1,0x801)==TSFP_DEVICE_BAD_INPUT);
    CHECK(tsfp_device_factory_create(&factory,0,0,0,0x3a,1,0x801)==TSFP_DEVICE_BAD_ADDRESS);
    CHECK(tsfp_device_factory_create(&factory,0x201,0,0,0x3a,1,UINT32_MAX)==TSFP_DEVICE_BAD_ADDRESS);
    ram[0x303]=0;CHECK(create(1,0)==TSFP_DEVICE_BAD_INPUT);ram[0x303]=ram[0x301];
    CHECK(!allocations);unchanged();meta_fail=1;CHECK(create(1,0)==TSFP_DEVICE_NO_MEMORY);counts(0,0,0);
    meta_fail=0;alloc_fail=1;CHECK(create(1,0)==TSFP_DEVICE_NO_MEMORY);counts(0,0,0);CHECK(!frees);
    alloc_fail=0;map_fail=1;CHECK(create(1,0)==TSFP_DEVICE_BAD_ADDRESS);counts(0,0,0);CHECK(frees==1);
    map_fail=0;init_fail=1;CHECK(create(1,0)==TSFP_DEVICE_INITIALIZATION);counts(0,0,0);CHECK(frees==2);
    init_fail=0;name_fail=1;CHECK(create(1,0)==TSFP_DEVICE_NO_MEMORY);counts(0,0,0);CHECK(!name_live);unchanged();
}
static void named_lifetime(void) {
    uint32_t body;uint64_t key;tsfp_guest_object_lease lease={0};init();
    CHECK(create(1,0x170)==TSFP_DEVICE_OK);body=value(ram+0x801);CHECK(body==0x2000 && ram[body]==BODY_TYPE);
#ifdef TSFP_REAL_DEVICE_BODY
    CHECK(value(ram+body+0x18)==body+72 && ram[body+0x1c]==0x3a && ram[body+0x1e]==1);
    CHECK(value(ram+body+0x14)==0x1a && ((uint32_t)ram[body+2]|((uint32_t)ram[body+3]<<8))==440);
#endif
    CHECK(ram[0x800]==0xcc && ram[0x805]==0xcc && tsfp_allocation_remaining(&memory,body)==440);
    CHECK(tsfp_guest_names_lookup(&names,device_name,sizeof(device_name)-1,0,&key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_objects_acquire_key(&objects,key,&lease));CHECK(tsfp_guest_objects_dereference(&objects,key));
    CHECK(tsfp_device_factory_delete(&factory,body));CHECK(!tsfp_device_factory_delete(&factory,body));
    CHECK(marks==1 && ram[body+DELETE_OFFSET]==DELETE_MARKER && !name_live && !frees);counts(1,0,0);
    CHECK(tsfp_guest_objects_release(&lease));counts(0,0,0);CHECK(frees==1 && !memory.heap.count);
}
static void collision_rebind_capacity(void) {
    uint32_t a,b;uint64_t key,found;unsigned before;init();CHECK(create(1,0)==TSFP_DEVICE_OK);a=value(ram+0x801);
    memset(ram+0x800,0xcc,6);CHECK(create(1,0)==TSFP_DEVICE_COLLISION);unchanged();counts(1,0,0);CHECK(name_live==1);
    CHECK(tsfp_guest_names_lookup(&names,device_name,sizeof(device_name)-1,0,&key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_objects_dereference(&objects,key));
    CHECK(tsfp_guest_names_remove(&names,device_name,sizeof(device_name)-1,0,key)==TSFP_NAME_OK);
    CHECK(create(1,0)==TSFP_DEVICE_OK);b=value(ram+0x801);CHECK(a!=b);
    CHECK(tsfp_device_factory_delete(&factory,a));
    CHECK(tsfp_guest_names_lookup(&names,device_name,sizeof(device_name)-1,0,&found)==TSFP_NAME_OK && found!=key);
    CHECK(tsfp_guest_objects_dereference(&objects,found));CHECK(tsfp_device_factory_delete(&factory,b));counts(0,0,0);
    CHECK(create(0,0)==TSFP_DEVICE_OK);a=value(ram+0x801);CHECK(create(0,0)==TSFP_DEVICE_OK);b=value(ram+0x801);
    before=allocations;memset(ram+0x800,0xcc,6);CHECK(create(0,0)==TSFP_DEVICE_NO_MEMORY && allocations==before);unchanged();
    CHECK(tsfp_device_factory_delete(&factory,a));CHECK(tsfp_device_factory_delete(&factory,b));counts(0,0,0);
}
static void stopping_and_cleanup(void) {
    uint32_t body;unsigned before;init();stop_during_init=1;CHECK(create(1,0)==TSFP_DEVICE_CLOSED);counts(0,0,0);unchanged();
    CHECK(create(1,0)==TSFP_DEVICE_CLOSED);init();
    map_fail=1;free_result=0;CHECK(create(0,0)==TSFP_DEVICE_BAD_ADDRESS);counts(1,1,0);unchanged();
    free_result=1;CHECK(tsfp_device_factory_retry_one(&factory));counts(0,0,0);
    map_fail=0;CHECK(create(1,0)==TSFP_DEVICE_OK);body=value(ram+0x801);free_result=0;
    CHECK(tsfp_device_factory_delete(&factory,body));counts(1,1,0);CHECK(!tsfp_device_factory_retry_one(&factory));
    free_result=1;CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);
    init();CHECK(create(1,0)==TSFP_DEVICE_OK);CHECK(create(0,0)==TSFP_DEVICE_OK);tsfp_device_factory_stop(&factory);
    counts(0,0,0);CHECK(!name_live && create(0,0)==TSFP_DEVICE_CLOSED);
    init();map_fail=1;free_result=-1;CHECK(create(0,0)==TSFP_DEVICE_BAD_ADDRESS);counts(1,0,1);
    map_fail=0;free_result=1;CHECK(create(1,0)==TSFP_DEVICE_OK);body=value(ram+0x801);free_result=-1;
    CHECK(tsfp_device_factory_delete(&factory,body));counts(2,0,2);before=frees;
    CHECK(!tsfp_device_factory_retry_one(&factory));CHECK(tsfp_guest_objects_collect(&objects)==1 && frees==before);
    CHECK(tsfp_device_factory_dispose(&factory,body,TSFP_KIND_DEVICE,0)==-1 && frees==before);
    tsfp_device_factory_stop(&factory);counts(2,0,2);CHECK(memory.heap.count==2 && !name_live);
}
#ifdef TSFP_DEVICE_BRIDGE
static uint32_t guest_args[6],g_eax;
static unsigned argument_reads[6],argument_limit;
static uint32_t read_argument(unsigned i) {
    CHECK(i<argument_limit);++argument_reads[i];return guest_args[i];
}
#define STACK_ARG(i) read_argument(i)
#define bridge_devices factory
#include "guest_kernel_devices.inc"
static void abi_create(void) {
    unsigned i;argument_limit=6;memset(argument_reads,0,sizeof(argument_reads));
    bridge_IoCreateDevice();
    for(i=0;i<6;++i)CHECK(argument_reads[i]==1);
}
static void abi_delete(uint32_t body) {
    unsigned i;guest_args[0]=body;argument_limit=1;
    memset(argument_reads,0,sizeof(argument_reads));g_eax=0xdeadbeef;
    bridge_IoDeleteDevice();CHECK(!g_eax && argument_reads[0]==1);
    for(i=1;i<6;++i)CHECK(!argument_reads[i]);
}
static void abi_cases(void) {
    uint32_t body;uint64_t key;unsigned prior_frees;tsfp_guest_object_lease lease={0};
    init();guest_args[0]=0x201;guest_args[1]=0x170;guest_args[2]=0x301;
    guest_args[3]=0x3a;guest_args[4]=0x100;guest_args[5]=0x801;
    expected_exclusive=0;abi_create();CHECK(!g_eax);body=value(ram+0x801);
    CHECK(value(ram+body+0x14)==0x18 && value(ram+body+0x18)==body+72);
    CHECK(tsfp_guest_names_lookup(&names,device_name,sizeof(device_name)-1,0,&key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_objects_acquire_key(&objects,key,&lease));
    CHECK(tsfp_guest_objects_dereference(&objects,key));
    memset(ram+0x800,0xcc,6);abi_create();CHECK(g_eax==0xc0000035);unchanged();
    prior_frees=frees;abi_delete(body);CHECK(!name_live && frees==prior_frees);abi_delete(body);
    CHECK(tsfp_guest_objects_release(&lease));counts(0,0,0);CHECK(frees==prior_frees+1);
    guest_args[0]=0x201;guest_args[5]=UINT32_MAX;abi_create();
    CHECK(g_eax==0xc0000005);unchanged();
    guest_args[5]=0x801;meta_fail=1;abi_create();CHECK(g_eax==0xc000009a);unchanged();
    meta_fail=0;guest_args[1]=UINT32_MAX;abi_create();CHECK(g_eax==0xc000000d);unchanged();
    guest_args[1]=0;tsfp_device_factory_stop(&factory);abi_create();CHECK(g_eax==0xc0000056);unchanged();
    CHECK(bridge_device_status(TSFP_DEVICE_INITIALIZATION)==0xc000000d);
    CHECK(bridge_device_status(999)==0xc000000d);
    expected_exclusive=1;
}
#endif
#ifdef TSFP_DEVICE_FINALIZATION
static void deferred_finalization(void) {
    uint32_t body;uint64_t key;unsigned before;tsfp_guest_object_lease lease={0};
    tsfp_device_finalization ticket={0},other={0},sentinel={7,8,9};
    init();factory.ops.defer_finalization=1;
    CHECK(create(1,0x170)==TSFP_DEVICE_OK);body=value(ram+0x801);
    CHECK(tsfp_guest_objects_reference_dispatcher(&objects,body,&key));
    CHECK(tsfp_guest_objects_acquire_key(&objects,key,&lease));
    CHECK(tsfp_device_factory_delete(&factory,body));CHECK(!name_live && marks==1 && !frees);
    CHECK(!tsfp_device_factory_take_finalization(&factory,&sentinel));
    CHECK(sentinel.key==7 && sentinel.body==8 && sentinel.driver==9);
    CHECK(tsfp_guest_objects_dereference(&objects,key));
    CHECK(!tsfp_device_factory_take_finalization(&factory,&ticket));
    CHECK(tsfp_guest_objects_release(&lease));counts(1,0,0);CHECK(!frees);
    CHECK(tsfp_guest_objects_collect(&objects)==1 && !frees);
    CHECK(tsfp_device_factory_take_finalization(&factory,&ticket));
    CHECK(ticket.body==body && ticket.driver==0x201 && ticket.key==key);
    CHECK(!tsfp_device_factory_take_finalization(&factory,&other));
    CHECK(tsfp_guest_objects_collect(&objects)==1 && !frees);
    CHECK(ram[body]==BODY_TYPE && ram[body+DELETE_OFFSET]==DELETE_MARKER);
    other=ticket;other.key^=1;CHECK(!tsfp_device_factory_finish_finalization(&factory,&other,1));
    other=ticket;other.driver^=4;CHECK(!tsfp_device_factory_finish_finalization(&factory,&other,1));
    CHECK(tsfp_device_factory_finish_finalization(&factory,&ticket,1));
    CHECK(!tsfp_device_factory_finish_finalization(&factory,&ticket,1));
    free_result=0;CHECK(tsfp_guest_objects_collect(&objects)==1);counts(1,1,0);
    CHECK(!tsfp_device_factory_take_finalization(&factory,&other));
    free_result=1;CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);
    CHECK(!tsfp_device_factory_finish_finalization(&factory,&ticket,1));
    /* Reused body address cannot make an old generation ticket valid. */
    other=ticket;CHECK(create(0,0)==TSFP_DEVICE_OK);CHECK(value(ram+0x801)==body);
    CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(tsfp_device_factory_take_finalization(&factory,&ticket));
    CHECK(ticket.key!=other.key && ticket.body==other.body);
    CHECK(!tsfp_device_factory_finish_finalization(&factory,&other,1));
    CHECK(tsfp_device_factory_finish_finalization(&factory,&ticket,1));
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);
    /* Failed publication must not invoke a guest driver's deletion callback. */
    init();factory.ops.defer_finalization=1;CHECK(create(1,0)==TSFP_DEVICE_OK);
    body=value(ram+0x801);CHECK(create(1,0)==TSFP_DEVICE_COLLISION);
    CHECK(!tsfp_device_factory_take_finalization(&factory,&other));
    CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(tsfp_device_factory_take_finalization(&factory,&ticket));
    CHECK(tsfp_device_factory_finish_finalization(&factory,&ticket,1));
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);
    /* Simulated nonlocal exit: explicit uncertain finish quarantines storage. */
    init();factory.ops.defer_finalization=1;CHECK(create(0,0)==TSFP_DEVICE_OK);
    body=value(ram+0x801);CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(tsfp_device_factory_take_finalization(&factory,&ticket));before=frees;
    CHECK(tsfp_device_factory_finish_finalization(&factory,&ticket,0));
    CHECK(!tsfp_device_factory_finish_finalization(&factory,&ticket,1));
    CHECK(tsfp_guest_objects_collect(&objects)==1 && frees==before);counts(1,0,1);
    CHECK(!tsfp_device_factory_take_finalization(&factory,&other));
    tsfp_device_factory_stop(&factory);CHECK(frees==before && memory.heap.count==1);
    puts("PASS: deferred finalization preserves storage through callback acknowledgement and uncertain exits");
}
#endif
#if defined(TSFP_DEVICE_CALLBACK_SCOPE) || defined(TSFP_DEVICE_DISPATCH)
#include "guest_device_callback.h"
#include "guest_lifetime.h"
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
#ifndef TSFP_DEVICE_DISPATCH
RECOMP_TLS uint32_t g_ebx,g_edi,g_esi,g_esp,g_ebp,g_seh_ebp;
#endif
static tsfp_guest_object_scopes callback_scopes;
static tsfp_guest_object_scope outer_scope;
static unsigned reached,escaped;
static uint64_t capture_scopes(void) {return tsfp_guest_scope_mark(&callback_scopes);}
static int restore_scopes(uint64_t mark) {return tsfp_guest_scope_unwind(&callback_scopes,mark);}
static void abandoned_callback(void) {
    tsfp_device_callback inner;uint64_t mark;
    CHECK(tsfp_device_callback_begin(&callback_scopes,&inner,&factory));
    CHECK(inner.active && ram[inner.ticket.body]==BODY_TYPE);
    mark=tsfp_guest_scope_mark(&callback_scopes);
    CHECK(!tsfp_guest_scope_unwind(&callback_scopes,mark+1));
    CHECK(inner.active && tsfp_guest_scope_mark(&callback_scopes)==mark);
    ++reached;(void)recomp_guest_longjmp(0x1000,7);++escaped;
}
static void callback_scope_cases(void) {
    tsfp_device_callback frame;uint32_t body;uint64_t outer_mark;
    init();factory.ops.defer_finalization=1;
    CHECK(tsfp_guest_scope_enter(&callback_scopes,&outer_scope,&objects));
    outer_mark=tsfp_guest_scope_mark(&callback_scopes);
    CHECK(!tsfp_device_callback_begin(&callback_scopes,&frame,&factory));
    CHECK(callback_scopes.current==&outer_scope);
    CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);
    CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(tsfp_device_callback_begin(&callback_scopes,&frame,&factory));
    CHECK(!tsfp_device_callback_begin(&callback_scopes,&frame,&factory));
    CHECK(frame.active && !frees);
    CHECK(tsfp_guest_objects_collect(&objects)==1 && !frees);
    CHECK(tsfp_device_callback_end(&callback_scopes,&frame));
    CHECK(!frame.active && callback_scopes.current==&outer_scope);
    CHECK(!tsfp_device_callback_end(&callback_scopes,&frame));
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);
    CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);
    CHECK(tsfp_device_factory_delete(&factory,body));
    g_xbox_mem_offset=(ptrdiff_t)ram;memset(ram+0x1000,0,20);
    CHECK(tsfp_guest_lifetime_hooks(capture_scopes,restore_scopes));
    switch(setjmp(*recomp_setjmp_slot(0x1000))) {
    case 0:abandoned_callback();CHECK(0);break;
    case 7:break;
    default:CHECK(0);
    }
    CHECK(reached==1 && !escaped && callback_scopes.current==&outer_scope);
    CHECK(tsfp_guest_scope_mark(&callback_scopes)==outer_mark);
    CHECK(tsfp_guest_objects_collect(&objects)==1);counts(1,0,1);
    CHECK(frees==1 && memory.heap.count==1 && ram[body]==BODY_TYPE);
    /* Explicit thread exit must quarantine an active callback too. */
    CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);
    CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(tsfp_device_callback_begin(&callback_scopes,&frame,&factory));
    CHECK(tsfp_guest_scope_abort(&callback_scopes));CHECK(!callback_scopes.current);
    CHECK(!frame.active && tsfp_guest_objects_collect(&objects)==2);
    counts(2,0,2);CHECK(frees==1 && memory.heap.count==2);
    CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: normal callback completion and real native guest longjmp finalize or quarantine exactly once");
}
#endif
#ifdef TSFP_DEVICE_DISPATCH
#include "guest_device_dispatch.h"
#if TSFP_DEVICE_DISPATCH >= 4
#include "guest_device_event_scope.h"
static tsfp_device_finalization callback_lock_ticket;
#endif
static uint32_t expected_body;static unsigned driver_calls;static int driver_malformed,driver_jump,driver_map_fail;
static void delete_driver(void) {
    uint32_t stack=g_esp;CHECK(!depth && MEM32(stack)==0 && MEM32(stack+4)==expected_body);
    CHECK(ram[expected_body]==BODY_TYPE && ram[expected_body+DELETE_OFFSET]==DELETE_MARKER);
    CHECK(tsfp_guest_objects_collect(&objects)>=1 && memory.heap.count>=1);
    ++driver_calls;g_eax=0xeeee;g_ebx=0xdddd;
#if TSFP_DEVICE_DISPATCH >= 4
    {
        tsfp_device_event_scope event_scope;
        CHECK(tsfp_device_callback_ticket(&callback_scopes,&factory,expected_body,&callback_lock_ticket));
        CHECK(tsfp_device_event_scope_begin(&callback_scopes,&event_scope,&factory,expected_body+0x34));
        CHECK(tsfp_device_factory_final_pins(&factory,&callback_lock_ticket)==1);
        CHECK(tsfp_device_event_try_wait(&event_scope.event)==1);
        if(driver_jump) {(void)recomp_guest_longjmp(0x1000,7);CHECK(0);}
        CHECK(tsfp_device_event_scope_end(&callback_scopes,&event_scope));
        CHECK(!tsfp_device_factory_final_pins(&factory,&callback_lock_ticket));
    }
#endif
    if(driver_jump) {(void)recomp_guest_longjmp(0x1000,7);CHECK(0);}
    g_esp=stack+(driver_malformed?4:8);
}
static tsfp_guest_entry_t lookup_driver(void *ctx,uint32_t routine) {
    CHECK(ctx==&factory && !depth);return routine==0x4444?delete_driver:NULL;
}
static const void *driver_input(void *ctx,uint32_t address,uint32_t bytes) {
    if(driver_map_fail && address==0x201)return NULL;
    return input(ctx,address,bytes);
}
static void prepare_dispatch(void) {
    init();factory.ops.defer_finalization=1;factory.ops.input=driver_input;
    g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x1800;g_eax=0x1234;g_ebx=0x5678;
    driver_calls=0;driver_malformed=driver_jump=driver_map_fail=0;
    CHECK(tsfp_guest_scope_enter(&callback_scopes,&outer_scope,&objects));
    CHECK(tsfp_guest_lifetime_hooks(capture_scopes,restore_scopes));
    put(ram+0x205,0x4444);
}
static void retire_for_dispatch(void) {
    CHECK(create(0,0)==TSFP_DEVICE_OK);expected_body=value(ram+0x801);
#if TSFP_DEVICE_DISPATCH >= 4
    {
        tsfp_device_event_scope ordinary;
        CHECK(tsfp_device_event_scope_begin(&callback_scopes,&ordinary,&factory,expected_body+0x34));
        CHECK(tsfp_device_event_try_wait(&ordinary.event)==1 && tsfp_device_event_set(&ordinary.event)==0);
        CHECK(tsfp_device_event_scope_end(&callback_scopes,&ordinary));
        CHECK(!tsfp_device_event_scope_begin(&callback_scopes,&ordinary,&factory,expected_body+0x35));
        CHECK(callback_scopes.current==&outer_scope);
    }
#endif
    CHECK(tsfp_device_factory_delete(&factory,expected_body));
}
static void dispatch_cases(void) {
    tsfp_guest_callback_ops ops={&factory,lookup_driver,output};
    prepare_dispatch();
    CHECK(tsfp_device_dispatch_one(&factory,&callback_scopes,&ops)==TSFP_DEVICE_DISPATCH_NOT_STARTED);
    retire_for_dispatch();CHECK(tsfp_device_dispatch_one(&factory,&callback_scopes,&ops)==TSFP_DEVICE_DISPATCH_COMPLETE);
    CHECK(driver_calls==1 && g_eax==0x1234 && g_ebx==0x5678 && g_esp==0x1800);
    CHECK(callback_scopes.current==&outer_scope && !tsfp_guest_objects_collect(&objects));counts(0,0,0);
    /* No callback still acknowledges disposal; no routine invocation occurs. */
    put(ram+0x205,0);retire_for_dispatch();
    CHECK(tsfp_device_dispatch_one(&factory,&callback_scopes,&ops)==TSFP_DEVICE_DISPATCH_COMPLETE);
    CHECK(driver_calls==1 && !tsfp_guest_objects_collect(&objects));counts(0,0,0);
    CHECK(tsfp_guest_scope_abort(&callback_scopes));
    /* Unknown target and invalid driver mapping never run guest code. */
    prepare_dispatch();put(ram+0x205,0x9999);retire_for_dispatch();
    CHECK(tsfp_device_dispatch_one(&factory,&callback_scopes,&ops)==TSFP_DEVICE_DISPATCH_QUARANTINED);
    CHECK(!driver_calls && tsfp_guest_objects_collect(&objects)==1);counts(1,0,1);
    put(ram+0x205,0x4444);retire_for_dispatch();driver_map_fail=1;
    CHECK(tsfp_device_dispatch_one(&factory,&callback_scopes,&ops)==TSFP_DEVICE_DISPATCH_QUARANTINED);
    CHECK(!driver_calls && tsfp_guest_objects_collect(&objects)==2);counts(2,0,2);
    CHECK(tsfp_guest_scope_abort(&callback_scopes));
    /* A separate fixture lifetime retains uncertain storage; do not re-init it.
     * The malformed/longjmp cases run in separate process modes below. */
    CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: real factory dispatch preserves registers, invokes validated driver, rejects targets and quarantines mappings");
}
static void dispatch_exit_case(int jump) {
    tsfp_guest_callback_ops ops={&factory,lookup_driver,output};
    prepare_dispatch();retire_for_dispatch();driver_malformed=!jump;driver_jump=jump;
    memset(ram+0x1000,0,20);MEM32(0x1000)=0x1111;MEM32(0x1004)=0x2222;MEM32(0x1010)=0x1700;
    if(jump) {
        switch(setjmp(*recomp_setjmp_slot(0x1000))) {
        case 0:(void)tsfp_device_dispatch_one(&factory,&callback_scopes,&ops);CHECK(0);break;
        case 7:break;
        default:CHECK(0);
        }
        CHECK(g_ebp==0x1111 && g_ebx==0x2222 && g_esp==0x1704);
    } else {
        CHECK(tsfp_device_dispatch_one(&factory,&callback_scopes,&ops)==TSFP_DEVICE_DISPATCH_QUARANTINED);
        CHECK(g_eax==0x1234 && g_ebx==0x5678 && g_esp==0x1800);
    }
    CHECK(driver_calls==1 && callback_scopes.current==&outer_scope);
#if TSFP_DEVICE_DISPATCH >= 4
    CHECK(!tsfp_device_factory_final_pins(&factory,&callback_lock_ticket));
#endif
    CHECK(tsfp_guest_objects_collect(&objects)==1);counts(1,0,1);CHECK(!frees);
    CHECK(tsfp_device_dispatch_one(&factory,&callback_scopes,&ops)==TSFP_DEVICE_DISPATCH_NOT_STARTED && driver_calls==1);
    CHECK(tsfp_guest_scope_abort(&callback_scopes));CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: malformed or nonlocal guest callback exit quarantines once and preserves target scope");
}
#endif
#ifdef TSFP_DEVICE_EVENT
#include "guest_device_event.h"
#if defined(_WIN32)
#include <windows.h>
static HANDLE event_ready,event_go;
static DWORD WINAPI waiter(LPVOID parameter) {
    tsfp_device_event pin={0};uint32_t address=*(uint32_t *)parameter;
    CHECK(tsfp_device_event_pin(&factory,address,&pin));
    CHECK(tsfp_device_event_try_wait(&pin)==0);CHECK(SetEvent(event_ready));
    CHECK(WaitForSingleObject(event_go,5000)==WAIT_OBJECT_0);
    CHECK(tsfp_device_event_try_wait(&pin)==1 && tsfp_device_event_try_wait(&pin)==0);
    CHECK(tsfp_device_event_release(&pin));return 0;
}
#endif
static void event_cases(void) {
    uint32_t body,address,span=7;void *mapping=(void *)(uintptr_t)9;
    tsfp_device_event first={0},second={0};tsfp_guest_object_lease lease={0};
    init();CHECK(create(0,0x170)==TSFP_DEVICE_OK);body=value(ram+0x801);address=body+0x34;
    CHECK(!tsfp_device_factory_pin(&factory,body+1,&lease,&mapping,&span));
    CHECK(!lease.owner && mapping==(void *)(uintptr_t)9 && span==7);
    CHECK(!tsfp_device_event_pin(&factory,0x33,&first));
    CHECK(!tsfp_device_event_pin(&factory,body,&first));
    CHECK(!tsfp_device_event_pin(&factory,address+4,&first));
    records[0].kind=TSFP_KIND_EVENT_AUTO;
    CHECK(!tsfp_device_event_pin(&factory,address,&first));
    records[0].kind=TSFP_KIND_DEVICE;
    ram[address]=0;CHECK(!tsfp_device_event_pin(&factory,address,&first));ram[address]=1;
    ram[address+2]=16;CHECK(!tsfp_device_event_pin(&factory,address,&first));ram[address+2]=4;
    CHECK(tsfp_device_event_pin(&factory,address,&first));
    CHECK(!tsfp_device_event_pin(&factory,address,&first));
    CHECK(tsfp_device_event_pin(&factory,address,&second));
    CHECK(value(ram+address+4)==1); /* Pinning never consumes the signal. */
    CHECK(tsfp_device_event_try_wait(&first)==1 && tsfp_device_event_try_wait(&second)==0);
    CHECK(tsfp_device_event_set(&second)==0 && tsfp_device_event_set(&first)==1);
    CHECK(tsfp_device_event_reset(&first)==1 && tsfp_device_event_reset(&second)==0);
    CHECK(tsfp_device_event_set(&first)==0 && tsfp_device_event_try_wait(&second)==1);
    CHECK(tsfp_device_factory_delete(&factory,body));CHECK(!frees && ram[body+0x1f]);
    CHECK(tsfp_device_event_set(&second)==0 && tsfp_device_event_try_wait(&first)==1);
    CHECK(tsfp_device_event_release(&first));CHECK(!frees && memory.heap.count==1);
    CHECK(tsfp_device_event_release(&second));counts(0,0,0);CHECK(frees==1 && !memory.heap.count);
    CHECK(!tsfp_device_event_release(&first) && tsfp_device_event_try_wait(&first)==-1);
    CHECK(tsfp_device_event_set(&first)==-1 && tsfp_device_event_reset(&first)==-1);
#if defined(_WIN32)
    {
        HANDLE worker;tsfp_device_event signal_pin={0};
        init();CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);address=body+0x34;
        CHECK(tsfp_device_event_pin(&factory,address,&signal_pin));
        CHECK(tsfp_device_event_reset(&signal_pin)==1);
        event_ready=CreateEventW(NULL,TRUE,FALSE,NULL);event_go=CreateEventW(NULL,TRUE,FALSE,NULL);
        CHECK(event_ready && event_go);worker=CreateThread(NULL,0,waiter,&address,0,NULL);CHECK(worker);
        CHECK(WaitForSingleObject(event_ready,5000)==WAIT_OBJECT_0);
        CHECK(tsfp_device_factory_delete(&factory,body) && !frees);
        CHECK(tsfp_device_event_set(&signal_pin)==0 && tsfp_device_event_release(&signal_pin));CHECK(!frees);
        CHECK(SetEvent(event_go));CHECK(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);
        CHECK(CloseHandle(worker) && CloseHandle(event_ready) && CloseHandle(event_go));
        counts(0,0,0);CHECK(frees==1 && !memory.heap.count);
    }
#endif
    puts("PASS: embedded auto-event pins parent across delete, shares guest signal bytes and releases once");
}
#endif
#ifdef TSFP_DEVICE_FINAL_EVENT
#include "guest_device_event.h"
static void final_event_cases(void) {
    uint32_t body,address;tsfp_device_finalization ticket,old,bad;
    tsfp_device_event first={0},second={0},unused={0};
    init();factory.ops.defer_finalization=1;CHECK(create(0,0)==TSFP_DEVICE_OK);
    body=value(ram+0x801);address=body+0x34;CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(!tsfp_device_event_pin(&factory,address,&unused));
    CHECK(tsfp_device_factory_take_finalization(&factory,&ticket));
    bad=ticket;bad.key^=1;CHECK(!tsfp_device_event_pin_finalization(&factory,&bad,address,&unused));
    bad=ticket;bad.driver^=4;CHECK(!tsfp_device_event_pin_finalization(&factory,&bad,address,&unused));
    CHECK(!tsfp_device_event_pin_finalization(&factory,&ticket,address+4,&unused));
    CHECK(tsfp_device_event_pin_finalization(&factory,&ticket,address,&first));
    CHECK(tsfp_device_event_pin_finalization(&factory,&ticket,address,&second));
    CHECK(tsfp_device_factory_final_pins(&factory,&ticket)==2);
    CHECK(!tsfp_device_event_pin_finalization(&factory,&ticket,address,&first));
    CHECK(tsfp_device_event_try_wait(&first)==1 && tsfp_device_event_try_wait(&second)==0);
    CHECK(tsfp_device_factory_finish_finalization(&factory,&ticket,1));
    CHECK(!tsfp_device_event_pin_finalization(&factory,&ticket,address,&unused));
    CHECK(tsfp_guest_objects_collect(&objects)==1 && !frees && memory.heap.count==1);
    CHECK(tsfp_device_event_set(&first)==0 && tsfp_device_event_try_wait(&second)==1);
    CHECK(tsfp_device_event_release(&first));CHECK(tsfp_device_factory_final_pins(&factory,&ticket)==1);
    CHECK(tsfp_guest_objects_collect(&objects)==1 && !frees);
    CHECK(tsfp_device_event_release(&second));CHECK(!tsfp_device_event_release(&second));
    CHECK(!tsfp_device_factory_final_pins(&factory,&ticket));
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);CHECK(frees==1);
    old=ticket;CHECK(create(0,0)==TSFP_DEVICE_OK);CHECK(value(ram+0x801)==body);
    CHECK(tsfp_device_factory_delete(&factory,body));CHECK(tsfp_device_factory_take_finalization(&factory,&ticket));
    CHECK(ticket.key!=old.key && !tsfp_device_event_pin_finalization(&factory,&old,address,&unused));
    CHECK(tsfp_device_event_pin_finalization(&factory,&ticket,address,&first));
    CHECK(tsfp_device_factory_finish_finalization(&factory,&ticket,0));
    CHECK(tsfp_guest_objects_collect(&objects)==1 && frees==1);counts(1,0,1);
    CHECK(tsfp_device_event_release(&first));CHECK(!tsfp_device_factory_final_pins(&factory,&ticket));
    CHECK(tsfp_guest_objects_collect(&objects)==1 && frees==1 && memory.heap.count==1);counts(1,0,1);
    puts("PASS: finalization event pins outlive completion, reject stale identities and retain uncertain cleanup");
}
#endif
int main(void) {
#ifdef TSFP_DEVICE_BRIDGE
    abi_cases();
#endif
#if defined(TSFP_DEVICE_FINAL_EVENT)
    final_event_cases();
#elif defined(TSFP_DEVICE_EVENT)
    event_cases();
#elif defined(TSFP_DEVICE_DISPATCH)
#if TSFP_DEVICE_DISPATCH == 2
    dispatch_exit_case(0);
#elif TSFP_DEVICE_DISPATCH == 3 || TSFP_DEVICE_DISPATCH == 5
    dispatch_exit_case(1);
#elif TSFP_DEVICE_DISPATCH == 4
    dispatch_exit_case(0);
#else
    dispatch_cases();
#endif
#elif defined(TSFP_DEVICE_CALLBACK_SCOPE)
    callback_scope_cases();
#elif defined(TSFP_DEVICE_FINALIZATION)
    deferred_finalization();
#else
    failures();named_lifetime();collision_rebind_capacity();stopping_and_cleanup();
#endif
    puts("PASS: device factory transactions, creator deletion, namespace rebinding and cleanup accounting");return 0;
}
