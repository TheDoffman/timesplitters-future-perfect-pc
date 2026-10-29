#include "guest_symlinks.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"symlinks line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static tsfp_guest_symlinks s;
static unsigned fail_alloc,stop_alloc,check_lock=1;
static volatile long allocated,freed;
static void increment(volatile long *p) {
#ifdef _WIN32
    (void)InterlockedIncrement(p);
#else
    (void)__atomic_add_fetch(p,1,__ATOMIC_SEQ_CST);
#endif
}
static void *allocate(void *p,size_t bytes) {
    void *out;CHECK(p==&s);if(check_lock)CHECK(!s.lock);
    if(stop_alloc) {stop_alloc=0;(void)tsfp_symlinks_stop(&s);}
    if(fail_alloc)return NULL;
    out=malloc(bytes);CHECK(out);memset(out,0xa7,bytes);increment(&allocated);return out;
}
static void dispose(void *p,void *memory) {
    char out[1];size_t n=99;int status;
    CHECK(p==&s && memory);if(check_lock)CHECK(!s.lock);
    status=tsfp_symlinks_copy(&s,"not-present",11,out,sizeof(out),&n);
    CHECK(status==TSFP_SYMLINK_MISSING || status==TSFP_SYMLINK_CLOSED);
    CHECK(n==99);increment(&freed);free(memory);
}
static void init(size_t limit) {
    CHECK(allocated==freed);CHECK(tsfp_symlinks_init(&s,limit,allocate,dispose,&s));
}
static void basics(void) {
    char name[80],other[80],target[160],out[170],before[170];size_t n=999;
    init(2);memset(name,'n',sizeof(name));memcpy(other,name,sizeof(name));other[79]='m';
    memset(target,'t',sizeof(target));target[159]='z';memset(out,0x77,sizeof(out));memcpy(before,out,sizeof(out));
    CHECK(tsfp_symlinks_create(&s,name,sizeof(name),target,sizeof(target))==TSFP_SYMLINK_OK);
    CHECK(tsfp_symlinks_create(&s,other,sizeof(other),"second",6)==TSFP_SYMLINK_OK);
    CHECK(tsfp_symlinks_create(&s,"third",5,"value",5)==TSFP_SYMLINK_RESOURCE && s.count==2);
    CHECK(tsfp_symlinks_create(&s,name,sizeof(name),"different",9)==TSFP_SYMLINK_COLLISION);
    CHECK(tsfp_symlinks_copy(&s,name,sizeof(name),out,159,&n)==TSFP_SYMLINK_RESOURCE);
    CHECK(n==999 && !memcmp(out,before,sizeof(out)));
    CHECK(tsfp_symlinks_copy(&s,name,sizeof(name)-1,out,sizeof(out),&n)==TSFP_SYMLINK_MISSING);
    CHECK(tsfp_symlinks_copy(&s,name,sizeof(name),out,sizeof(out),&n)==TSFP_SYMLINK_OK && n==160 && !memcmp(out,target,160));
    memset(target,'x',sizeof(target));CHECK(tsfp_symlinks_remove(&s,name,sizeof(name))==TSFP_SYMLINK_OK);
    CHECK(out[159]=='z' && tsfp_symlinks_remove(&s,name,sizeof(name))==TSFP_SYMLINK_MISSING);
    CHECK(tsfp_symlinks_create(&s,name,sizeof(name),target,sizeof(target))==TSFP_SYMLINK_OK);
    CHECK(out[159]=='z');CHECK(tsfp_symlinks_stop(&s)==2 && tsfp_symlinks_stop(&s)==0);
    CHECK(tsfp_symlinks_create(&s,"a",1,"b",1)==TSFP_SYMLINK_CLOSED);
    CHECK(tsfp_symlinks_remove(&s,"a",1)==TSFP_SYMLINK_CLOSED);
    CHECK(tsfp_symlinks_copy(&s,"a",1,out,sizeof(out),&n)==TSFP_SYMLINK_CLOSED && out[159]=='z');
    CHECK(allocated==freed);
}
static void limits(void) {
    unsigned char *name=malloc(65535),*target=malloc(65535),*out=malloc(65535);size_t n=0;
    CHECK(name && target && out);memset(name,0xcc,65535);memset(target,0x3a,65535);target[65534]=0xab;
    init(2);CHECK(tsfp_symlinks_create(&s,name,65535,target,65535)==TSFP_SYMLINK_OK);
    CHECK(tsfp_symlinks_copy(&s,name,65535,out,65535,&n)==TSFP_SYMLINK_OK && n==65535 && !memcmp(out,target,65535));
    CHECK(tsfp_symlinks_create(&s,name,65536,target,1)==TSFP_SYMLINK_INVALID);
    CHECK(tsfp_symlinks_create(&s,name,1,target,65536)==TSFP_SYMLINK_INVALID);
    CHECK(tsfp_symlinks_create(&s,NULL,1,target,1)==TSFP_SYMLINK_INVALID);
    CHECK(tsfp_symlinks_create(&s,name,0,target,1)==TSFP_SYMLINK_INVALID);
    CHECK(tsfp_symlinks_stop(&s)==1);free(name);free(target);free(out);
    init(2);fail_alloc=1;CHECK(tsfp_symlinks_create(&s,"a",1,"b",1)==TSFP_SYMLINK_RESOURCE && !s.count);fail_alloc=0;
    CHECK(tsfp_symlinks_create(&s,"a",1,"b",1)==TSFP_SYMLINK_OK);
    stop_alloc=1;CHECK(tsfp_symlinks_create(&s,"c",1,"d",1)==TSFP_SYMLINK_CLOSED && !s.count && allocated==freed);
}
static void worker(unsigned id) {
    char value[64],out[64];unsigned i,j;size_t n;int result;memset(value,(int)('A'+id),sizeof(value));
    for(i=0;i<250;++i) {
        result=tsfp_symlinks_create(&s,"race",4,value,sizeof(value));CHECK(result==TSFP_SYMLINK_OK || result==TSFP_SYMLINK_COLLISION);
        n=0;result=tsfp_symlinks_copy(&s,"race",4,out,sizeof(out),&n);CHECK(result==TSFP_SYMLINK_OK || result==TSFP_SYMLINK_MISSING);
        if(result==TSFP_SYMLINK_OK) {CHECK(n==64 && out[0]>='A' && out[0]<='D');for(j=1;j<64;++j)CHECK(out[j]==out[0]);}
        result=tsfp_symlinks_remove(&s,"race",4);CHECK(result==TSFP_SYMLINK_OK || result==TSFP_SYMLINK_MISSING);
    }
}
#ifdef _WIN32
static DWORD WINAPI thread(void *p) {worker((unsigned)(uintptr_t)p);return 0;}
#else
static void *thread(void *p) {worker((unsigned)(uintptr_t)p);return NULL;}
#endif
static void concurrency(void) {
    unsigned i;
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    init(4);check_lock=0;
    for(i=0;i<4;++i) {
#ifdef _WIN32
        threads[i]=CreateThread(NULL,0,thread,(void *)(uintptr_t)i,0,NULL);CHECK(threads[i]);
#else
        CHECK(!pthread_create(&threads[i],NULL,thread,(void *)(uintptr_t)i));
#endif
    }
    for(i=0;i<4;++i) {
#ifdef _WIN32
        CHECK(WaitForSingleObject(threads[i],10000)==WAIT_OBJECT_0 && CloseHandle(threads[i]));
#else
        CHECK(!pthread_join(threads[i],NULL));
#endif
    }
    check_lock=1;(void)tsfp_symlinks_stop(&s);CHECK(allocated==freed);
}
int main(void) {basics();limits();concurrency();puts("PASS: exact counted links, snapshot lifetime, bounds, reentrant cleanup, stop during allocation and four native workers");return 0;}
