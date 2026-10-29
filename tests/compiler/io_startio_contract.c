#define TSFP_REAL_DEVICE_BODY
#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_startio.h"
#include "guest_io_queue.h"
#include "guest_lifetime.h"
ptrdiff_t g_xbox_mem_offset;
static tsfp_io_owner owner;static tsfp_io_devices binding;static tsfp_io_device_entry device_entries[2];
static tsfp_io_queue queue;static tsfp_io_owner *entries[2];
static tsfp_guest_object_scopes scopes;static tsfp_guest_object_scope outer,extra;
static unsigned request_depth,queue_depth,retired_count,disposed,callbacks,mode,armed,extra_released,escaped;
static uint32_t body;static unsigned char before_packet[256];
static void request_lock(void *c) {CHECK(c==&owner && !request_depth);++request_depth;}
static void request_unlock(void *c) {
    CHECK(c==&owner && request_depth==1);--request_depth;
    if(armed && owner.internal==2) {
        armed=0;
        if(mode==9)g_esp-=4;
        else if(mode==11)CHECK(tsfp_io_owner_complete(&owner,0,17,NULL,NULL));
    }
}
static void queue_lock(void *c) {CHECK(c==&queue && !queue_depth);++queue_depth;}
static void queue_unlock(void *c) {CHECK(c==&queue && queue_depth==1);--queue_depth;}
static void retired(void *c,tsfp_io_devices *value,int clean) {
    CHECK(c==&owner && value==&binding && clean && !request_depth && !queue_depth && !retired_count && !disposed);++retired_count;
}
static void request_dispose(void *c,uint32_t address,void *mapping,size_t bytes) {
    CHECK(c==&owner && address==0x1000 && mapping==ram+0x1000 && bytes==256);
    CHECK(!request_depth && !queue_depth && retired_count==1 && !disposed);++disposed;
}
static int release_extra(void *c,int abandoned) {CHECK(c==&extra && abandoned && !extra_released);++extra_released;return 1;}
static void start_entry(void) {
    uint32_t sp=g_esp;tsfp_io_owner *selected=(tsfp_io_owner *)1;
    CHECK(!request_depth && !queue_depth && !depth && sp==0x8ffb && g_seh_ebp==sp && MEM32(sp)==0);
    CHECK(MEM32(sp+4)==body && MEM32(sp+8)==0x1000);
    CHECK(!memcmp(ram+0x1000,before_packet,sizeof(before_packet)));
    CHECK(owner.clients==2 && owner.internal==2 && queue.current==&owner);++callbacks;
    CHECK(tsfp_device_factory_delete(&factory,body));CHECK(!tsfp_guest_objects_collect(&objects) && !frees);
    g_ebx=0x9999;g_eax=0xfeedface; /* StartIo is void: EAX is not a status. */
    if(mode==3) {(void)recomp_guest_longjmp(0x500,7);CHECK(0);}
    if(mode==1) {
        CHECK(tsfp_io_owner_complete(&owner,0,17,NULL,NULL));
        CHECK(tsfp_io_queue_next(&queue,&owner,&selected) && !selected);
        CHECK(!disposed && !frees && owner.internal==1 && owner.clients==1);
    } else if(mode==14)CHECK(tsfp_io_owner_phase(&owner)==TSFP_IO_PENDING);
    else CHECK(tsfp_io_owner_pending(&owner));
    if(mode==10) {CHECK(tsfp_guest_scope_enter(&scopes,&extra,&objects));CHECK(tsfp_guest_scope_on_leave(&scopes,&extra,release_extra,&extra));}
    g_esp=sp+(mode==2?8:12);
}
static tsfp_guest_entry_t lookup_start(void *c,uint32_t address) {
    CHECK(c==&owner && !request_depth && !queue_depth && !depth);
    return mode==4 || address!=0x4444?NULL:start_entry;
}
static void *callback_output(void *c,uint32_t address,uint32_t bytes) {
    CHECK(c==&owner && !request_depth && !queue_depth && !depth && bytes==12);
    return mode==5 || address>sizeof(ram)-bytes?NULL:ram+address;
}
static const void *driver_input(void *c,uint32_t address,uint32_t bytes) {
    CHECK(c==&owner && !request_depth && !queue_depth && !depth && bytes==68);
    return mode==12 || address!=0x201?NULL:ram+address;
}
static uint64_t capture_scopes(void) {return tsfp_guest_scope_mark(&scopes);}
static int restore_scopes(uint64_t mark) {return tsfp_guest_scope_unwind(&scopes,mark);}
static void scenario(unsigned which) {
    tsfp_io_owner_ops owner_ops={&owner,request_lock,request_unlock,request_dispose};
    tsfp_io_call_ops ops={{&owner,lookup_start,callback_output},&owner,driver_input};
    tsfp_io_ticket initial={0};tsfp_io_owner *selected,*empty=(tsfp_io_owner *)1;uint32_t slot;int result;
    init();mode=which;retired_count=disposed=callbacks=armed=extra_released=escaped=0;
    g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x9007;g_seh_ebp=0x1234;g_eax=0x1111;g_ebx=0x2222;
    CHECK(tsfp_io_queue_init(&queue,entries,2,&queue,queue_lock,queue_unlock));
    CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);memset(ram+0x201,0,68);put(ram+0x201,0x4444);
    CHECK(tsfp_io_owner_init(&owner,&owner_ops,ram+0x1000,256,0x1000,256,2,6));
    CHECK(tsfp_io_devices_init(&binding,&owner,&factory,device_entries,2,retired,&owner));
    CHECK(tsfp_io_request_advance(owner.bytes,owner.span,owner.address,&slot) && slot==0x1080);
    if(mode!=13) {
        CHECK(tsfp_io_owner_dispatch_begin(&owner,&initial));CHECK(tsfp_io_owner_dispatch_publish(&initial));
        CHECK(tsfp_io_owner_dispatch_end(&initial,0));
        if(mode==14)CHECK(tsfp_io_owner_pending(&owner));
        CHECK(tsfp_io_queue_push(&queue,&owner));CHECK(tsfp_io_queue_next(&queue,NULL,&selected) && selected==&owner);
        CHECK(tsfp_io_owner_release(&owner)); /* creator; queue and selected refs remain */
    }
    if(mode==6)put(ram+0x201,0);
    if(mode==7)put(ram+body+8,UINT32_MAX-4);
    if(mode==8)CHECK(tsfp_io_owner_complete(&owner,0,17,NULL,NULL));
    if(mode==9 || mode==11)armed=1;
    memcpy(before_packet,ram+0x1000,256);
    CHECK(tsfp_guest_scope_enter(&scopes,&outer,&objects));
    if(mode==3) {
        MEM32(0x500)=0x1122;MEM32(0x504)=0x3344;MEM32(0x508)=0x5566;MEM32(0x50c)=0x7788;MEM32(0x510)=0x9100;
        switch(setjmp(*recomp_setjmp_slot(0x500))) {
        case 0:(void)tsfp_io_startio(&scopes,&owner,&binding,body,&ops);++escaped;CHECK(0);break;
        case 7:break;
        default:CHECK(0);
        }
        CHECK(!escaped && g_esp==0x9104 && g_ebx==0x3344 && callbacks==1);
    } else {
        result=tsfp_io_startio(&scopes,&owner,&binding,mode==16?0:body,mode==15?NULL:&ops);
        CHECK(g_ebx==0x2222 && g_eax==0x1111 && g_seh_ebp==0x1234 && g_esp==(mode==9?0x9003u:0x9007u));
        if(mode<=1 || mode==14)CHECK(result==TSFP_IO_CALL_RETURNED);
        else if(mode==2 || mode==10)CHECK(result==TSFP_IO_CALL_UNCERTAIN);
        else CHECK(result==TSFP_IO_CALL_NOT_ENTERED);
    }
    CHECK(scopes.current==&outer);
    CHECK(!memcmp(ram+0x1000,before_packet,0x10));
    CHECK(!memcmp(ram+0x1018,before_packet+0x18,sizeof(before_packet)-0x18));
    if(mode==1 || mode==13)CHECK(disposed==1 && !owner.clients && !owner.internal);
    else {
        CHECK(!disposed && owner.clients==1 && queue.current==&owner);
        CHECK(owner.dispatch_abandoned==((mode==2 || mode==3 || mode==10)?1u:0u));
        if(mode==8 || mode==11)CHECK(owner.phase==TSFP_IO_COMPLETE && !owner.internal);
        else CHECK(owner.internal==1 && (owner.phase==TSFP_IO_ACTIVE || owner.phase==TSFP_IO_PENDING));
        CHECK(tsfp_io_owner_retain(&owner));CHECK(tsfp_io_queue_next(&queue,&owner,&empty) && !empty);
        CHECK(tsfp_io_owner_release(&owner));
        if(mode!=8 && mode!=11)CHECK(tsfp_io_owner_complete(&owner,0,17,NULL,NULL));
    }
    CHECK(disposed==1 && retired_count==1 && !owner.clients && !owner.internal);
    if(!callbacks)CHECK(tsfp_device_factory_delete(&factory,body));
    CHECK(!tsfp_guest_objects_collect(&objects) && frees==1);counts(0,0,0);
    CHECK(extra_released==(mode==10?1u:0u));CHECK(tsfp_guest_scope_leave(&scopes,&outer));tsfp_io_queue_stop(&queue);
}
int main(void) {
    unsigned which;CHECK(tsfp_guest_lifetime_hooks(capture_scopes,restore_scopes));
    for(which=0;which<=16;++which)scenario(which);
    CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: StartIo from active/pending FIFO ownership, void stdcall/unchanged slots, inline completion/next, rejection rollback, malformed return and actual native unwind");return 0;
}
