/* Original synthetic guest context, actual runtime headers, Windows locks and
 * project heap. No title bytes or translated game routines. */
#include "kernel.h"
#include "recomp_types.h"
#include "guest_virtual_memory.h"
#include "xbox_memory_layout.h"
#include "guest_object_services.h"
#include "guest_object_scope.h"
#include "guest_lifetime.h"
#include "guest_device_factory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"host line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned char ram[0x800000];
ptrdiff_t g_xbox_mem_offset;uint32_t g_xbox_low_shift;
static tsfp_allocations allocations;static tsfp_allocation_record heap_records[512];
static tsfp_guest_objects bridge_objects;
static tsfp_guest_handle_record handles[512];static tsfp_guest_object_record records[512];
static tsfp_guest_object_services bridge_object_services;
static RECOMP_TLS tsfp_guest_object_scopes bridge_object_scopes;
static int bridge_objects_initialized,deny_heap;static uint32_t deny_address;
static LONG entries,leaves;static unsigned warnings,driver_calls;static int driver_jumping;
static uint32_t current_body;static tsfp_device_finalization last_ticket;
static int bridge_device_free_body(uint32_t,uint32_t,uint32_t);
static void *resolve(void *context,uint32_t address,uint32_t bytes,int writable) {
    CHECK(context==ram && (writable==0 || writable==1));
    if(!address || !bytes || address==deny_address || address>=sizeof(ram) || bytes>sizeof(ram)-address)return NULL;
    return ram+address;
}
void xbox_GetGuestVirtualMemory(tsfp_guest_vm *memory) {
    memset(memory,0,sizeof(*memory));memory->allocations=&allocations;memory->host=ram;memory->resolve=resolve;
}
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) {
    CHECK(alignment==16);return deny_heap?0:tsfp_heap_allocate(&allocations,bytes,alignment);
}
void xbox_log(int level,const char *subsystem,const char *format,...) {
    CHECK(level==XBOX_LOG_WARN && !strcmp(subsystem,XBOX_LOG_IO));(void)format;++warnings;
}
static void *bridge_object_output(void *context,uint32_t address,uint32_t bytes) {(void)context;return resolve(ram,address,bytes,1);}
static void bridge_object_word(void *target,uint32_t v) {unsigned i;for(i=0;i<4;++i)((unsigned char *)target)[i]=(unsigned char)(v>>(i*8));}
static void traced_enter(LPCRITICAL_SECTION section) {EnterCriticalSection(section);InterlockedIncrement(&entries);}
static void traced_leave(LPCRITICAL_SECTION section) {InterlockedIncrement(&leaves);LeaveCriticalSection(section);}
#define EnterCriticalSection traced_enter
#define LeaveCriticalSection traced_leave
#include "guest_kernel_device_host.inc"
#undef EnterCriticalSection
#undef LeaveCriticalSection
static volatile uint32_t arguments[8];static unsigned allowed;
static uint32_t argument(unsigned i) {CHECK(i<allowed);return arguments[i];}
#define STACK_ARG(i) argument(i)
#include "guest_kernel_devices.inc"
#include "guest_kernel_names.inc"
#include "guest_kernel_device_events.inc"
static int close_host(void *context,uintptr_t host) {(void)context;(void)host;CHECK(0);return -1;}
static int free_body(void *context,uint32_t body,uint32_t kind,uint32_t flags) {(void)context;return bridge_device_free_body(body,kind,flags);}
static uint64_t capture(void) {return tsfp_guest_scope_mark(&bridge_object_scopes);}
static int restore(uint64_t mark) {return tsfp_guest_scope_unwind(&bridge_object_scopes,mark);}
static void counts(size_t live,size_t uncertain) {
    size_t a,b,c;tsfp_device_factory_counts(&bridge_devices,&a,&b,&c);
    CHECK(a==live && !b && c==uncertain);
}
static void forbidden_driver(void) {CHECK(0);}
static void driver(void) {
    tsfp_device_event_scope event;uint32_t stack=g_esp;
    CHECK(MEM32(stack)==0 && MEM32(stack+4)==current_body && ram[current_body+0x1f]==1);
    CHECK(bridge_device_pumping && tsfp_device_callback_ticket(&bridge_object_scopes,&bridge_devices,current_body,&last_ticket));
    CHECK(bridge_device_event_begin(&event,current_body+0x34));
    CHECK(tsfp_device_factory_final_pins(&bridge_devices,&last_ticket)==1);
    CHECK(tsfp_device_event_try_wait(&event.event)==1);++driver_calls;
    bridge_device_pump();CHECK(tsfp_device_factory_final_pins(&bridge_devices,&last_ticket)==1);
    g_eax=0xdddd;g_ebx=0xeeee;
    if(driver_jumping) {(void)recomp_guest_longjmp(0x1000,7);CHECK(0);}
    CHECK(tsfp_device_event_scope_end(&bridge_object_scopes,&event));
    g_esp=stack+8;
}
tsfp_guest_entry_t recomp_lookup_manual(uint32_t address) {return address==0x4444?driver:NULL;}
tsfp_guest_entry_t recomp_lookup(uint32_t address) {return address==0x4444?forbidden_driver:NULL;}
static uint32_t create(int named) {
    allowed=6;arguments[0]=0x201;arguments[1]=0x170;arguments[2]=named?0x301:0;
    arguments[3]=0x3a;arguments[4]=0x100;arguments[5]=0x801;
    MEM32(0x801)=0xcccccccc;bridge_IoCreateDevice();
    return g_eax?0:MEM32(0x801);
}
static void retire(uint32_t body) {allowed=1;arguments[0]=body;bridge_IoDeleteDevice();CHECK(!g_eax);}
static unsigned legacy_calls,wakes;static int legacy_action;static uint32_t wait_bodies[2];
static void event_wake_waiters(uint32_t address) {CHECK(address==wait_bodies[0]+0x34);++wakes;}
static void observe_legacy(unsigned count) {
    unsigned i;++legacy_calls;
    if(legacy_action) {
        for(i=0;i<count;++i)retire(wait_bodies[i]);
        bridge_device_pump();CHECK(allocations.heap.count==count);
        if(legacy_action==2) {(void)recomp_guest_longjmp(0x1000,7);CHECK(0);}
    }
    g_eax=0xabc;
}
static void bridge_KeSetEvent_legacy(void) {observe_legacy(1);}
static void bridge_KeResetEvent_legacy(void) {observe_legacy(1);}
static void bridge_KePulseEvent_legacy(void) {observe_legacy(1);}
static void bridge_KeSetEventBoostPriority_legacy(void) {observe_legacy(1);}
static void bridge_KeWaitForSingleObject_legacy(void) {observe_legacy(1);}
static void bridge_KeWaitForMultipleObjects_legacy(void) {observe_legacy(2);}
static void wait_arguments(void) {
    unsigned i;for(i=0;i<8;++i)arguments[i]=0;allowed=8;
    arguments[0]=2;arguments[1]=0x601;
    MEM32(0x601)=wait_bodies[0]+0x34;MEM32(0x605)=wait_bodies[1]+0x34;
}
static void event_wrappers(void) {
    unsigned before;uint32_t body;
    bridge_object_word(ram+0x205,0);
    body=create(0);CHECK(body);wait_bodies[0]=body;allowed=3;arguments[0]=body+0x34;
    bridge_KeResetEvent();CHECK(g_eax==1 && MEM32(body+0x38)==0 && !legacy_calls);
    bridge_KeResetEvent();CHECK(g_eax==0 && MEM32(body+0x38)==0);
    allowed=2;bridge_KeSetEventBoostPriority();CHECK(!g_eax && MEM32(body+0x38)==1 && wakes==1 && !legacy_calls);
    retire(body);bridge_device_pump();CHECK(!allocations.heap.count);
    /* Each forwarding route pins the parent across deletion by its callee. */
    body=create(0);wait_bodies[0]=body;allowed=3;arguments[0]=body+0x34;legacy_action=1;
    bridge_KeSetEvent();CHECK(g_eax==0xabc && legacy_calls==1 && allocations.heap.count==1);bridge_device_pump();CHECK(!allocations.heap.count);
    body=create(0);wait_bodies[0]=body;allowed=3;arguments[0]=body+0x34;
    bridge_KePulseEvent();CHECK(g_eax==0xabc && legacy_calls==2);bridge_device_pump();CHECK(!allocations.heap.count);
    body=create(0);wait_bodies[0]=body;allowed=5;arguments[0]=body+0x34;
    bridge_KeWaitForSingleObject();CHECK(g_eax==0xabc && legacy_calls==3);bridge_device_pump();CHECK(!allocations.heap.count);
    /* A known retired event cannot fall back to unpinned legacy memory. */
    body=create(0);wait_bodies[0]=body;retire(body);before=legacy_calls;allowed=5;arguments[0]=body+0x34;
    bridge_KeWaitForSingleObject();CHECK(g_eax==TSFP_OBJECT_BAD_PARAMETER && legacy_calls==before);bridge_device_pump();CHECK(!allocations.heap.count);
    /* Complete unaligned array, two device parents and no per-element frames. */
    wait_bodies[0]=create(0);wait_bodies[1]=create(0);CHECK(wait_bodies[0] && wait_bodies[1]);wait_arguments();
    bridge_KeWaitForMultipleObjects();CHECK(g_eax==0xabc && legacy_calls==before+1 && allocations.heap.count==2);
    bridge_device_pump();CHECK(!allocations.heap.count);
    /* Invalid counts/spans do not invoke legacy or acquire any pins. */
    before=legacy_calls;legacy_action=0;wait_arguments();arguments[0]=0;
    bridge_KeWaitForMultipleObjects();CHECK(g_eax==TSFP_OBJECT_BAD_PARAMETER);
    arguments[0]=65;bridge_KeWaitForMultipleObjects();CHECK(g_eax==TSFP_OBJECT_BAD_PARAMETER);
    arguments[0]=2;arguments[1]=UINT32_MAX-3;bridge_KeWaitForMultipleObjects();CHECK(g_eax==TSFP_OBJECT_BAD_ADDRESS && legacy_calls==before);
    /* Partial failure releases the earlier pin; creators remain owned. */
    wait_bodies[0]=create(0);wait_bodies[1]=create(0);CHECK(wait_bodies[0] && wait_bodies[1]);wait_arguments();
    ram[wait_bodies[1]+0x34]=0;
    bridge_KeWaitForMultipleObjects();CHECK(g_eax==TSFP_OBJECT_BAD_PARAMETER && legacy_calls==before && !bridge_object_scopes.current);
    ram[wait_bodies[1]+0x34]=1;retire(wait_bodies[0]);retire(wait_bodies[1]);bridge_device_pump();CHECK(!allocations.heap.count);
    /* Other providers' events preserve the existing fallback route. */
    allowed=1;arguments[0]=0xface;bridge_KeResetEvent();CHECK(g_eax==0xabc && legacy_calls==before+1);
    allowed=2;bridge_KeSetEventBoostPriority();CHECK(g_eax==0xabc && legacy_calls==before+2);
    counts(0,0);CHECK(!bridge_object_scopes.current);
}
static void interrupted_multi_wait(void) {
    tsfp_guest_object_scope outer;uint64_t mark;
    bridge_object_word(ram+0x205,0);wait_bodies[0]=create(0);wait_bodies[1]=create(0);CHECK(wait_bodies[0] && wait_bodies[1]);
    wait_arguments();legacy_action=2;CHECK(tsfp_guest_scope_enter(&bridge_object_scopes,&outer,&bridge_objects));
    mark=tsfp_guest_scope_mark(&bridge_object_scopes);MEM32(0x1000)=0x1111;MEM32(0x1004)=0x2222;MEM32(0x1010)=0x1700;
    switch(setjmp(*recomp_setjmp_slot(0x1000))) {
    case 0:bridge_KeWaitForMultipleObjects();CHECK(0);break;
    case 7:break;
    default:CHECK(0);
    }
    CHECK(legacy_calls==1 && bridge_object_scopes.current==&outer && tsfp_guest_scope_mark(&bridge_object_scopes)==mark);
    CHECK(g_ebp==0x1111 && g_ebx==0x2222 && g_esp==0x1704);
    CHECK(tsfp_guest_scope_leave(&bridge_object_scopes,&outer));g_esp=0x1800;bridge_device_pump();
    CHECK(!allocations.heap.count && !bridge_object_scopes.current);counts(0,0);
}
static void initialize(void) {
    tsfp_allocation_config config;const char name[]="\\Device\\NativeFixture";
    memset(&config,0,sizeof(config));config.ram=ram;config.ram_size=0x400000;
    config.heap_begin=0x2000;config.reserve_begin=0x400000;
    CHECK(tsfp_allocations_init(&allocations,&config,heap_records,512,NULL,0));
    g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x1800;
    CHECK(tsfp_guest_objects_init(&bridge_objects,handles,records,512,close_host,free_body,NULL));
    bridge_object_services.objects=&bridge_objects;bridge_object_services.resolve_output=bridge_object_output;
    bridge_objects_initialized=1;CHECK(tsfp_guest_lifetime_hooks(capture,restore));bridge_device_init();
    CHECK(MEM32(XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE)==XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE);
    CHECK(bridge_object_services.type_id[TSFP_KIND_DEVICE]==XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE);
    bridge_object_word(ram+0x205,0x4444);memcpy(ram+0x401,name,sizeof(name)-1);
    ram[0x301]=(unsigned char)(sizeof(name)-1);ram[0x303]=ram[0x301];bridge_object_word(ram+0x305,0x401);
    bridge_device_pump();CHECK(!bridge_device_pumping && !bridge_object_scopes.current);counts(0,0);
}
static void normal(void) {
    uint32_t body;unsigned before;
    deny_address=0x801;CHECK(!create(1) && g_eax==TSFP_OBJECT_BAD_ADDRESS && !allocations.heap.count);
    deny_address=0;deny_heap=1;CHECK(!create(1) && g_eax==TSFP_OBJECT_NO_MEMORY && !allocations.heap.count);deny_heap=0;
    body=create(1);CHECK(body && MEM32(body+0x18)==body+72 && MEM32(body+0x14)==0x18);
    current_body=body;CHECK(!create(1) && g_eax==0xc0000035 && MEM32(0x801)==0xcccccccc && allocations.heap.count==1);
    allowed=5;arguments[0]=0x301;arguments[1]=0x40;arguments[2]=XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE;arguments[3]=0;arguments[4]=0x901;
    MEM32(0x901)=0xcccccccc;bridge_ObReferenceObjectByName();CHECK(!g_eax && MEM32(0x901)==body);
    arguments[2]=XBOX_KERNEL_DATA_BASE+KDATA_FILE_OBJ_TYPE;MEM32(0x901)=0xcccccccc;
    bridge_ObReferenceObjectByName();CHECK(g_eax==TSFP_OBJECT_WRONG_TYPE && MEM32(0x901)==0xcccccccc);
    retire(body);bridge_device_pump();CHECK(!driver_calls && allocations.heap.count==1 && !bridge_device_names.count);
    CHECK(tsfp_guest_object_dereference_body(&bridge_object_services,body));
    g_eax=0x1234;g_ebx=0x5678;g_esp=0x1800;bridge_device_pump();
    CHECK(driver_calls==1 && g_eax==0x1234 && g_ebx==0x5678 && g_esp==0x1800);
    CHECK(!bridge_device_pumping && !bridge_object_scopes.current && !allocations.heap.count);counts(0,0);
    before=driver_calls;bridge_object_word(ram+0x205,0);body=create(0);CHECK(body);retire(body);bridge_device_pump();
    CHECK(driver_calls==before && !allocations.heap.count);counts(0,0);
}
static void interrupted(void) {
    tsfp_guest_object_scope outer;uint64_t mark;uint32_t body;
    body=create(0);CHECK(body);current_body=body;retire(body);
    CHECK(tsfp_guest_scope_enter(&bridge_object_scopes,&outer,&bridge_objects));mark=tsfp_guest_scope_mark(&bridge_object_scopes);
    MEM32(0x1000)=0x1111;MEM32(0x1004)=0x2222;MEM32(0x1008)=0x3333;MEM32(0x100c)=0x4444;MEM32(0x1010)=0x1700;driver_jumping=1;
    switch(setjmp(*recomp_setjmp_slot(0x1000))) {
    case 0:bridge_device_pump();CHECK(0);break;
    case 7:break;
    default:CHECK(0);
    }
    CHECK(driver_calls==1 && !bridge_device_pumping && bridge_object_scopes.current==&outer);
    CHECK(tsfp_guest_scope_mark(&bridge_object_scopes)==mark && g_ebp==0x1111 && g_ebx==0x2222 && g_esp==0x1704);
    CHECK(!tsfp_device_factory_final_pins(&bridge_devices,&last_ticket));
    CHECK(tsfp_guest_objects_collect(&bridge_objects)==1);counts(1,1);CHECK(allocations.heap.count==1);
    CHECK(tsfp_guest_scope_leave(&bridge_object_scopes,&outer));
    driver_jumping=0;body=create(0);CHECK(body);current_body=body;retire(body);g_esp=0x1800;bridge_device_pump();
    CHECK(driver_calls==2 && !bridge_device_pumping && !bridge_object_scopes.current && allocations.heap.count==1);counts(1,1);
}
int main(void) {
    initialize();
#if TSFP_HOST_MODE == 3
    event_wrappers();
#elif TSFP_HOST_MODE == 4
    interrupted_multi_wait();
#elif TSFP_HOST_MODE == 2
    interrupted();
#else
    normal();
#endif
    CHECK(entries>0 && entries==leaves && !warnings);
    CHECK(!tsfp_guest_names_stop(&bridge_device_names));CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    DeleteCriticalSection(&bridge_device_lock);
    puts("PASS: actual-header Windows host bindings initialize namespace/type, real heap/locks, callback pump and scoped event cleanup");return 0;
}
