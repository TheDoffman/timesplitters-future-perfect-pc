#include "guest_object_names.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"name contract line %d: %s\n",__LINE__,#c);exit(1); } } while(0)
static tsfp_guest_names names;
static tsfp_guest_objects objects;
static tsfp_guest_handle_record handles[8];
static tsfp_guest_object_record records[8];
static unsigned allocations,frees,closes,fail_allocation,stop_on_allocate,reenter;
static int close_host(void *ctx,uintptr_t host) {
    CHECK(ctx==&objects && host);++closes;return 1;
}
static void *allocate(void *ctx,size_t size) {
    void *p;CHECK(ctx==&names);
    if(fail_allocation) return NULL;
    if(stop_on_allocate) { stop_on_allocate=0;(void)tsfp_guest_names_stop(&names); }
    p=malloc(size);CHECK(p);++allocations;return p;
}
static void dispose(void *ctx,void *p) {
    CHECK(ctx==&names && p);
    if(reenter) {
        uint64_t key=0;int status;
        CHECK(names.lock==0);
        status=tsfp_guest_names_lookup(&names,"probe",5,0,&key);
        CHECK(status==TSFP_NAME_MISSING || status==TSFP_NAME_CLOSED || status==TSFP_NAME_OK);
        if(status==TSFP_NAME_OK) CHECK(tsfp_guest_objects_dereference(&objects,key));
    }
    ++frees;free(p);
}
static void initialize(size_t limit) {
    allocations=frees=closes=fail_allocation=stop_on_allocate=0;reenter=1;
    CHECK(tsfp_guest_objects_init(&objects,handles,records,8,close_host,NULL,&objects));
    CHECK(tsfp_guest_names_init(&names,&objects,limit,allocate,dispose,&names));
}
static uint64_t make(uintptr_t host,uint32_t *handle) {
    uint64_t key;
    CHECK(tsfp_guest_objects_adopt(&objects,host,3,0,handle));
    CHECK(tsfp_guest_objects_reference(&objects,*handle,&key));return key;
}
static void release(uint32_t handle,uint64_t key) {
    CHECK(tsfp_guest_objects_close(&objects,handle));
    CHECK(tsfp_guest_objects_dereference(&objects,key));
}
static void lifetime(void) {
    unsigned char input[]={'A','l','p','h','a'};
    uint32_t handle;uint64_t key,found=0;
    initialize(4);key=make(42,&handle);
    CHECK(tsfp_guest_names_bind(&names,input,sizeof(input),0,key)==TSFP_NAME_OK);
    input[0]='Z';
    release(handle,key);CHECK(!closes);
    CHECK(tsfp_guest_names_lookup(&names,"Alpha",5,0,&found)==TSFP_NAME_OK && found==key);
    CHECK(tsfp_guest_names_remove(&names,"Alpha",5,0,key+1)==TSFP_NAME_MISSING);
    CHECK(tsfp_guest_names_remove(&names,"Alpha",5,0,key)==TSFP_NAME_OK);
    CHECK(!closes && names.count==0 && allocations==frees);
    CHECK(tsfp_guest_objects_dereference(&objects,found));CHECK(closes==1);
    found=123;
    CHECK(tsfp_guest_names_lookup(&names,"Alpha",5,0,&found)==TSFP_NAME_MISSING && found==123);
    CHECK(tsfp_guest_names_stop(&names)==0);
}
static void comparisons_and_limits(void) {
    uint32_t a,b;uint64_t first,second,found=123;
    initialize(2);first=make(11,&a);second=make(22,&b);
    CHECK(tsfp_guest_names_bind(&names,"Case",4,0,first)==TSFP_NAME_OK);
    CHECK(tsfp_guest_names_lookup(&names,"case",4,0,&found)==TSFP_NAME_MISSING && found==123);
    CHECK(tsfp_guest_names_bind(&names,"cASE",4,1,second)==TSFP_NAME_COLLISION);
    CHECK(tsfp_guest_names_bind(&names,"case",4,0,second)==TSFP_NAME_OK);
    CHECK(tsfp_guest_names_lookup(&names,"CASE",4,1,&found)==TSFP_NAME_OK && found==second);
    CHECK(tsfp_guest_objects_dereference(&objects,found));
    CHECK(tsfp_guest_names_lookup(&names,"Case",4,0,&found)==TSFP_NAME_OK && found==first);
    CHECK(tsfp_guest_objects_dereference(&objects,found));
    CHECK(tsfp_guest_names_bind(&names,"other",5,0,first)==TSFP_NAME_CAPACITY);
    release(a,first);release(b,second);CHECK(!closes);
    CHECK(tsfp_guest_names_stop(&names)==2 && closes==2 && allocations==frees);
    CHECK(tsfp_guest_names_stop(&names)==0);
    found=456;
    CHECK(tsfp_guest_names_lookup(&names,"Case",4,0,&found)==TSFP_NAME_CLOSED && found==456);
    CHECK(tsfp_guest_names_bind(&names,"late",4,0,first)==TSFP_NAME_CLOSED);
    CHECK(tsfp_guest_names_remove(&names,"Case",4,0,0)==TSFP_NAME_CLOSED);
}
static void counted_bytes(void) {
    unsigned char one[]={'a',0,'z'},two[]={'a',0,'x'},high[]={0xe1},upper[]={0xc1};
    unsigned char *long_name=malloc(UINT16_MAX);
    uint32_t handle;uint64_t key,found=0;
    CHECK(long_name);memset(long_name,'q',UINT16_MAX);
    initialize(4);key=make(10,&handle);
    CHECK(tsfp_guest_names_bind(&names,one,3,0,key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_names_lookup(&names,two,3,0,&found)==TSFP_NAME_MISSING);
    CHECK(tsfp_guest_names_lookup(&names,one,2,0,&found)==TSFP_NAME_MISSING);
    CHECK(tsfp_guest_names_bind(&names,high,1,1,key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_names_lookup(&names,upper,1,1,&found)==TSFP_NAME_MISSING);
    CHECK(tsfp_guest_names_bind(&names,long_name,UINT16_MAX,0,key)==TSFP_NAME_OK);
    CHECK(tsfp_guest_names_lookup(&names,long_name,UINT16_MAX,0,&found)==TSFP_NAME_OK && found==key);
    CHECK(tsfp_guest_objects_dereference(&objects,found));
    CHECK(tsfp_guest_names_bind(&names,long_name,(size_t)UINT16_MAX+1,0,key)==TSFP_NAME_INVALID);
    CHECK(tsfp_guest_names_bind(&names,NULL,1,0,key)==TSFP_NAME_INVALID);
    CHECK(tsfp_guest_names_bind(&names,one,0,0,key)==TSFP_NAME_INVALID);
    CHECK(tsfp_guest_names_bind(&names,one,3,2,key)==TSFP_NAME_INVALID);
    CHECK(tsfp_guest_names_lookup(&names,one,3,0,NULL)==TSFP_NAME_INVALID);
    release(handle,key);CHECK(!closes);
    CHECK(tsfp_guest_names_stop(&names)==3 && closes==1 && allocations==frees);free(long_name);
}
static void failures(void) {
    uint32_t handle;uint64_t key,found=55;
    initialize(4);key=make(12,&handle);
    fail_allocation=1;
    CHECK(tsfp_guest_names_bind(&names,"fail",4,0,key)==TSFP_NAME_ALLOCATION);
    CHECK(!names.count && !allocations);
    fail_allocation=0;
    CHECK(tsfp_guest_names_bind(&names,"stale",5,0,UINT64_MAX)==TSFP_NAME_OBJECT);
    CHECK(allocations==frees && !names.count);
    stop_on_allocate=1;
    CHECK(tsfp_guest_names_bind(&names,"racing",6,0,key)==TSFP_NAME_CLOSED);
    CHECK(allocations==frees && !names.count);
    release(handle,key);CHECK(closes==1);
    CHECK(tsfp_guest_names_lookup(NULL,"none",4,0,&found)==TSFP_NAME_INVALID && found==55);
    CHECK(tsfp_guest_names_stop(NULL)==0);
}

#if defined(_WIN32)
static DWORD WINAPI reader(void *argument)
#else
static void *reader(void *argument)
#endif
{
    unsigned i;(void)argument;
    for(i=0;i<4000;++i) {
        uint64_t key=0;int status=tsfp_guest_names_lookup(&names,"SHARED",6,1,&key);
        CHECK(status==TSFP_NAME_OK || status==TSFP_NAME_MISSING);
        if(status==TSFP_NAME_OK) {
            tsfp_guest_object_lease lease={0};
            CHECK(tsfp_guest_objects_acquire_key(&objects,key,&lease));CHECK(lease.host==42);
            CHECK(tsfp_guest_objects_release(&lease));CHECK(tsfp_guest_objects_dereference(&objects,key));
        }
    }
    return 0;
}
static void concurrent(void) {
    uint32_t handle;uint64_t key;unsigned i;
#if defined(_WIN32)
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    initialize(4);reenter=0;key=make(42,&handle);
    CHECK(tsfp_guest_names_bind(&names,"shared",6,0,key)==TSFP_NAME_OK);
    for(i=0;i<4;++i) {
#if defined(_WIN32)
        threads[i]=CreateThread(NULL,0,reader,NULL,0,NULL);CHECK(threads[i]);
#else
        CHECK(!pthread_create(&threads[i],NULL,reader,NULL));
#endif
    }
    for(i=0;i<400;++i) {
        CHECK(tsfp_guest_names_remove(&names,"shared",6,0,key)==TSFP_NAME_OK);
        CHECK(tsfp_guest_names_bind(&names,"shared",6,0,key)==TSFP_NAME_OK);
    }
    for(i=0;i<4;++i) {
#if defined(_WIN32)
        CHECK(WaitForSingleObject(threads[i],10000)==WAIT_OBJECT_0);CHECK(CloseHandle(threads[i]));
#else
        CHECK(!pthread_join(threads[i],NULL));
#endif
    }
    CHECK(!closes);release(handle,key);CHECK(!closes);
    CHECK(tsfp_guest_names_stop(&names)==1 && closes==1 && allocations==frees);
}
int main(void) {
    lifetime();comparisons_and_limits();counted_bytes();failures();concurrent();
    puts("PASS: counted names, object lifetime, failure cleanup and concurrent lookup/removal");return 0;
}
