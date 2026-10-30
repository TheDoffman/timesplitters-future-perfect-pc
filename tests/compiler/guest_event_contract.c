#include "guest_event.h"
#include "recomp_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"event line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static union {uint64_t align;unsigned char bytes[32];} storage;
static tsfp_event_view event;
static uint32_t ready,gate,wins;static unsigned notification;
static uint32_t iosb_status,iosb_information;
static void yield(void) {
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
}
static void primitives(void) {
    static const unsigned char literal[16]={1,0,4,0,1,0,0,0,8,0x50,0x34,0x12,8,0x50,0x34,0x12};
    unsigned type;unsigned char before[32];tsfp_event_view candidate,saved;unsigned char *p=storage.bytes;
    memset(p,0xa5,32);CHECK(tsfp_event_initialize(p+1,16,0x12345000,1,7));
    CHECK(!memcmp(p+1,literal,16) && p[0]==0xa5 && p[17]==0xa5);
    memset(&candidate,0xa5,sizeof(candidate));memcpy(&saved,&candidate,sizeof(candidate));
    CHECK(!tsfp_event_bind(&candidate,p+1,16,0x12345000) && !memcmp(&saved,&candidate,sizeof(candidate)));
    for(type=0;type<256;++type) {
        memset(p,0xa5,32);memcpy(before,p,32);
        CHECK(tsfp_event_initialize(p,16,0x1000,type,0)==(type<2));
        if(type>=2) {CHECK(!memcmp(before,p,32));continue;}
        CHECK(tsfp_event_bind(&event,p,16,0x1000));CHECK(tsfp_event_state(&event)==0);
        CHECK(tsfp_event_try_wait(&event)==0 && tsfp_event_set(&event)==0 && tsfp_event_set(&event)==1);
        CHECK(tsfp_event_try_wait(&event)==1 && tsfp_event_try_wait(&event)==(type?0:1));
        CHECK(tsfp_event_reset(&event)==(type?0:1) && tsfp_event_reset(&event)==0);
        CHECK(tsfp_event_set(&event)==0 && tsfp_event_state(&event)==1);
        CHECK(tsfp_atomic_exchange32(p+4,2)==1);
        CHECK(tsfp_event_set(&event)==-1 && tsfp_event_reset(&event)==-1 && tsfp_event_try_wait(&event)==-1);
        CHECK(tsfp_atomic_cas32(p+4,0,0)==2);memcpy(before,p,32);
        CHECK(!tsfp_event_bind(&candidate,p,16,0x1000) && !memcmp(before,p,32));
    }
    memset(p,0xa5,32);memcpy(before,p,32);
    CHECK(!tsfp_event_initialize(p,15,0x1000,0,1));CHECK(!tsfp_event_initialize(p,16,0x1001,0,1));
    CHECK(!tsfp_event_initialize(p,16,0xfffffff4,0,1));CHECK(!tsfp_event_initialize(p,16,0,0,1));
    CHECK(!memcmp(before,p,32));CHECK(tsfp_event_initialize(p,16,0xfffffff0,0,1));
    CHECK(tsfp_event_bind(&event,p,16,0xfffffff0));CHECK(tsfp_event_state(&event)==1);
    CHECK(tsfp_event_state(NULL)==-1 && tsfp_event_set(NULL)==-1 && tsfp_event_reset(NULL)==-1 && tsfp_event_try_wait(NULL)==-1);
}
#ifdef _WIN32
static DWORD WINAPI waiter(void *context) {
#else
static void *waiter(void *context) {
#endif
    int result;(void)context;tsfp_atomic_add32(&ready,1);
    while(!tsfp_atomic_cas32(&gate,0,0))yield();
    if(notification) {
        while((result=tsfp_event_try_wait(&event))==0)yield();
        CHECK(result==1 && iosb_status==0 && iosb_information==17);tsfp_atomic_add32(&wins,1);
    } else {result=tsfp_event_try_wait(&event);CHECK(result>=0);if(result)tsfp_atomic_add32(&wins,1);}
    return 0;
}
static void concurrent(unsigned manual) {
    unsigned i;notification=manual;ready=gate=wins=0;iosb_status=0x103;iosb_information=0;
#ifdef _WIN32
    HANDLE threads[8];
#else
    pthread_t threads[8];
#endif
    CHECK(tsfp_event_initialize(storage.bytes,16,0x1000,manual?0:1,manual?0:1));
    CHECK(tsfp_event_bind(&event,storage.bytes,16,0x1000));
    for(i=0;i<8;++i) {
#ifdef _WIN32
        threads[i]=CreateThread(NULL,0,waiter,NULL,0,NULL);CHECK(threads[i]);
#else
        CHECK(!pthread_create(&threads[i],NULL,waiter,NULL));
#endif
    }
    while(tsfp_atomic_cas32(&ready,0,0)!=8)yield();
    CHECK(tsfp_atomic_exchange32(&gate,1)==0);
    if(manual) {iosb_information=17;iosb_status=0;CHECK(tsfp_event_set(&event)==0);}
    for(i=0;i<8;++i) {
#ifdef _WIN32
        DWORD code;CHECK(WaitForSingleObject(threads[i],10000)==WAIT_OBJECT_0);
        CHECK(GetExitCodeThread(threads[i],&code) && !code);CHECK(CloseHandle(threads[i]));
#else
        CHECK(!pthread_join(threads[i],NULL));
#endif
    }
    CHECK(wins==(manual?8u:1u) && tsfp_event_state(&event)==(manual?1:0));
}
int main(void) {
    primitives();concurrent(0);concurrent(1);
    puts("PASS: literal event header/limits, invalid-state preservation, single auto-event consumer and notification IOSB publication to eight native waiters");return 0;
}
