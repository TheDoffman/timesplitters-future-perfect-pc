#define TSFP_REAL_DEVICE_BODY
#define main device_fixture_main
#include "guest_device_contract.c"
#undef main
#include "guest_io_call.h"
#include "guest_lifetime.h"
ptrdiff_t g_xbox_mem_offset;
static tsfp_io_owner request_owner;static tsfp_io_devices binding;static tsfp_io_device_entry device_entries[2];
static tsfp_guest_object_scopes scopes;static tsfp_guest_object_scope outer;
static tsfp_io_call_ops call_ops;
static unsigned char stack_snapshot[48];
static uint32_t first_body,second_body,status_out,child_status;
static unsigned mode,callbacks,lookups,request_depth,retired_count,disposed,alter_esp,alter_slot,finish_private,escaped,after_child;
static void request_lock(void *context) {CHECK(context==&request_owner && !request_depth);++request_depth;}
static void request_unlock(void *context) {
    CHECK(context==&request_owner && request_depth==1);--request_depth;
    if(request_owner.phase==TSFP_IO_ACTIVE && request_owner.internal==3) {
        if(alter_esp) {alter_esp=0;g_esp-=4;}
        if(alter_slot) {alter_slot=0;put(ram+0x105c,0);}
        if(finish_private) {finish_private=0;CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));}
    }
}
static void retired(void *context,tsfp_io_devices *owned,int clean) {
    CHECK(context==&request_owner && owned==&binding && clean && !request_depth && !retired_count && !disposed);++retired_count;
}
static void request_dispose(void *context,uint32_t address,void *mapping,size_t bytes) {
    CHECK(context==&request_owner && address==0x1000 && mapping==ram+0x1000 && bytes==256 && !request_depth && retired_count==1 && !disposed);++disposed;
}
static void driver_entry(void) {
    uint32_t sp=g_esp,device=MEM32(sp+4);int child_result;
    CHECK(!request_depth && !depth && MEM32(sp)==0 && MEM32(sp+8)==0x1000 && g_seh_ebp==sp);
    CHECK(!memcmp(ram+0x1068,stack_snapshot,48));
    ++callbacks;CHECK(!disposed && !frees);CHECK(tsfp_device_factory_delete(&factory,device));
    if(device==first_body) {
        CHECK(sp==0x8ffb && request_owner.internal==2 && request_owner.clients==1 && ram[0x1019]==2 && value(ram+0x1094)==0x22220000);
        g_ebx=0x9999;
        if(mode==9) {CHECK(tsfp_io_owner_pending(&request_owner));g_eax=0x103;g_esp=sp+12;return;}
        child_status=0xfeedface;
        if(mode==5)alter_esp=1;
        if(mode==6)alter_slot=1;
        if(mode==7)finish_private=1;
        CHECK(tsfp_io_owner_retain(&request_owner));
        child_result=tsfp_io_call_driver(&scopes,&request_owner,&binding,second_body,&call_ops,&child_status);++after_child;
        CHECK(request_owner.clients==1 && g_ebx==0x9999 && !disposed && !frees && binding.count==2);
        if(mode>=4 && mode<=7) {
            CHECK(child_result==TSFP_IO_CALL_NOT_ENTERED && child_status==0xfeedface && callbacks==1 && ram[0x1019]==2 && value(ram+0x107c)==0x11110000);
            if(mode==5) {CHECK(g_esp==sp-4);g_esp=sp;}
            else CHECK(g_esp==sp);
            if(mode==6) {CHECK(value(ram+0x105c)==0);put(ram+0x105c,0x1080);}
            else CHECK(value(ram+0x105c)==0x1080);
            if(mode==7)CHECK(request_owner.phase==TSFP_IO_COMPLETE && request_owner.internal==1);
            else {CHECK(request_owner.phase==TSFP_IO_ACTIVE && request_owner.internal==2);CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));}
            g_eax=0x77;
        } else {
            CHECK(g_esp==sp && callbacks==2 && ram[0x1019]==1 && value(ram+0x105c)==0x1068 && value(ram+0x107c)==0x11110000 && value(ram+0x1094)==0x22220000);
            if(mode==2) {CHECK(child_result==TSFP_IO_CALL_UNCERTAIN && child_status==0xfeedface && request_owner.internal==2);g_eax=0xc0000001;}
            else {CHECK(child_result==TSFP_IO_CALL_RETURNED && child_status==(mode==0?0x103u:0x77u));g_eax=child_status;}
        }
        g_esp=sp+12;
    } else {
        CHECK(device==second_body && sp==(mode==9?0x8ffbu:0x8fefu));
        CHECK(request_owner.clients==(mode==9?1u:2u) && request_owner.internal==(mode==9?2u:3u));
        CHECK(ram[0x1019]==1 && value(ram+0x105c)==0x1068 && value(ram+0x107c)==0x11110000);g_ebx=0xaaaa;
        if(mode==3) {(void)recomp_guest_longjmp(0x500,7);CHECK(0);}
        if(mode==8) {
            uint32_t extra_status=0xfeedface;CHECK(tsfp_io_owner_retain(&request_owner));
            CHECK(tsfp_io_call_driver(&scopes,&request_owner,&binding,first_body,&call_ops,&extra_status)==TSFP_IO_CALL_NOT_ENTERED);
            CHECK(extra_status==0xfeedface && request_owner.clients==2 && request_owner.internal==3 && binding.count==2);
        }
        if(mode==0 || mode==2) {CHECK(tsfp_io_owner_pending(&request_owner));g_eax=0x103;}
        else {CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));g_eax=0x77;}
        CHECK(!frees && !disposed);g_esp=sp+(mode==2?8:12);
    }
}
static tsfp_guest_entry_t lookup_driver(void *context,uint32_t address) {CHECK(context==&request_owner && !request_depth && !depth);++lookups;return address!=0x4444 || (mode==4 && lookups==2)?NULL:driver_entry;}
static void *callback_output(void *context,uint32_t address,uint32_t bytes) {CHECK(context==&request_owner && !request_depth && !depth && bytes==12);return address>sizeof(ram)-bytes?NULL:ram+address;}
static const void *driver_input(void *context,uint32_t address,uint32_t bytes) {CHECK(context==&request_owner && !request_depth && !depth && address==0x201 && bytes==68);return ram+address;}
static uint64_t capture_scopes(void) {return tsfp_guest_scope_mark(&scopes);}
static int restore_scopes(uint64_t mark) {return tsfp_guest_scope_unwind(&scopes,mark);}
static void scenario(unsigned which) {
    tsfp_io_owner_ops owner_ops={&request_owner,request_lock,request_unlock,request_dispose};int result;
    init();mode=which;callbacks=lookups=retired_count=disposed=alter_esp=alter_slot=finish_private=escaped=after_child=0;
    g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=0x9007;g_eax=0x1111;g_ebx=0x2222;g_seh_ebp=0x1234;
    CHECK(create(0,0)==TSFP_DEVICE_OK);first_body=value(ram+0x801);CHECK(create(0,0)==TSFP_DEVICE_OK);second_body=value(ram+0x801);CHECK(first_body!=second_body);
    memset(ram+0x201,0,68);put(ram+0x201+12+4*2,0x4444);
    CHECK(tsfp_io_owner_init(&request_owner,&owner_ops,ram+0x1000,256,0x1000,256,2,6));ram[0x1080]=ram[0x1068]=2;
    put(ram+0x107c,0x11110000);put(ram+0x1094,0x22220000);memcpy(stack_snapshot,ram+0x1068,48);
    CHECK(tsfp_io_devices_init(&binding,&request_owner,&factory,device_entries,2,retired,&request_owner));
    call_ops.callback.context=&request_owner;call_ops.callback.lookup=lookup_driver;call_ops.callback.output=callback_output;call_ops.context=&request_owner;call_ops.input=driver_input;
    status_out=0xfeedface;CHECK(tsfp_guest_scope_enter(&scopes,&outer,&objects));
    if(mode==3) {
        MEM32(0x500)=0x1122;MEM32(0x504)=0x3344;MEM32(0x508)=0x5566;MEM32(0x50c)=0x7788;MEM32(0x510)=0x9100;
        switch(setjmp(*recomp_setjmp_slot(0x500))) {
        case 0:(void)tsfp_io_call_driver(&scopes,&request_owner,&binding,first_body,&call_ops,&status_out);++escaped;CHECK(0);break;
        case 7:break;
        default:CHECK(0);
        }
        CHECK(!escaped && !after_child && status_out==0xfeedface && g_ebx==0x3344 && g_esp==0x9104);
    } else {
        result=tsfp_io_call_driver(&scopes,&request_owner,&binding,first_body,&call_ops,&status_out);
        CHECK(result==TSFP_IO_CALL_RETURNED && g_esp==0x9007 && g_ebx==0x2222 && g_eax==0x1111 && g_seh_ebp==0x1234);
        CHECK(status_out==((mode==0 || mode==9)?0x103u:mode==2?0xc0000001u:0x77u));
        if(mode==9) {
            CHECK(request_owner.phase==TSFP_IO_PENDING && request_owner.internal==1 && !request_owner.clients && binding.count==1 && !disposed && !frees);
            CHECK(tsfp_io_owner_retain(&request_owner));status_out=0xfeedface;
            CHECK(tsfp_io_call_driver(&scopes,&request_owner,&binding,second_body,&call_ops,&status_out)==TSFP_IO_CALL_RETURNED && status_out==0x77);
        } else CHECK(after_child==1);
    }
    CHECK(scopes.current==&outer);
    if(mode==0 || mode==2 || mode==3) {
        CHECK(!disposed && !retired_count && !request_owner.clients && request_owner.internal==1 && request_owner.phase==TSFP_IO_PENDING && !frees);
        CHECK(request_owner.dispatch_abandoned==(mode==0?0u:1u));CHECK(tsfp_io_owner_complete(&request_owner,0,17,NULL,NULL));
    }
    CHECK(disposed==1 && retired_count==1 && !request_owner.clients && !request_owner.internal);
    if(mode>=4 && mode<=7) {CHECK(frees==1);CHECK(tsfp_device_factory_delete(&factory,second_body));}
    CHECK(!tsfp_guest_objects_collect(&objects));counts(0,0,0);CHECK(frees==2 && !memory.heap.count);
    CHECK(callbacks==((mode>=4 && mode<=7)?1u:2u));CHECK(tsfp_guest_scope_leave(&scopes,&outer));
}
int main(void) {
    unsigned which;CHECK(tsfp_guest_lifetime_hooks(capture_scopes,restore_scopes));
    for(which=0;which<=9;++which)scenario(which);
    CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: layered/asynchronous driver calls share one operation, preserve parent state, restore rejected slots and retain all device leases through native jump");return 0;
}
