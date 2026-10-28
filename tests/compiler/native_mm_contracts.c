/* Original contract fixture for actual native Mm adapter bodies. */
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#define __stdcall
typedef void *PVOID;
typedef void VOID;
typedef uint32_t ULONG,*PULONG;
typedef uintptr_t ULONG_PTR;
typedef unsigned char BOOLEAN;
#endif
#include "host_virtual_memory.h"
#include "native_mm_adapters.inc"

static void worker(void) {
    for(unsigned i=0;i<100;++i) {
        unsigned char *p=xbox_MmAllocateContiguousMemoryEx(79,UINTPTR_MAX-1,
            UINTPTR_MAX,65536,4);
        assert(p && !((uintptr_t)p&65535u) && (uintptr_t)p>UINT32_MAX);
        for(unsigned j=0;j<79;++j)assert(!p[j]);
        memset(p,0x5a,79);
        assert(xbox_MmQueryAddressProtect(p)==4);
        xbox_MmFreeContiguousMemory(p);
    }
}
#if defined(_WIN32)
static DWORD WINAPI thread_entry(void *unused) { (void)unused;worker();return 0; }
#else
static void *thread_entry(void *unused) { (void)unused;worker();return NULL; }
#endif
static void concurrent_first_use(void) {
#if defined(_WIN32)
    HANDLE threads[4];
    for(unsigned i=0;i<4;++i){threads[i]=CreateThread(NULL,0,thread_entry,NULL,0,NULL);assert(threads[i]);}
    assert(WaitForMultipleObjects(4,threads,TRUE,INFINITE)==WAIT_OBJECT_0);
    for(unsigned i=0;i<4;++i)CloseHandle(threads[i]);
#else
    pthread_t threads[4];
    for(unsigned i=0;i<4;++i)assert(!pthread_create(threads+i,NULL,thread_entry,NULL));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
#endif
    assert(native_mm_ready && native_mm_storage.live==0);
}
int main(void) {
    unsigned char *contiguous,*system,*io,*gpu,*top;
    tsfp_host_page_region region;
    size_t page=tsfp_host_page_size();
    ULONG padding=17;
    concurrent_first_use();
    assert(!xbox_MmAllocateContiguousMemory(0));
    assert(!xbox_MmAllocateContiguousMemoryEx(0,0,UINTPTR_MAX,64,4));
    assert(!xbox_MmAllocateSystemMemory(0,4));
    assert(!xbox_MmCreateKernelStack(0,0));
    assert(!xbox_MmMapIoSpace(0,0,4));
    assert(!xbox_MmClaimGpuInstanceMemory(0,&padding) && !padding);
    contiguous=xbox_MmAllocateContiguousMemoryEx(4097,0,UINTPTR_MAX,3,4);
    system=xbox_MmAllocateSystemMemory((ULONG)(page*3),4);
    io=xbox_MmMapIoSpace(UINTPTR_MAX,83,1);
    gpu=xbox_MmClaimGpuInstanceMemory(87,&padding);
    top=xbox_MmCreateKernelStack(12345,1);
    assert(contiguous && system && io && gpu && top && !padding);
    assert(!((uintptr_t)contiguous%(uintptr_t)page));
    assert(native_mm_storage.live==5);
    assert(xbox_MmQueryAddressProtect(io)==4);
    assert(xbox_MmQueryAllocationSize(system)>=page*3);
    /* Reject allocator confusion, interior pointers, and foreign host memory. */
    xbox_MmFreeSystemMemory(contiguous,0);
    xbox_MmFreeContiguousMemory(system);
    xbox_MmUnmapIoSpace(gpu,0);
    xbox_MmDeleteKernelStack(top,top-12344);
    assert(native_mm_storage.live==5 && xbox_MmQueryAddressProtect(system)==4);
    {
        void *foreign=tsfp_host_pages_map(page,4);
        assert(foreign);
        xbox_MmFreeContiguousMemory(foreign);
        xbox_MmFreeSystemMemory(foreign,0);
        assert(tsfp_host_pages_query(foreign,&region));
        assert(tsfp_host_pages_unmap(foreign,page));
    }
    memset(system,0x31,page*3);
    xbox_MmSetAddressProtect(system+page+7,23,1);
    assert(xbox_MmQueryAddressProtect(system+page+7)==1);
    assert(xbox_MmQueryAddressProtect(system+7)==4);
    xbox_MmSetAddressProtect(system+page+7,23,2);
    assert(xbox_MmQueryAddressProtect(system+page+7)==2 && system[page+7]==0x31);
    xbox_MmSetAddressProtect(system+page+7,23,0); /* default access remains RW */
    assert(xbox_MmQueryAddressProtect(system+page+7)==4);
    system[page+7]=0x1a;
    xbox_MmFreeContiguousMemory(contiguous);
    xbox_MmFreeSystemMemory(system,1); /* size remains ignored */
    xbox_MmUnmapIoSpace(io,0);
    xbox_MmFreeContiguousMemory(gpu);
    xbox_MmDeleteKernelStack(NULL,top-12345); /* low address owns the stack */
    xbox_MmFreeContiguousMemory(contiguous); /* already released */
    assert(!native_mm_storage.live);
    assert(!xbox_MmQueryAllocationSize(NULL) && xbox_MmQueryAddressProtect(NULL)==1);
    /* The finite ownership table must reject admission without a hidden map. */
    {
        void *blocks[4096];
        for(size_t i=0;i<4096;++i){blocks[i]=xbox_MmAllocateSystemMemory(1,4);assert(blocks[i]);}
        assert(native_mm_storage.live==4096 && !xbox_MmAllocateSystemMemory(1,4));
        for(size_t i=0;i<4096;++i)xbox_MmFreeSystemMemory(blocks[i],0);
        assert(!native_mm_storage.live);
    }
    /* Extended allocation honors protection even with larger alignment. */
    contiguous=xbox_MmAllocateContiguousMemoryEx(4096,0,UINTPTR_MAX,65536,2);
    assert(contiguous && xbox_MmQueryAddressProtect(contiguous)==2 && !contiguous[17]);
    xbox_MmFreeContiguousMemory(contiguous);
    assert(!native_mm_storage.live);
    puts("native Mm adapter contracts passed");return 0;
}
