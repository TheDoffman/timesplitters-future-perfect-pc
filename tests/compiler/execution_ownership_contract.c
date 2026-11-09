/* Original transition/order and real native guest-jump ownership contracts. */
#include "guest_execution_scope.h"
#include "recomp_types.h"
#include "guest_lifetime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define TEST_TLS __declspec(thread)
#else
#include <pthread.h>
#define TEST_TLS _Thread_local
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"execution line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct mock_state {unsigned locks,active,length;int enabled;char trace[512];} mock_state;
static TEST_TLS mock_state machine;
static void event(char value){CHECK(machine.length<sizeof machine.trace-1);machine.trace[machine.length++]=value;machine.trace[machine.length]=0;}
static int enabled(void *p){(void)p;return machine.enabled;}
static void lock(void *p){(void)p;event('L');++machine.locks;}
static void unlock(void *p){(void)p;CHECK(machine.locks);event('U');--machine.locks;}
static void activate(void *p){(void)p;CHECK(!machine.active);event('A');machine.active=1;}
static void deactivate(void *p){(void)p;CHECK(machine.active);event('D');machine.active=0;}
static const tsfp_guest_execution_ops ops={NULL,enabled,lock,unlock,activate,deactivate};
static const tsfp_guest_execution_ops other={NULL,enabled,lock,unlock,activate,deactivate};
static tsfp_guest_execution_snapshot zero,main_snapshot;
static tsfp_guest_objects objects;
static tsfp_guest_handle_record handles[8];
static tsfp_guest_object_record records[8];
static tsfp_guest_object_scopes scopes;
static tsfp_guest_object_scope outer,bad;
static tsfp_guest_execution_scope first,second;
static unsigned escaped;
static union {uint64_t alignment;unsigned char bytes[4096];} ram;
ptrdiff_t g_xbox_mem_offset;
static void clear_trace(void){machine.length=0;machine.trace[0]=0;}
static tsfp_guest_execution_snapshot capture(void){tsfp_guest_execution_snapshot s;CHECK(tsfp_guest_execution_capture(&ops,&s));return s;}
static void state(unsigned locks,unsigned active){tsfp_guest_execution_snapshot s=capture();CHECK(s.lock_depth==locks && s.active_depth==active);CHECK(machine.locks==locks && machine.active==(unsigned)!!active);}
static void restore(unsigned locks,unsigned active){tsfp_guest_execution_snapshot s=capture();s.lock_depth=locks;s.active_depth=active;CHECK(tsfp_guest_execution_restore(&ops,&s));state(locks,active);}
static int close_host(void *p,uintptr_t resource){(void)p;CHECK(resource==11 || resource==22);CHECK(!machine.active && !machine.locks);event('C');return 1;}
static void lease(tsfp_guest_execution_scope *f,uintptr_t host){uint32_t token;CHECK(tsfp_guest_objects_adopt(&objects,host,1,0,&token));CHECK(scopes.current==&f->objects);CHECK(tsfp_guest_scope_borrow(&scopes,token));CHECK(tsfp_guest_objects_close(&objects,token));}
static uint64_t lifetime_capture(void){return tsfp_guest_scope_mark(&scopes);}
static int lifetime_restore(uint64_t mark){return tsfp_guest_execution_scope_unwind(&scopes,mark,&ops);}
static int fail_cleanup(void *p,int abandoned){(void)p;CHECK(abandoned && !machine.active && !machine.locks);event('F');return 0;}

static void exact_transitions(void){
    tsfp_guest_execution_snapshot saved,unchanged,query;tsfp_guest_execution_ops invalid=ops;
    machine.enabled=1;CHECK(tsfp_guest_execution_capture(&ops,&zero));
    CHECK(tsfp_guest_execution_capture(&other,&query)); /* Query must not bind. */
    invalid.unlock=NULL;memset(&query,0xcc,sizeof query);unchanged=query;
    CHECK(!tsfp_guest_execution_capture(&invalid,&query) && !memcmp(&query,&unchanged,sizeof query));
    CHECK(tsfp_guest_execution_lock_enter(&ops));CHECK(tsfp_guest_execution_lock_enter(&ops));
    CHECK(tsfp_guest_execution_enter(&ops));CHECK(tsfp_guest_execution_enter(&ops));CHECK(tsfp_guest_execution_enter(&ops));
    state(2,3);saved=capture();CHECK(!strcmp(machine.trace,"LLA"));
    clear_trace();CHECK(tsfp_guest_execution_suspend(&ops,&query));CHECK(!strcmp(machine.trace,"DUU"));state(0,0);
    CHECK(!memcmp(&query,&saved,sizeof saved));
    clear_trace();CHECK(tsfp_guest_execution_restore(&ops,&saved));CHECK(!strcmp(machine.trace,"LLA"));state(2,3);
    clear_trace();CHECK(tsfp_guest_execution_restore(&ops,&saved));CHECK(!machine.length);
    CHECK(!tsfp_guest_execution_capture(&other,&query));CHECK(!tsfp_guest_execution_restore(&other,&saved));
    CHECK(!machine.length);CHECK(tsfp_guest_execution_restore(&ops,&zero));
    clear_trace();CHECK(tsfp_guest_execution_leave(&ops));CHECK(tsfp_guest_execution_lock_leave(&ops));CHECK(!machine.length);
    CHECK(tsfp_guest_execution_lock_enter(&ops));saved=capture();CHECK(tsfp_guest_execution_lock_drop(&ops)==1);
    clear_trace();machine.enabled=0;CHECK(!tsfp_guest_execution_restore(&ops,&saved));CHECK(!machine.length);state(0,0);
    CHECK(tsfp_guest_execution_lock_enter(&ops));CHECK(!tsfp_guest_execution_lock_restore(&ops,1));state(0,0);
    machine.enabled=1;CHECK(tsfp_guest_execution_restore(&ops,&saved));CHECK(tsfp_guest_execution_restore(&ops,&zero));
    saved=capture();saved.owner^=1;clear_trace();CHECK(!tsfp_guest_execution_restore(&ops,&saved));CHECK(!machine.length);
    saved=capture();saved.active_depth=UINT32_MAX;CHECK(!tsfp_guest_execution_restore(&ops,&saved));CHECK(!machine.length);
}

static void transition_matrix(void){
    unsigned before_lock,before_active,to_lock,to_active;
    for(before_lock=0;before_lock<4;++before_lock)for(before_active=0;before_active<4;++before_active)
    for(to_lock=0;to_lock<4;++to_lock)for(to_active=0;to_active<4;++to_active){
        char expected[32];unsigned n=0,i;int parked=before_active && (before_lock!=to_lock || !to_active);
        restore(before_lock,before_active);clear_trace();
        if(parked)expected[n++]='D';
        for(i=to_lock;i<before_lock;++i)expected[n++]='U';
        for(i=before_lock;i<to_lock;++i)expected[n++]='L';
        if(to_active && (!before_active || parked))expected[n++]='A';expected[n]=0;
        restore(to_lock,to_active);CHECK(!strcmp(machine.trace,expected));
    }
    CHECK(tsfp_guest_execution_restore(&ops,&zero));
}
static void normal_scope(void){
    unsigned char before[sizeof first];uint64_t mark;
    restore(2,3);clear_trace();CHECK(tsfp_guest_execution_scope_enter(&scopes,&first,&objects,&ops));
    CHECK(!strcmp(machine.trace,"DUU"));state(0,0);lease(&first,11);mark=tsfp_guest_scope_mark(&scopes);
    memcpy(before,&first,sizeof first);clear_trace();
    CHECK(!tsfp_guest_execution_scope_enter(&scopes,&first,&objects,&ops));CHECK(!memcmp(before,&first,sizeof first));
    CHECK(!tsfp_guest_execution_scope_unwind(&scopes,mark+1,&ops));CHECK(!machine.length);state(0,0);
    CHECK(tsfp_guest_execution_scope_leave(&scopes,&first));CHECK(!strcmp(machine.trace,"CLLA"));state(2,3);
    clear_trace();CHECK(!tsfp_guest_execution_scope_leave(&scopes,&first));CHECK(!machine.length);
    CHECK(tsfp_guest_execution_restore(&ops,&zero));
}
static void nested_jump(void){
    CHECK(tsfp_guest_execution_scope_enter(&scopes,&first,&objects,&ops));lease(&first,11);
    CHECK(tsfp_guest_execution_lock_enter(&ops));CHECK(tsfp_guest_execution_enter(&ops));
    CHECK(tsfp_guest_execution_scope_enter(&scopes,&second,&objects,&ops));lease(&second,22);
    clear_trace();recomp_guest_longjmp(0x100,7);escaped=1;
}
static void native_abandonment(void){
    uint64_t mark;
    restore(2,3);CHECK(tsfp_guest_scope_enter(&scopes,&outer,&objects));mark=tsfp_guest_scope_mark(&scopes);
    CHECK(tsfp_guest_lifetime_hooks(lifetime_capture,lifetime_restore));
    MEM32(0x100)=0x1234;MEM32(0x104)=3;MEM32(0x108)=5;MEM32(0x10c)=7;MEM32(0x110)=1000;
    switch(setjmp(*recomp_setjmp_slot(0x100))){case 0:nested_jump();CHECK(0);break;case 7:break;default:CHECK(0);}
    CHECK(!escaped && scopes.current==&outer && tsfp_guest_scope_mark(&scopes)==mark);
    CHECK(!strcmp(machine.trace,"CCLLA"));state(2,3); /* All leases before reactivation. */
    CHECK(g_ebp==0x1234 && g_ebx==3 && g_edi==5 && g_esi==7 && g_esp==1004);
    CHECK(tsfp_guest_execution_scope_enter(&scopes,&first,&objects,&ops));lease(&first,11);
    CHECK(tsfp_guest_scope_enter(&scopes,&bad,&objects));CHECK(tsfp_guest_scope_on_leave(&scopes,&bad,fail_cleanup,NULL));
    clear_trace();CHECK(!tsfp_guest_execution_scope_unwind(&scopes,mark,&ops));
    CHECK(!strcmp(machine.trace,"FCLLA"));state(2,3);CHECK(scopes.current==&outer);
    CHECK(tsfp_guest_scope_leave(&scopes,&outer));CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    CHECK(tsfp_guest_execution_restore(&ops,&zero));
}
static void worker_body(unsigned *result){
    unsigned i;tsfp_guest_execution_snapshot own,empty;
    machine.enabled=1;CHECK(!tsfp_guest_execution_restore(&ops,&main_snapshot));CHECK(!machine.length);
    CHECK(tsfp_guest_execution_capture(&ops,&empty));
    for(i=0;i<64;++i){
        clear_trace();CHECK(tsfp_guest_execution_lock_enter(&ops));CHECK(tsfp_guest_execution_enter(&ops));
        own=capture();CHECK(own.owner!=main_snapshot.owner);state(1,1);
        CHECK(tsfp_guest_execution_suspend(&ops,&own));state(0,0);
        CHECK(tsfp_guest_execution_restore(&ops,&own));state(1,1);
        CHECK(tsfp_guest_execution_restore(&ops,&empty));state(0,0);
    }
    *result=1;
}
static void exit_without_reactivation(void){
    restore(2,3);CHECK(tsfp_guest_execution_scope_enter(&scopes,&first,&objects,&ops));lease(&first,11);
    CHECK(tsfp_guest_execution_lock_enter(&ops));CHECK(tsfp_guest_execution_enter(&ops));
    CHECK(tsfp_guest_execution_scope_enter(&scopes,&second,&objects,&ops));lease(&second,22);
    clear_trace();machine.enabled=0;
    CHECK(tsfp_guest_execution_scope_abort(&scopes,&ops));CHECK(!strcmp(machine.trace,"CC"));
    state(0,0);CHECK(!scopes.current);machine.enabled=1;
    restore(2,3);clear_trace();CHECK(tsfp_guest_execution_scope_abort(&scopes,&ops));
    CHECK(!strcmp(machine.trace,"DUU"));state(0,0);
}
#ifdef _WIN32
static DWORD WINAPI worker(void *p){worker_body(p);return 0;}
#else
static void *worker(void *p){worker_body(p);return NULL;}
#endif
static void threads(void){
    unsigned result[4]={0},i;
#ifdef _WIN32
    HANDLE handles_[4];
#else
    pthread_t handles_[4];
#endif
    restore(2,3);main_snapshot=capture();
    for(i=0;i<4;++i){
#ifdef _WIN32
        handles_[i]=CreateThread(NULL,0,worker,&result[i],0,NULL);CHECK(handles_[i]);
#else
        CHECK(!pthread_create(&handles_[i],NULL,worker,&result[i]));
#endif
    }
    for(i=0;i<4;++i){
#ifdef _WIN32
        CHECK(WaitForSingleObject(handles_[i],10000)==WAIT_OBJECT_0);CHECK(CloseHandle(handles_[i]));
#else
        CHECK(!pthread_join(handles_[i],NULL));
#endif
        CHECK(result[i]);
    }
    state(2,3);CHECK(tsfp_guest_execution_restore(&ops,&zero));
}
int main(void){
    g_xbox_mem_offset=(ptrdiff_t)ram.bytes;
    CHECK(tsfp_guest_objects_init(&objects,handles,records,8,close_host,NULL,NULL));
    exact_transitions();transition_matrix();normal_scope();native_abandonment();exit_without_reactivation();threads();
    puts("Exact execution ownership, deferred lease cleanup, actual guest jump and thread isolation passed");return 0;
}
