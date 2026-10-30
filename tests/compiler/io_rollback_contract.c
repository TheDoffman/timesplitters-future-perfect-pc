#include "guest_io_scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"rollback line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static tsfp_io_owner owners[4];static unsigned char packets[4][256];static unsigned depth,disposed[4];
static tsfp_guest_objects objects;static tsfp_guest_handle_record handles[1];static tsfp_guest_object_record records[1];static tsfp_guest_object_scopes scopes;
static void lock(void *ctx) {(void)ctx;CHECK(!depth);++depth;}
static void unlock(void *ctx) {(void)ctx;CHECK(depth==1);--depth;}
static void dispose(void *ctx,uint32_t address,void *mapping,size_t bytes) {
    unsigned index=(unsigned)(uintptr_t)ctx;CHECK(index<4 && !depth && !disposed[index]);
    CHECK(address==0x1000+index*0x1000 && mapping==packets[index] && bytes==256);++disposed[index];
}
static int close_host(void *ctx,uintptr_t value) {(void)ctx;(void)value;CHECK(0);return -1;}
static void init(unsigned index) {
    tsfp_io_owner_ops ops={(void *)(uintptr_t)index,lock,unlock,dispose};
    CHECK(tsfp_io_owner_init(&owners[index],&ops,packets[index],256,0x1000+index*0x1000,256,2,6));
}
int main(void) {
    tsfp_io_scope frame,inner;tsfp_io_ticket ticket={0};unsigned char before[256];
    CHECK(tsfp_guest_objects_init(&objects,handles,records,1,close_host,NULL,NULL));
    init(0);memcpy(before,packets[0],256);
    CHECK(tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[0]));
    CHECK(tsfp_guest_scope_enter(&scopes,&inner.scope,&objects));
    CHECK(!tsfp_io_dispatch_scope_cancel(&scopes,&frame));CHECK(owners[0].internal==2);
    CHECK(tsfp_guest_scope_leave(&scopes,&inner.scope));CHECK(tsfp_io_dispatch_scope_cancel(&scopes,&frame));
    CHECK(!scopes.current && owners[0].phase==TSFP_IO_READY && !owners[0].internal && owners[0].clients==1);
    CHECK(!memcmp(before,packets[0],256) && !disposed[0]);CHECK(!tsfp_io_dispatch_scope_cancel(&scopes,&frame));
    CHECK(tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[0]));
    CHECK(tsfp_io_dispatch_scope_publish(&scopes,&frame));CHECK(!tsfp_io_dispatch_scope_publish(&scopes,&frame));
    CHECK(!tsfp_io_dispatch_scope_cancel(&scopes,&frame));CHECK(scopes.current==&frame.scope && owners[0].internal==2);
    CHECK(tsfp_io_owner_complete(&owners[0],0,1,NULL,NULL));CHECK(tsfp_io_scope_end(&scopes,&frame));CHECK(tsfp_io_owner_release(&owners[0]) && disposed[0]);
    init(1);CHECK(tsfp_io_owner_dispatch_begin(&owners[1],&ticket));CHECK(tsfp_io_owner_release(&owners[1]));
    CHECK(tsfp_io_owner_dispatch_rollback(&ticket));CHECK(disposed[1] && !ticket.owner && !ticket.kind);
    CHECK(!tsfp_io_owner_dispatch_rollback(&ticket));
    init(2);CHECK(tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[2]));CHECK(tsfp_io_owner_pending(&owners[2]));
    CHECK(!tsfp_io_dispatch_scope_cancel(&scopes,&frame));CHECK(!scopes.current && owners[2].phase==TSFP_IO_PENDING && owners[2].internal==1 && owners[2].dispatch_abandoned);
    CHECK(tsfp_io_owner_complete(&owners[2],0,2,NULL,NULL));CHECK(tsfp_io_owner_release(&owners[2]) && disposed[2]);
    init(3);CHECK(tsfp_io_owner_dispatch_begin(&owners[3],&ticket));CHECK(tsfp_io_owner_dispatch_publish(&ticket));
    CHECK(!tsfp_io_owner_dispatch_rollback(&ticket));CHECK(tsfp_io_owner_dispatch_end(&ticket,1));
    CHECK(owners[3].phase==TSFP_IO_PENDING && owners[3].internal==1);CHECK(tsfp_io_owner_complete(&owners[3],0,3,NULL,NULL));CHECK(tsfp_io_owner_release(&owners[3]) && disposed[3]);
    /* Forwarding shares the original operation pin. Cancellation preserves the
     * parent phase, including completion while a private child is prepared. */
    {
        unsigned pending;
        for(pending=0;pending<2;++pending) {
            disposed[0]=0;init(0);
            CHECK(tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[0]));
            CHECK(tsfp_io_dispatch_scope_publish(&scopes,&frame));
            if(pending)CHECK(tsfp_io_owner_pending(&owners[0]));
            CHECK(tsfp_io_forward_scope_begin(&scopes,&inner,&objects,&owners[0]));
            CHECK(owners[0].internal==3 && owners[0].clients==1);
            CHECK(tsfp_io_dispatch_scope_cancel(&scopes,&inner));
            CHECK(owners[0].internal==2 && owners[0].phase==(pending?TSFP_IO_PENDING:TSFP_IO_ACTIVE));
            CHECK(tsfp_io_forward_scope_begin(&scopes,&inner,&objects,&owners[0]));
            CHECK(tsfp_io_dispatch_scope_publish(&scopes,&inner));
            CHECK(tsfp_io_owner_complete(&owners[0],0,4,NULL,NULL));
            CHECK(tsfp_io_owner_release(&owners[0]) && !disposed[0]);
            CHECK(tsfp_io_scope_end(&scopes,&inner) && owners[0].internal==1 && !disposed[0]);
            CHECK(tsfp_io_scope_end(&scopes,&frame) && disposed[0]==1);
        }
        disposed[0]=0;init(0);
        CHECK(tsfp_io_dispatch_scope_begin(&scopes,&frame,&objects,&owners[0]));
        CHECK(tsfp_io_dispatch_scope_publish(&scopes,&frame));
        CHECK(tsfp_io_forward_scope_begin(&scopes,&inner,&objects,&owners[0]));
        CHECK(tsfp_io_owner_complete(&owners[0],0,5,NULL,NULL));
        CHECK(tsfp_io_dispatch_scope_cancel(&scopes,&inner));
        CHECK(owners[0].phase==TSFP_IO_COMPLETE && owners[0].internal==1);
        CHECK(tsfp_io_owner_release(&owners[0]) && !disposed[0]);
        CHECK(tsfp_io_scope_end(&scopes,&frame) && disposed[0]==1);
    }
    puts("PASS: private rollback, scoped layered dispatch, shared operation pin and completion during admission");return 0;
}
