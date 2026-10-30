#include "native_vm_host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static DWORD WINAPI first_use(void *arg)
#else
#include <pthread.h>
static void *first_use(void *arg)
#endif
{
    size_t page=tsfp_host_page_size();
    unsigned char *p=tsfp_native_vm_allocate(NULL,page,0x3000,4);
    tsfp_native_vm_region info;
    assert(p);p[0]=(unsigned char)(uintptr_t)arg;
    assert(tsfp_native_vm_query(p,&info) && info.allocation_base==(uintptr_t)p);
    assert(p[0]==(unsigned char)(uintptr_t)arg);
    assert(tsfp_native_vm_release(p,0,0x8000));return 0;
}
int main(void) {
    size_t page=tsfp_host_page_size();
    unsigned char *p,*q;
    tsfp_native_vm_region info,before;
    assert(page && !(page&(page-1)));
#ifdef _WIN32
    HANDLE threads[4];
    for(uintptr_t i=0;i<4;++i) { threads[i]=CreateThread(NULL,0,first_use,(void *)(i+1),0,NULL);assert(threads[i]); }
    assert(WaitForMultipleObjects(4,threads,TRUE,30000)==WAIT_OBJECT_0);
    for(unsigned i=0;i<4;++i)assert(CloseHandle(threads[i]));
#else
    pthread_t threads[4];
    for(uintptr_t i=0;i<4;++i)assert(!pthread_create(threads+i,NULL,first_use,(void *)(i+1)));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
#endif
    assert(!tsfp_native_vm_allocate(NULL,0,0x3000,4));
    p=tsfp_native_vm_allocate(NULL,3*page,0x2000,4);assert(p && (uintptr_t)p>UINT32_MAX);
    assert(!tsfp_native_vm_protect(p,1,4));
    assert(tsfp_native_vm_query(p,&info) && info.allocation_base==(uintptr_t)p);
    assert(info.base==(uintptr_t)p && info.bytes>=3*page && info.state==0x2000 && info.type==0x20000);
    assert(tsfp_native_vm_allocate(p+page,page,0x1000,4)==p+page);
    memset(p+page,0x6d,page);
    assert(tsfp_native_vm_protect(p+page+3,1,2));
    assert(tsfp_native_vm_allocate(p+page,page,0x1000,2)==p+page);
    assert(p[page]==0x6d && p[2*page-1]==0x6d);
    assert(tsfp_native_vm_query(p+page,&info) && info.state==0x1000 && info.bytes==page);
    assert(!tsfp_native_vm_release(p+page,0,0x8000));
    assert(tsfp_native_vm_release(p+page+3,1,0x4000));
    assert(tsfp_native_vm_query(p+page,&info) && info.state==0x2000);
    assert(tsfp_native_vm_allocate(p+page,page,0x1000,4)==p+page);
    for(size_t i=page;i<2*page;++i)assert(!p[i]);
    assert(tsfp_native_vm_allocate(p,page,0x1000,2)==p);
    assert(tsfp_native_vm_query(p,&info) && info.state==0x1000 && info.protection==2 && !p[0]);
    assert(tsfp_native_vm_release(p,0,0x4000));
    assert(tsfp_native_vm_query(p,&info) && info.state==0x2000);
    assert(tsfp_native_vm_allocate(p,3*page,0x1000,4)==p);
    for(size_t i=0;i<3*page;++i)assert(!p[i]);
    p[0]=17;
    q=tsfp_native_vm_allocate(p,3*page,0x2000,4);
    assert(!q && p[0]==17); /* failed exact reservation cannot overwrite */
    before=info;
    assert(!tsfp_native_vm_query((void *)UINTPTR_MAX,&info));
    assert(!memcmp(&before,&info,sizeof(info)));
    assert(tsfp_native_vm_release(p,0,0x8000));
#ifdef _WIN32
    assert(tsfp_native_vm_query(p,&info) && info.state==MEM_FREE);
#else
    assert(!tsfp_native_vm_query(p,&info));
#endif
    assert(!tsfp_native_vm_release(p,0,0x8000));
    puts("real native reservation, recommit zeroing, query and release passed");return 0;
}
