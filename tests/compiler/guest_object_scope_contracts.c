#include "guest_object_scope.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static tsfp_guest_objects store;
static tsfp_guest_handle_record handles[130];
static tsfp_guest_object_record objects[130];
static tsfp_guest_object_scopes stack;
static unsigned closes;
static uint32_t reenter_token;
static int close_host(void *context,uintptr_t host) {
    CHECK(context==&store && host);++closes;
    if(reenter_token) {
        tsfp_guest_object_scope nested;uint32_t token=reenter_token;reenter_token=0;
        CHECK(tsfp_guest_scope_enter(&stack,&nested,&store));
        CHECK(tsfp_guest_scope_borrow(&stack,token));
        CHECK(tsfp_guest_objects_close(&store,token));
        CHECK(tsfp_guest_scope_leave(&stack,&nested));
    }
    return 1;
}
int main(void) {
    tsfp_guest_object_scope outer,inner;
    const tsfp_guest_object_lease *first;
    uint32_t a,b,tokens[129];size_t i;
    CHECK(tsfp_guest_objects_init(&store,handles,objects,130,close_host,NULL,&store));
    CHECK(tsfp_guest_objects_adopt(&store,1,0,0,&a));
    CHECK(tsfp_guest_objects_adopt(&store,2,0,0,&b));
    CHECK(!tsfp_guest_scope_borrow(&stack,a));
    CHECK(tsfp_guest_scope_enter(&stack,&outer,&store));
    CHECK(!tsfp_guest_scope_enter(&stack,&outer,&store));
    first=tsfp_guest_scope_borrow(&stack,a);CHECK(first && first->host==1);
    CHECK(tsfp_guest_scope_borrow(&stack,a)==first && outer.count==1);
    CHECK(tsfp_guest_scope_enter(&stack,&inner,&store));
    CHECK(tsfp_guest_scope_borrow(&stack,a));CHECK(!tsfp_guest_scope_leave(&stack,&outer));
    CHECK(tsfp_guest_objects_close(&store,a));
    CHECK(tsfp_guest_scope_leave(&stack,&inner) && !closes && first->host==1);
    CHECK(tsfp_guest_scope_enter(&stack,&inner,&store));
    CHECK(!tsfp_guest_scope_borrow(&stack,a));
    CHECK(tsfp_guest_scope_leave(&stack,&inner));
    reenter_token=b;CHECK(tsfp_guest_scope_leave(&stack,&outer));
    CHECK(closes==2 && !stack.current);
    CHECK(tsfp_guest_objects_adopt(&store,3,0,0,&a));
    CHECK(tsfp_guest_objects_adopt(&store,4,0,0,&b));
    CHECK(tsfp_guest_scope_enter(&stack,&outer,&store));CHECK(tsfp_guest_scope_borrow(&stack,a));
    CHECK(tsfp_guest_scope_enter(&stack,&inner,&store));CHECK(tsfp_guest_scope_borrow(&stack,b));
    CHECK(tsfp_guest_objects_close(&store,a));CHECK(tsfp_guest_objects_close(&store,b));
    CHECK(tsfp_guest_scope_abort(&stack) && !stack.current && closes==4);
    CHECK(tsfp_guest_scope_abort(&stack) && closes==4);
    CHECK(tsfp_guest_scope_enter(&stack,&outer,&store));
    for(i=0;i<129;++i) {
        CHECK(tsfp_guest_objects_adopt(&store,i+1,0,0,&tokens[i]));
        if(i<128) CHECK(tsfp_guest_scope_borrow(&stack,tokens[i]));
        else CHECK(!tsfp_guest_scope_borrow(&stack,tokens[i]));
    }
    for(i=0;i<129;++i) CHECK(tsfp_guest_objects_close(&store,tokens[i]));
    CHECK(closes==5);CHECK(tsfp_guest_scope_leave(&stack,&outer));
    CHECK(closes==133 && !tsfp_guest_objects_collect(&store));
    {
        uint64_t ka,kb;const tsfp_guest_object_lease *la,*lb;
        CHECK(tsfp_guest_objects_adopt(&store,300,0,0,&a));
        CHECK(tsfp_guest_objects_adopt(&store,301,0,0,&b));
        CHECK(tsfp_guest_objects_reference(&store,a,&ka));CHECK(tsfp_guest_objects_reference(&store,b,&kb));
        CHECK(tsfp_guest_scope_enter(&stack,&outer,&store));
        la=tsfp_guest_scope_borrow_key(&stack,UINT32_MAX-1,ka);
        lb=tsfp_guest_scope_borrow_key(&stack,UINT32_MAX-1,kb);
        CHECK(la && lb && la!=lb && la->host==300 && lb->host==301 && outer.count==2);
        CHECK(tsfp_guest_scope_borrow_key(&stack,UINT32_MAX-1,ka)==la && outer.count==2);
        CHECK(tsfp_guest_objects_close(&store,a));CHECK(tsfp_guest_objects_close(&store,b));
        CHECK(tsfp_guest_objects_dereference(&store,ka));CHECK(tsfp_guest_objects_dereference(&store,kb));
        CHECK(closes==133);CHECK(tsfp_guest_scope_leave(&stack,&outer));CHECK(closes==135);
    }
    puts("PASS: nested callback borrowing, reentry, scope limit and thread-exit cleanup");return 0;
}
