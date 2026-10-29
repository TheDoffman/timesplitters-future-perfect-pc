#include "guest_io_scope.h"
#include "recomp_types.h"
#include "guest_lifetime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"io scope line%d %s\n",__LINE__,#c);exit(1);}}while(0)
ptrdiff_t g_xbox_mem_offset;RECOMP_TLS uint32_t g_ebx,g_edi,g_esi,g_esp,g_ebp,g_seh_ebp;
static unsigned char jump_ram[4096],bytes[3][256];
static tsfp_io_owner owners[3];
static struct {unsigned depth,disposed;} contexts[3];
static tsfp_guest_objects objects;static tsfp_guest_handle_record handles[1];static tsfp_guest_object_record records[1];
static tsfp_guest_object_scopes scopes;static tsfp_guest_object_scope outer;
static unsigned escaped;
static void lock(void *ctx) {unsigned i=(unsigned)((char *)ctx-(char *)contexts)/sizeof(contexts[0]);CHECK(i<3 && !contexts[i].depth);contexts[i].depth=1;}
static void unlock(void *ctx) {unsigned i=(unsigned)((char *)ctx-(char *)contexts)/sizeof(contexts[0]);CHECK(i<3 && contexts[i].depth==1);contexts[i].depth=0;}
static void dispose(void *ctx,uint32_t address,void *memory,size_t size) {
    unsigned i=(unsigned)((char *)ctx-(char *)contexts)/sizeof(contexts[0]);
    CHECK(i<3 && !contexts[i].depth && !contexts[i].disposed && address==0x2000+i*0x1000 && memory==bytes[i] && size==256);++contexts[i].disposed;
}
static void init(unsigned i) {
    tsfp_io_owner_ops ops={&contexts[i],lock,unlock,dispose};
    CHECK(tsfp_io_owner_init(&owners[i],&ops,bytes[i],256,0x2000+i*0x1000,TSFP_IO_REQUEST_BYTES+2*TSFP_IO_STACK_BYTES,2,6));
}
static int close_host(void *ctx,uintptr_t host) {(void)ctx;(void)host;CHECK(0);return -1;}
static uint64_t capture(void) {return tsfp_guest_scope_mark(&scopes);}
static int restore(uint64_t mark) {return tsfp_guest_scope_unwind(&scopes,mark);}
static void dispatch_jump(void) {
    tsfp_io_scope frame;
    CHECK(tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[1]));
    CHECK(tsfp_io_owner_release(&owners[1]) && !contexts[1].disposed);
    CHECK(owners[1].internal==2);CHECK(tsfp_io_dispatch_scope_publish(&scopes,&frame));(void)recomp_guest_longjmp(0x100,7);++escaped;
}
static void completion_jump(void) {
    tsfp_io_scope frame;
    CHECK(tsfp_io_completion_scope_begin(&scopes,&frame,&objects,&owners[2],0xabcdef01,17));
    CHECK(tsfp_io_owner_release(&owners[2]) && !contexts[2].disposed);
    (void)recomp_guest_longjmp(0x200,11);++escaped;
}
int main(void) {
    tsfp_io_scope frame;tsfp_io_ticket ticket={0};uint64_t mark;
    CHECK(tsfp_guest_objects_init(&objects,handles,records,1,close_host,NULL,NULL));
    g_xbox_mem_offset=(ptrdiff_t)jump_ram;CHECK(tsfp_guest_lifetime_hooks(capture,restore));
    init(0);CHECK(tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[0]));
    CHECK(!tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[0]));
    CHECK(tsfp_io_owner_release(&owners[0]));CHECK(tsfp_io_dispatch_scope_publish(&scopes,&frame));CHECK(tsfp_io_owner_complete(&owners[0],0,9,NULL,NULL));CHECK(!contexts[0].disposed);
    CHECK(tsfp_io_scope_end(&scopes,&frame));CHECK(contexts[0].disposed==1 && !frame.active && !frame.ticket.owner);
    CHECK(!tsfp_io_scope_end(&scopes,&frame));
    CHECK(tsfp_guest_scope_enter(&scopes,&outer,&objects));mark=tsfp_guest_scope_mark(&scopes);init(1);
    switch(setjmp(*recomp_setjmp_slot(0x100))) {
    case 0:dispatch_jump();CHECK(0);break;
    case 7:break;
    default:CHECK(0);
    }
    CHECK(!escaped && scopes.current==&outer && tsfp_guest_scope_mark(&scopes)==mark);
    CHECK(owners[1].phase==TSFP_IO_PENDING && owners[1].internal==1 && owners[1].dispatch_abandoned && !contexts[1].disposed);
    CHECK(tsfp_io_owner_complete(&owners[1],0,19,NULL,NULL));CHECK(contexts[1].disposed==1);
    init(2);CHECK(tsfp_io_owner_dispatch_begin(&owners[2],&ticket));CHECK(tsfp_io_owner_dispatch_end(&ticket,0));
    CHECK(!tsfp_io_owner_dispatch_end(&ticket,0));
    switch(setjmp(*recomp_setjmp_slot(0x200))) {
    case 0:completion_jump();CHECK(0);break;
    case 11:break;
    default:CHECK(0);
    }
    CHECK(!escaped && scopes.current==&outer && owners[2].phase==TSFP_IO_UNCERTAIN && owners[2].internal==1 && !owners[2].clients && !contexts[2].disposed);
    CHECK(!tsfp_io_owner_complete(&owners[2],0,0,NULL,NULL));
    CHECK(bytes[2][0x10]==1 && bytes[2][0x11]==0xef && bytes[2][0x14]==17);
    CHECK(tsfp_guest_scope_leave(&scopes,&outer));CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: scoped dispatch/inline completion, native guest jump pin release and uncertain completion quarantine");return 0;
}
