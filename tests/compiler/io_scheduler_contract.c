#define TSFP_REAL_DEVICE_BODY
#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_scheduler.h"
#include "guest_lifetime.h"
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION manager_mutex;
static __declspec(thread) unsigned manager_depth;
#else
#include <pthread.h>
static pthread_mutex_t manager_mutex;
static _Thread_local unsigned manager_depth;
#endif
ptrdiff_t g_xbox_mem_offset;
static tsfp_io_scheduler manager;static tsfp_io_schedule_record queues[2];static tsfp_io_owner *waiting[4];
typedef struct {tsfp_io_owner owner;tsfp_io_devices binding;tsfp_io_device_entry entry[1];unsigned retired,disposed;} packet;
static packet packets[3];static unsigned request_depth,mode,called,stop_preparing,deleted,escaped,expected_frees,submit_while_closing;
static uint32_t body;static tsfp_guest_object_scopes scopes;static tsfp_guest_object_scope outer;
static tsfp_io_queue_scope api;
static unsigned fail_release,release_attempts;
extern int tsfp_guest_objects_release_actual(tsfp_guest_object_lease *);
int tsfp_guest_objects_release(tsfp_guest_object_lease *lease) {
    if(lease==&queues[0].device) {
        ++release_attempts;if(fail_release) {fail_release=0;return 0;}
    }
    return tsfp_guest_objects_release_actual(lease);
}
static void manager_enter(void *c) {
    CHECK(c==&manager && !manager_depth);
#ifdef _WIN32
    EnterCriticalSection(&manager_mutex);
#else
    CHECK(!pthread_mutex_lock(&manager_mutex));
#endif
    CHECK(!manager_depth);++manager_depth;
}
static void manager_leave(void *c) {
    CHECK(c==&manager && manager_depth==1);--manager_depth;
#ifdef _WIN32
    LeaveCriticalSection(&manager_mutex);
#else
    CHECK(!pthread_mutex_unlock(&manager_mutex));
#endif
    if(stop_preparing && queues[0].state==TSFP_IO_QUEUE_PREPARING) {
        stop_preparing=0;tsfp_io_scheduler_stop(&manager);
    }
    if(submit_while_closing && queues[0].state==TSFP_IO_QUEUE_LIVE && queues[0].closed) {
        tsfp_io_owner *selected=(tsfp_io_owner *)1;submit_while_closing=0;
        CHECK(!tsfp_io_scheduler_submit(&api.ticket,&packets[2].owner,&packets[2].binding,&selected));
        CHECK(selected==(tsfp_io_owner *)1);
    }
}
static int tracked_deallocate(void *c,uint32_t address) {CHECK(!manager_depth);return deallocate(c,address);}
static void request_enter(void *c) {(void)c;CHECK(!request_depth);++request_depth;}
static void request_leave(void *c) {(void)c;CHECK(request_depth==1);--request_depth;}
static void binding_retired(void *c,tsfp_io_devices *binding,int clean) {
    packet *p=c;CHECK(binding==&p->binding && clean && !request_depth && !manager_depth && !p->retired && !p->disposed);++p->retired;
}
static void request_dispose(void *c,uint32_t address,void *mapping,size_t span) {
    packet *p=c;CHECK(!request_depth && !manager_depth && p->retired==1 && !p->disposed && span==256);
    CHECK(address==0x1000+(uint32_t)(p-packets)*256 && mapping==ram+address);++p->disposed;
}
static void setup(size_t capacity) {
    unsigned i;init();mode=called=stop_preparing=deleted=escaped=submit_while_closing=fail_release=release_attempts=0;expected_frees=1;memset(packets,0,sizeof(packets));
    factory.ops.deallocate=tracked_deallocate;
    CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);memset(ram+0x201,0,68);put(ram+0x201,0x4444);
    CHECK(tsfp_io_scheduler_init(&manager,&factory,queues,capacity,waiting,2,&manager,manager_enter,manager_leave));
    for(i=0;i<3;++i) {
        tsfp_io_owner_ops ops={&packets[i],request_enter,request_leave,request_dispose};uint32_t address=0x1000+i*256,slot;
        CHECK(tsfp_io_owner_init(&packets[i].owner,&ops,ram+address,256,address,256,2,6));
        CHECK(tsfp_io_request_advance(ram+address,256,address,&slot));
        CHECK(tsfp_io_devices_init(&packets[i].binding,&packets[i].owner,&factory,packets[i].entry,1,binding_retired,&packets[i]));
    }
    g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x9007;g_ebx=0x2222;g_eax=0x1111;
}
static void begin(unsigned i) {
    tsfp_io_ticket t={0};CHECK(tsfp_io_owner_dispatch_begin(&packets[i].owner,&t));
    CHECK(tsfp_io_owner_dispatch_publish(&t));CHECK(tsfp_io_owner_dispatch_end(&t,0));
}
static void counts_manager(size_t live,size_t quarantined) {
    size_t a=99,b=99;tsfp_io_scheduler_counts(&manager,&a,&b);CHECK(a==live && b==quarantined);
}
static void drain(void) {
    size_t i;tsfp_io_scheduler_stop(&manager);
    for(i=0;i<manager.capacity;++i)if(queues[i].state==TSFP_IO_QUEUE_LIVE) {
        tsfp_io_queue_ticket t={0};tsfp_io_owner *owner=queues[i].queue.current,*selected=(tsfp_io_owner *)1;
        CHECK(!queues[i].references && owner && tsfp_io_owner_retain(owner));
        CHECK(tsfp_io_scheduler_acquire(&manager,queues[i].body,0,&t));
        CHECK(tsfp_io_scheduler_next(&t,owner,&selected) && !selected);
        CHECK(tsfp_io_owner_release(owner));CHECK(tsfp_io_scheduler_release(&t));
    }
    counts_manager(0,0);
}
static void finish(void) {
    unsigned i;drain();
    if(!deleted){CHECK(tsfp_device_factory_delete(&factory,body));deleted=1;}
    for(i=0;i<3;++i) {
        tsfp_io_owner *owner=&packets[i].owner;
        if(owner->phase==TSFP_IO_ACTIVE || owner->phase==TSFP_IO_PENDING)CHECK(tsfp_io_owner_complete(owner,0,17,NULL,NULL));
        if(owner->clients)CHECK(owner->clients==1 && tsfp_io_owner_release(owner));
        CHECK(packets[i].retired==1 && packets[i].disposed==1);
    }
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);CHECK(frees==expected_frees && !memory.heap.count && !scopes.current);
}
static void start_entry(void) {
    uint32_t sp=g_esp;tsfp_io_owner *owner=&packets[0].owner,*selected=(tsfp_io_owner *)1;
    CHECK(!manager_depth && !request_depth && !depth && sp==0x8ffb && MEM32(sp)==0);
    CHECK(MEM32(sp+4)==body && MEM32(sp+8)==owner->address && owner->clients==2 && owner->internal==2);
    CHECK(value(ram+body+0x10)==owner->address && ram[body+0x2b]==1 && queues[0].references==1);++called;
    if(mode==1 || mode==2 || mode==3 || mode==5) {
        if(mode==3 || mode==5) {begin(2);submit_while_closing=1;}
        if(mode==5)tsfp_io_scheduler_stop(&manager);
        else CHECK(tsfp_io_scheduler_close(&manager,body));
        CHECK(tsfp_device_factory_delete(&factory,body));deleted=1;
        CHECK(!frees && queues[0].state==TSFP_IO_QUEUE_LIVE && queues[0].references==1);
        if(mode==3 || mode==5) {
            tsfp_io_owner *rejected=(tsfp_io_owner *)1;
            CHECK(!tsfp_io_scheduler_submit(&api.ticket,&packets[2].owner,&packets[2].binding,&rejected));
            CHECK(rejected==(tsfp_io_owner *)1);
        }
    }
    if(mode==2) {(void)recomp_guest_longjmp(0x500,7);CHECK(0);}
    if(mode==1) {
        CHECK(tsfp_io_owner_complete(owner,0,17,NULL,NULL));
        CHECK(tsfp_io_scheduler_next(&api.ticket,owner,&selected) && !selected);
        CHECK(!frees && queues[0].state==TSFP_IO_QUEUE_LIVE && queues[0].references==1);
    } else CHECK(tsfp_io_owner_pending(owner));
    g_eax=0xdeadbeef;g_esp=sp+12;
}
static tsfp_guest_entry_t lookup_start(void *c,uint32_t address) {CHECK(c==&manager && !manager_depth && !request_depth);return mode==4 || address!=0x4444?NULL:start_entry;}
static void *callback_output(void *c,uint32_t address,uint32_t bytes) {CHECK(c==&manager && bytes==12 && !manager_depth);return address>sizeof(ram)-bytes?NULL:ram+address;}
static const void *driver_input(void *c,uint32_t address,uint32_t bytes) {CHECK(c==&manager && address==0x201 && bytes==68 && !manager_depth);return ram+address;}
static uint64_t capture_scopes(void) {return tsfp_guest_scope_mark(&scopes);}
static int restore_scopes(uint64_t mark) {return tsfp_guest_scope_unwind(&scopes,mark);}
static void pipeline(unsigned selected_mode) {
    tsfp_io_owner *selected=(tsfp_io_owner *)1;tsfp_io_call_ops ops={{&manager,lookup_start,callback_output},&manager,driver_input};
    setup(2);mode=selected_mode;begin(0);if(mode==3 || mode==5)begin(1);
    CHECK(tsfp_guest_scope_enter(&scopes,&outer,&objects));
    if(mode==2) {
        MEM32(0x500)=0x1122;MEM32(0x504)=0x3344;MEM32(0x508)=0x5566;MEM32(0x50c)=0x7788;MEM32(0x510)=0x9100;
        switch(setjmp(*recomp_setjmp_slot(0x500))) {
        case 0:
            CHECK(tsfp_io_queue_scope_begin(&scopes,&api,&manager,body,1));
            CHECK(tsfp_io_scheduler_submit(&api.ticket,&packets[0].owner,&packets[0].binding,&selected) && selected==&packets[0].owner);
            CHECK(tsfp_io_owner_release(&packets[0].owner));
            (void)tsfp_io_scheduler_startio(&scopes,&api,selected,&packets[0].binding,&ops);++escaped;CHECK(0);break;
        case 7:break;
        default:CHECK(0);
        }
        CHECK(!escaped && !api.ticket.active && !api.stack && g_esp==0x9104 && g_ebx==0x3344);
        CHECK(queues[0].state==TSFP_IO_QUEUE_LIVE && !queues[0].references && queues[0].closed && called==1);
    } else {
        CHECK(tsfp_io_queue_scope_begin(&scopes,&api,&manager,body,1));
        CHECK(tsfp_io_scheduler_submit(&api.ticket,&packets[0].owner,&packets[0].binding,&selected) && selected==&packets[0].owner);
        CHECK(tsfp_io_owner_release(&packets[0].owner));
        if(mode==3 || mode==5) {
            tsfp_io_owner *pending=(tsfp_io_owner *)1;
            CHECK(tsfp_io_scheduler_submit(&api.ticket,&packets[1].owner,&packets[1].binding,&pending) && !pending);
            CHECK(tsfp_io_owner_release(&packets[1].owner));
        }
        CHECK(tsfp_io_scheduler_startio(&scopes,&api,selected,&packets[0].binding,&ops)==(mode==4?TSFP_IO_CALL_NOT_ENTERED:TSFP_IO_CALL_RETURNED));
        CHECK(g_eax==0x1111 && g_ebx==0x2222 && g_esp==0x9007);
        if(mode==1)CHECK(packets[0].disposed==1 && !frees && queues[0].references==1);
        if(mode==3 || mode==5)CHECK(!packets[1].disposed && !packets[1].owner.clients && packets[1].owner.internal==1 && packets[1].binding.count==1);
        CHECK(tsfp_io_queue_scope_end(&scopes,&api));
        if(mode==1) {counts_manager(0,0);CHECK(frees==1);}
    }
    CHECK(scopes.current==&outer);CHECK(tsfp_guest_scope_leave(&scopes,&outer));finish();
}
static void registry(void) {
    tsfp_io_queue_ticket first={0},second={0},stale={0},empty={0};uint32_t other;unsigned char before[72];
    setup(1);CHECK(tsfp_io_scheduler_acquire(&manager,body,1,&first));counts_manager(1,0);
    CHECK(create(0,0)==TSFP_DEVICE_OK);other=value(ram+0x801);++expected_frees;
    CHECK(!tsfp_io_scheduler_acquire(&manager,other,1,&second) && !memcmp(&second,&empty,sizeof(second)));
    memcpy(&stale,&first,sizeof(first));CHECK(tsfp_io_scheduler_close(&manager,body));
    CHECK(tsfp_io_scheduler_release(&first));counts_manager(0,0);
    CHECK(tsfp_io_scheduler_acquire(&manager,body,1,&second) && second.generation!=stale.generation);
    CHECK(!tsfp_io_scheduler_release(&stale) && queues[0].references==1);
    CHECK(tsfp_io_scheduler_close(&manager,body));CHECK(tsfp_io_scheduler_release(&second));
    queues[0].generation=UINT64_MAX;CHECK(!tsfp_io_scheduler_acquire(&manager,other,1,&first));
    CHECK(tsfp_device_factory_delete(&factory,other));CHECK(frees==1);
    finish();
    setup(1);memcpy(before,ram+body,72);ram[body]=0;
    CHECK(!tsfp_io_scheduler_acquire(&manager,body,1,&first));counts_manager(0,0);ram[body]=before[0];
    CHECK(!memcmp(before,ram+body,72));
    stop_preparing=1;CHECK(!tsfp_io_scheduler_acquire(&manager,body,1,&first));
    CHECK(manager.closed && !stop_preparing && !first.active);counts_manager(0,0);finish();
}
static void quarantine(void) {
    tsfp_io_queue_ticket t={0},rejected={0};unsigned attempts;
    setup(1);CHECK(tsfp_io_scheduler_acquire(&manager,body,1,&t));
    CHECK(tsfp_io_scheduler_close(&manager,body));fail_release=1;
    CHECK(!tsfp_io_scheduler_release(&t) && !t.active && !t.manager);
    counts_manager(0,1);attempts=release_attempts;CHECK(attempts==1 && queues[0].device.owner==&objects);
    CHECK(!tsfp_io_scheduler_acquire(&manager,body,1,&rejected));
    tsfp_io_scheduler_stop(&manager);CHECK(release_attempts==attempts && !frees);
    /* Fixture-only recovery of the known injected retained failure. The actual
     * manager never retries a quarantined lease or silently reuses its slot. */
    CHECK(tsfp_guest_objects_release_actual(&queues[0].device));
    queues[0].state=TSFP_IO_QUEUE_FREE;queues[0].body=0;finish();
}
int main(void) {
    unsigned which;
#ifdef _WIN32
    InitializeCriticalSection(&manager_mutex);
#else
    CHECK(!pthread_mutex_init(&manager_mutex,NULL));
#endif
    CHECK(tsfp_guest_lifetime_hooks(capture_scopes,restore_scopes));
    for(which=0;which<6;++which)pipeline(which);registry();quarantine();
    CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
#ifdef _WIN32
    DeleteCriticalSection(&manager_mutex);
#else
    CHECK(!pthread_mutex_destroy(&manager_mutex));
#endif
    puts("PASS: persistent device queue lease, scoped API/native unwind, close/drain/current retention, generation/capacity guards and preparing-stop rollback");return 0;
}
