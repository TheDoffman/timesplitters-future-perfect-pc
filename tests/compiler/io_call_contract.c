#define TSFP_REAL_DEVICE_BODY
#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_call.h"
#include "guest_lifetime.h"
ptrdiff_t g_xbox_mem_offset;
static tsfp_io_owner request_owner;static tsfp_io_devices binding;static tsfp_io_device_entry device_entries[2];
static tsfp_guest_object_scopes scopes;static tsfp_guest_object_scope outer,extra;
static unsigned request_depth,retired_count,disposed,callbacks,mode,invalidate_slot,change_esp,reject_admission,extra_released,escaped;
static uint32_t body,status_out;static unsigned char before_packet[256];
static void request_lock(void *context) {
    CHECK(context==&request_owner && !request_depth);++request_depth;
    if(reject_admission && request_owner.phase==TSFP_IO_ACTIVE) {reject_admission=0;request_owner.phase=TSFP_IO_PENDING;}
}
static void request_unlock(void *context) {
    CHECK(context==&request_owner && request_depth==1);--request_depth;
    if(request_owner.phase==TSFP_IO_ACTIVE) {
        if(invalidate_slot) {invalidate_slot=0;put(ram+0x1000+0x5c,0);}
        if(change_esp) {change_esp=0;g_esp-=4;}
    }
}
static void retired(void *context,tsfp_io_devices *value,int clean) {
    CHECK(context==&request_owner && value==&binding && clean && !request_depth && !retired_count && !disposed);++retired_count;
}
static void request_dispose(void *context,uint32_t address,void *mapping,size_t bytes) {
    CHECK(context==&request_owner && address==0x1000 && mapping==ram+0x1000 && bytes==256);
    CHECK(!request_depth && retired_count==1 && !disposed);++disposed;
}
static int release_extra(void *context,int abandoned) {CHECK(context==&extra && abandoned && !extra_released);++extra_released;return 1;}
static void driver_entry(void) {
    uint32_t sp=g_esp;
    CHECK(!request_depth && !depth && sp==0x8ffb && g_seh_ebp==sp && MEM32(sp)==0);
    CHECK(MEM32(sp+4)==body && MEM32(sp+8)==0x1000 && ram[0x1019]==2 && value(ram+0x105c)==0x1080);
    CHECK(!memcmp(ram+0x1068,before_packet+0x68,48));
    CHECK(request_owner.clients==1 && request_owner.internal==2);++callbacks;
    CHECK(tsfp_device_factory_delete(&factory,body));CHECK(!tsfp_guest_objects_collect(&objects) && !frees);
    g_ebx=0x9999;
    if(mode==3) {(void)recomp_guest_longjmp(0x500,7);CHECK(0);}
    if(mode==1) {CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));CHECK(!disposed && !frees);g_eax=0;}
    else {CHECK(tsfp_io_owner_pending(&request_owner));g_eax=0x103;}
    if(mode==10) {CHECK(tsfp_guest_scope_enter(&scopes,&extra,&objects));CHECK(tsfp_guest_scope_on_leave(&scopes,&extra,release_extra,&extra));}
    g_esp=sp+(mode==2?8:12);
}
static tsfp_guest_entry_t lookup_driver(void *context,uint32_t address) {
    CHECK(context==&request_owner && !request_depth && !depth);return mode==4 || address!=0x4444?NULL:driver_entry;
}
static void *callback_output(void *context,uint32_t address,uint32_t bytes) {
    CHECK(context==&request_owner && !request_depth && !depth && bytes==12);
    return mode==5 || address>sizeof(ram)-bytes?NULL:ram+address;
}
static const void *driver_input(void *context,uint32_t address,uint32_t bytes) {
    CHECK(context==&request_owner && !request_depth && !depth && bytes==68);return address==0x201?ram+address:NULL;
}
static uint64_t capture_scopes(void) {return tsfp_guest_scope_mark(&scopes);}
static int restore_scopes(uint64_t mark) {return tsfp_guest_scope_unwind(&scopes,mark);}
static void scenario(unsigned which) {
    tsfp_io_owner_ops owner_ops={&request_owner,request_lock,request_unlock,request_dispose};
    tsfp_io_call_ops call_ops={{&request_owner,lookup_driver,callback_output},&request_owner,driver_input};
    int result;
    init();mode=which;retired_count=disposed=callbacks=invalidate_slot=change_esp=reject_admission=extra_released=escaped=0;
    g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x9007;g_seh_ebp=0x1234;g_eax=0x1111;g_ebx=0x2222;
    CHECK(create(0,0)==TSFP_DEVICE_OK);body=value(ram+0x801);memset(ram+0x201,0,68);put(ram+0x201+12+4*2,0x4444);
    CHECK(tsfp_io_owner_init(&request_owner,&owner_ops,ram+0x1000,256,0x1000,256,2,6));ram[0x1080]=2;
    CHECK(tsfp_io_devices_init(&binding,&request_owner,&factory,device_entries,2,retired,&request_owner));
    if(mode==6)put(ram+0x105c,0);
    if(mode==7)invalidate_slot=1;
    if(mode==8)change_esp=1;
    if(mode==9)reject_admission=1;
    memcpy(before_packet,ram+0x1000,256);status_out=0xfeedface;
    CHECK(tsfp_guest_scope_enter(&scopes,&outer,&objects));
    if(mode==3) {
        MEM32(0x500)=0x1122;MEM32(0x504)=0x3344;MEM32(0x508)=0x5566;MEM32(0x50c)=0x7788;MEM32(0x510)=0x9100;
        switch(setjmp(*recomp_setjmp_slot(0x500))) {
        case 0:(void)tsfp_io_call_driver(&scopes,&request_owner,&binding,body,&call_ops,&status_out);++escaped;CHECK(0);break;
        case 7:break;
        default:CHECK(0);
        }
        CHECK(!escaped && g_esp==0x9104 && g_ebx==0x3344 && status_out==0xfeedface && callbacks==1);
    } else {
        result=tsfp_io_call_driver(&scopes,&request_owner,&binding,body,&call_ops,&status_out);
        CHECK(g_ebx==0x2222 && g_eax==0x1111 && g_seh_ebp==0x1234 && g_esp==(mode==8?0x9003u:0x9007u));
        if(mode<=1)CHECK(result==TSFP_IO_CALL_RETURNED && status_out==(mode==1?0u:0x103u));
        else if(mode==2 || mode==9 || mode==10)CHECK(result==TSFP_IO_CALL_UNCERTAIN && status_out==0xfeedface);
        else CHECK(result==TSFP_IO_CALL_NOT_ENTERED && status_out==0xfeedface);
    }
    CHECK(scopes.current==&outer);
    if(mode==0 || mode==2 || mode==3 || mode==9 || mode==10) {
        CHECK(!disposed && !retired_count && !request_owner.clients && request_owner.internal==1 && request_owner.phase==TSFP_IO_PENDING);
        CHECK(request_owner.dispatch_abandoned==(mode==0?0u:1u));
        if(mode==9) {CHECK(!callbacks && tsfp_device_factory_delete(&factory,body));CHECK(!memcmp(before_packet,ram+0x1000,256));}
        else CHECK(callbacks==1);
        CHECK(!tsfp_guest_objects_collect(&objects) && !frees);
        CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));
    } else if(mode>=4) {
        CHECK(!callbacks && disposed==1 && !request_owner.internal);
        if(mode!=7)CHECK(!memcmp(before_packet,ram+0x1000,256));
        CHECK(tsfp_device_factory_delete(&factory,body));
    }
    CHECK(disposed==1 && retired_count==1 && !request_owner.clients && !request_owner.internal);
    CHECK(!tsfp_guest_objects_collect(&objects) && frees==1);counts(0,0,0);
    CHECK(extra_released==(mode==10?1u:0u));CHECK(tsfp_guest_scope_leave(&scopes,&outer));
}
int main(void) {
    unsigned which;CHECK(tsfp_guest_lifetime_hooks(capture_scopes,restore_scopes));
    for(which=0;which<=10;++which)scenario(which);
    CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: initial driver calls, inline/pending completion, malformed return/native jump, rejected preflight, slot rollback and exact client/device cleanup");return 0;
}
