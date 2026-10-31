#include "recomp_types.h"
#include "guest_lifetime.h"
#include "guest_object_scope.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_ebx,g_edi,g_esi,g_esp,g_ebp,g_seh_ebp;
static unsigned char memory[4096];
static tsfp_guest_objects store;
static tsfp_guest_handle_record handles[4];
static tsfp_guest_object_record objects[4];
static tsfp_guest_object_scopes scopes;
static tsfp_guest_object_scope outer;
static unsigned closed[3],escaped;
static uint64_t outer_mark,retired_mark;
static int close_host(void *context,uintptr_t host) {
    CHECK(context==&store && host>0 && host<3 && !closed[host]);++closed[host];return 1;
}
static uint64_t capture(void) { return tsfp_guest_scope_mark(&scopes); }
static int restore(uint64_t mark) { return tsfp_guest_scope_unwind(&scopes,mark); }
static void callback(void) {
    tsfp_guest_object_scope inner;uint32_t token;
    CHECK(tsfp_guest_scope_enter(&scopes,&inner,&store));
    CHECK(tsfp_guest_objects_adopt(&store,2,0,0,&token));
    CHECK(tsfp_guest_scope_borrow(&scopes,token));
    CHECK(tsfp_guest_objects_close(&store,token));CHECK(!closed[2]);
    retired_mark=tsfp_guest_scope_mark(&scopes);
    CHECK(retired_mark!=outer_mark);
    CHECK(!tsfp_guest_scope_unwind(&scopes,retired_mark+1));
    CHECK(tsfp_guest_scope_mark(&scopes)==retired_mark && !closed[2]);
    (void)recomp_guest_longjmp(128,7);++escaped;
}
int main(void) {
    uint32_t token;
    g_xbox_mem_offset=(ptrdiff_t)memory;
    CHECK(tsfp_guest_objects_init(&store,handles,objects,4,close_host,NULL,&store));
    CHECK(!tsfp_guest_lifetime_hooks(capture,NULL));
    CHECK(tsfp_guest_lifetime_hooks(capture,restore));
    CHECK(tsfp_guest_scope_enter(&scopes,&outer,&store));
    CHECK(tsfp_guest_objects_adopt(&store,1,0,0,&token));
    CHECK(tsfp_guest_scope_borrow(&scopes,token));
    CHECK(tsfp_guest_objects_close(&store,token));outer_mark=tsfp_guest_scope_mark(&scopes);
    switch(setjmp(*recomp_setjmp_slot(128))) {
    case 0:callback();CHECK(0);break;
    case 7:break;
    default:CHECK(0);
    }
    CHECK(!escaped && closed[2]==1 && !closed[1]);
    CHECK(scopes.current==&outer && tsfp_guest_scope_mark(&scopes)==outer_mark);
    CHECK(!tsfp_guest_scope_unwind(&scopes,retired_mark));
    CHECK(tsfp_guest_scope_abort(&scopes) && closed[1]==1);
    CHECK(!tsfp_guest_objects_collect(&store));
    puts("PASS: real native guest longjmp releases abandoned scopes and preserves target ownership");
    return 0;
}
