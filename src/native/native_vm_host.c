#include "native_vm_host.h"
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
void *tsfp_native_vm_allocate(void *hint,size_t bytes,uint32_t type,uint32_t protection) {
    return VirtualAlloc(hint,bytes,(DWORD)type,(DWORD)protection);
}
int tsfp_native_vm_release(void *base,size_t bytes,uint32_t type) {
    return VirtualFree(base,bytes,(DWORD)type)!=0;
}
int tsfp_native_vm_protect(void *base,size_t bytes,uint32_t protection) {
    return tsfp_host_pages_protect(base,bytes,protection);
}
int tsfp_native_vm_query(const void *address,tsfp_native_vm_region *out) {
    MEMORY_BASIC_INFORMATION info={0};
    tsfp_native_vm_region result={0};
    if(!out || VirtualQuery(address,&info,sizeof(info))!=sizeof(info))return 0;
    result.base=(uintptr_t)info.BaseAddress;result.bytes=info.RegionSize;
    result.allocation_base=(uintptr_t)info.AllocationBase;
    result.allocation_protection=info.AllocationProtect;result.state=info.State;
    result.protection=info.Protect;result.type=info.Type;
    result.partition_id=info.PartitionId;
    *out=result;return 1;
}
#else
#include <pthread.h>
#include <sys/mman.h>
static tsfp_native_reservations reservations;
static tsfp_native_reservation records[4096];
static pthread_once_t once=PTHREAD_ONCE_INIT;
static int ready;
static int posix_protection(uint32_t p) {
    if(p==1)return PROT_NONE;
    if(p==2)return PROT_READ;
    if(p==4 || p==8)return PROT_READ|PROT_WRITE;
    if(p==16)return PROT_EXEC;
    if(p==32)return PROT_READ|PROT_EXEC;
    return PROT_READ|PROT_WRITE|PROT_EXEC;
}
static void *reserve_mapping(void *context,void *hint,size_t bytes) {
    void *p;
    (void)context;
    /* A hint is never MAP_FIXED: a collision must not destroy another mapping.
     * The owner releases a different returned address and reports failure. */
    p=mmap(hint,bytes,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
    return p==MAP_FAILED?NULL:p;
}
static int release_mapping(void *context,void *base,size_t bytes) {
    (void)context;return munmap(base,bytes)==0;
}
static int commit_mapping(void *context,void *base,size_t bytes,uint32_t protection,int zero) {
    int desired=posix_protection(protection);
    (void)context;
    if(zero) {
        if(mprotect(base,bytes,PROT_READ|PROT_WRITE))return -1;
        memset(base,0,bytes);
    }
    if(!mprotect(base,bytes,desired))return 1;
    /* OS protection failure need not be transactional. For previously reserved
     * dirty pages, successful rollback restores the reserved/no-access state.
     * Otherwise quarantine the owned mapping; exact release remains available. */
    if(zero && !mprotect(base,bytes,PROT_NONE))return 0;
    return -1;
}
static int decommit_mapping(void *context,void *base,size_t bytes) {
    (void)context;
    if(mprotect(base,bytes,PROT_NONE))return -1;
    /* Advice is optional reclamation, not a zero-content guarantee. The owner
     * records discarded pages and clears them on every later recommit. */
#ifdef MADV_FREE
    (void)madvise(base,bytes,MADV_FREE);
#endif
    return 1;
}
static int query_mapping(void *context,const void *address,tsfp_host_page_region *out) {
    (void)context;return tsfp_host_pages_query(address,out);
}
static void initialize(void) {
    const tsfp_reservation_backend backend={NULL,reserve_mapping,release_mapping,
        commit_mapping,decommit_mapping,query_mapping};
    ready=tsfp_native_reservations_init(&reservations,&backend,tsfp_host_page_size(),
        records,sizeof(records)/sizeof(records[0]));
}
static tsfp_native_reservations *owner(void) {
    return !pthread_once(&once,initialize) && ready?&reservations:NULL;
}
void *tsfp_native_vm_allocate(void *hint,size_t bytes,uint32_t type,uint32_t protection) {
    return tsfp_native_reserve(owner(),hint,bytes,type,protection);
}
int tsfp_native_vm_release(void *base,size_t bytes,uint32_t type) {
    return tsfp_native_reserve_free(owner(),base,bytes,type);
}
int tsfp_native_vm_query(const void *address,tsfp_native_vm_region *out) {
    return tsfp_native_reserve_query(owner(),address,out);
}
int tsfp_native_vm_protect(void *base,size_t bytes,uint32_t protection) {
    return tsfp_native_reserve_protect(owner(),base,bytes,protection);
}
#endif
