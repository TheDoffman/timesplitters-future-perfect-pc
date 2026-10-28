/* Project guest-object adapter. Included by the kernel bridge after its ABI
 * macros/data constants; every provider is original project source. */
#include "guest_object_services.h"
#include "guest_object_scope.h"
#include "guest_lifetime.h"

#define TSFP_KERNEL_OBJECT_LIMIT 16383
#define TSFP_KERNEL_ASYNC_FILE 1u
static tsfp_guest_objects bridge_objects;
static tsfp_guest_handle_record bridge_handles[TSFP_KERNEL_OBJECT_LIMIT];
static tsfp_guest_object_record bridge_object_records[TSFP_KERNEL_OBJECT_LIMIT];
static tsfp_guest_object_services bridge_object_services;
static RECOMP_TLS tsfp_guest_object_scopes bridge_object_scopes;
static RECOMP_TLS uint64_t bridge_thread_key;
static uint64_t bridge_process_key;
static int bridge_objects_initialized;
extern uint32_t xbox_CurrentThreadObject(void);
static int bridge_object_publish_thread(uint64_t,uint32_t);
static int bridge_object_retain_host(HANDLE,uint32_t,uint64_t *);

static void *bridge_object_output(void *context,uint32_t address,uint32_t bytes) {
    tsfp_guest_vm memory;
    (void)context;xbox_GetGuestVirtualMemory(&memory);
    return memory.resolve(memory.host,address,bytes,1);
}
static void bridge_object_word(void *target,uint32_t value) {
    unsigned char *p=(unsigned char *)target;unsigned i;
    for(i=0;i<4;++i) p[i]=(unsigned char)(value>>(i*8));
}
static int bridge_object_close_host(void *context,uintptr_t host) {
    (void)context;
    return CloseHandle((HANDLE)host)?1:-1;
}
static int bridge_object_free_body(void *context,uint32_t body,uint32_t kind,uint32_t flags) {
    tsfp_guest_vm memory;
    (void)context;(void)flags;
    /* TIB/object storage is retained for the guest-memory lifetime. */
    if(kind==TSFP_KIND_THREAD) return 1;
    xbox_GetGuestVirtualMemory(&memory);
    return tsfp_heap_release(memory.allocations,body)?1:-1;
}
static void bridge_object_discard_body(void *context,uint32_t body) {
    (void)context;
    xbox_HeapFree(body);
}
static uint32_t bridge_object_create_body(void *context,const tsfp_guest_object_lease *lease) {
    uint32_t body;unsigned char *p;
    (void)context;
    /* Thread bodies must be published from the existing TIB, never fabricated. */
    if(lease->kind==TSFP_KIND_THREAD) return 0;
    body=xbox_HeapAlloc(64,16);
    if(!body) return 0;
    p=(unsigned char *)bridge_object_output(NULL,body,64);
    if(!p) { xbox_HeapFree(body);return 0; }
    memset(p,0,64);
    p[2]=4;
    switch(lease->kind) {
    case TSFP_KIND_EVENT_AUTO:p[0]=1;break;
    case TSFP_KIND_MUTANT:p[0]=2;p[2]=8;break;
    case TSFP_KIND_PROCESS:p[0]=3;break;
    case TSFP_KIND_SEMAPHORE:p[0]=5;p[2]=5;break;
    case TSFP_KIND_TIMER:p[0]=8;p[2]=10;break;
    default:break;
    }
    /* Size is the dispatcher object's size in guest words, not header size. */
    bridge_object_word(p+8,body+8);bridge_object_word(p+12,body+8);
    return body;
}
static int bridge_object_refresh_signal(void *context,const tsfp_guest_object_lease *lease,uint32_t body) {
    DWORD result;volatile LONG *signal;
    (void)context;
    signal=(volatile LONG *)bridge_object_output(NULL,body+4,4);
    if(!signal || ((uintptr_t)signal&3)) return 0;
    /* A POSIX host thread not created by the compat layer has only a pseudo
     * handle. It is still running; a zero-timeout wait cannot query that token. */
    if(lease->host==(uintptr_t)(intptr_t)-2) result=WAIT_TIMEOUT;
    else result=WaitForSingleObject((HANDLE)lease->host,0);
    if(result!=WAIT_OBJECT_0 && result!=WAIT_TIMEOUT) return 0;
    InterlockedExchange(signal,result==WAIT_OBJECT_0?1:0);return 1;
}
static uint64_t bridge_object_current_key(void *context,uint32_t pseudo) {
    (void)context;
    /* Timer/USB host workers also enter guest callbacks. Their TIB is already
     * supplied by thread memory; adopt an identity on first use on that thread. */
    if(pseudo==UINT32_MAX-1 && !bridge_thread_key && g_fs_base) {
        uint64_t key=0;
        if(bridge_object_retain_host(GetCurrentThread(),TSFP_KIND_THREAD,&key)) {
            if(bridge_object_publish_thread(key,g_fs_base)) bridge_thread_key=key;
            else tsfp_guest_objects_dereference(&bridge_objects,key);
        }
    }
    return pseudo==UINT32_MAX?bridge_process_key:bridge_thread_key;
}
static uint64_t bridge_object_capture(void) { return tsfp_guest_scope_mark(&bridge_object_scopes); }
static int bridge_object_restore(uint64_t mark) { return tsfp_guest_scope_unwind(&bridge_object_scopes,mark); }
static int bridge_object_publish_thread(uint64_t key,uint32_t tib) {
    uint32_t body,winner=0;unsigned char *p;
    p=(unsigned char *)bridge_object_output(NULL,tib+0x28,4);
    if(!p) return 0;
    body=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
    p=(unsigned char *)bridge_object_output(NULL,body,16);
    if(!body || !p) return 0;
    /* Only the dispatcher prologue is initialized. Preserve TLS, ID and every
     * other field supplied by the original project thread-memory provider. */
    memset(p,0,16);p[0]=6;p[2]=0x44; /* Xbox KTHREAD is 0x110 bytes. */
    bridge_object_word(p+8,body+8);bridge_object_word(p+12,body+8);
    return tsfp_guest_objects_publish(&bridge_objects,key,body,&winner) && winner==body;
}
static int bridge_object_retain_host(HANDLE host,uint32_t kind,uint64_t *key) {
    HANDLE owned=NULL;uint32_t token;
    if(!DuplicateHandle(GetCurrentProcess(),host,GetCurrentProcess(),&owned,0,FALSE,DUPLICATE_SAME_ACCESS)) return 0;
    if(!tsfp_guest_objects_adopt(&bridge_objects,(uintptr_t)owned,kind,0,&token)) {
        CloseHandle(owned);return 0;
    }
    if(!tsfp_guest_objects_reference(&bridge_objects,token,key)) {
        tsfp_guest_objects_close(&bridge_objects,token);return 0;
    }
    tsfp_guest_objects_close(&bridge_objects,token);return 1;
}
static void bridge_object_init(void) {
    /* Reinitializing a live owner would erase host ownership. A new guest
     * memory lifetime needs an explicit drain before this can become reusable. */
    if(bridge_objects_initialized) abort();
    if(!tsfp_guest_objects_init(&bridge_objects,bridge_handles,bridge_object_records,
        TSFP_KERNEL_OBJECT_LIMIT,bridge_object_close_host,bridge_object_free_body,NULL)) abort();
    memset(&bridge_object_services,0,sizeof(bridge_object_services));
    bridge_object_services.objects=&bridge_objects;
    bridge_object_services.resolve_output=bridge_object_output;
    bridge_object_services.current_key=bridge_object_current_key;
    bridge_object_services.create_body=bridge_object_create_body;
    bridge_object_services.discard_body=bridge_object_discard_body;
    bridge_object_services.refresh_signal=bridge_object_refresh_signal;
    bridge_object_services.type_id[TSFP_KIND_THREAD]=XBOX_KERNEL_DATA_BASE+KDATA_THREAD_OBJ_TYPE;
    bridge_object_services.type_id[TSFP_KIND_EVENT_MANUAL]=XBOX_KERNEL_DATA_BASE+KDATA_EVENT_OBJ_TYPE;
    bridge_object_services.type_id[TSFP_KIND_EVENT_AUTO]=XBOX_KERNEL_DATA_BASE+KDATA_EVENT_OBJ_TYPE;
    bridge_object_services.type_id[TSFP_KIND_MUTANT]=XBOX_KERNEL_DATA_BASE+KDATA_MUTANT_OBJ_TYPE;
    bridge_object_services.type_id[TSFP_KIND_SEMAPHORE]=XBOX_KERNEL_DATA_BASE+KDATA_SEMAPHORE_OBJ_TYPE;
    bridge_object_services.type_id[TSFP_KIND_TIMER]=XBOX_KERNEL_DATA_BASE+KDATA_TIMER_OBJ_TYPE;
    bridge_object_services.type_id[TSFP_KIND_FILE]=XBOX_KERNEL_DATA_BASE+KDATA_FILE_OBJ_TYPE;
    if(!tsfp_guest_lifetime_hooks(bridge_object_capture,bridge_object_restore) ||
       !bridge_object_retain_host(GetCurrentProcess(),TSFP_KIND_PROCESS,&bridge_process_key) ||
       !bridge_object_retain_host(GetCurrentThread(),TSFP_KIND_THREAD,&bridge_thread_key) ||
       !bridge_object_publish_thread(bridge_thread_key,g_fs_base)) abort();
    bridge_objects_initialized=1;
}
static const tsfp_guest_object_lease *bridge_object_borrow(uint32_t token) {
    const tsfp_guest_object_lease *lease;uint64_t key;
    if(token==UINT32_MAX || token==UINT32_MAX-1) {
        key=bridge_object_current_key(NULL,token);
        return tsfp_guest_scope_borrow_key(&bridge_object_scopes,token,key);
    }
    lease=tsfp_guest_scope_borrow(&bridge_object_scopes,token);
    if(lease) return lease;
    if(!tsfp_guest_objects_reference_dispatcher(&bridge_objects,token,&key)) return NULL;
    lease=tsfp_guest_scope_borrow_key(&bridge_object_scopes,token,key);
    tsfp_guest_objects_dereference(&bridge_objects,key);return lease;
}
static HANDLE bridge_resolve_handle(uint32_t token) {
    const tsfp_guest_object_lease *lease=bridge_object_borrow(token);
    return lease?(HANDLE)lease->host:NULL;
}
static int bridge_handle_is_async(uint32_t token) {
    const tsfp_guest_object_lease *lease=bridge_object_borrow(token);
    return lease && (lease->flags&TSFP_KERNEL_ASYNC_FILE)!=0;
}
/* Explicit standalone scope for the audio worker, which bypasses thunk dispatch. */
int xbox_bridge_signal_event(uint32_t token) {
    tsfp_guest_object_scope frame;const tsfp_guest_object_lease *lease;int ok=0;
    if(!tsfp_guest_scope_enter(&bridge_object_scopes,&frame,&bridge_objects)) return 0;
    lease=bridge_object_borrow(token);
    if(lease && (lease->kind==TSFP_KIND_EVENT_MANUAL || lease->kind==TSFP_KIND_EVENT_AUTO))
        ok=SetEvent((HANDLE)lease->host)!=0;
    if(!tsfp_guest_scope_leave(&bridge_object_scopes,&frame)) abort();
    return ok;
}
typedef struct bridge_object_creation {
    uint32_t ticket;
    void *output;
} bridge_object_creation;
static uint32_t bridge_object_prepare(bridge_object_creation *creation,uint32_t address,uint32_t kind,uint32_t flags) {
    memset(creation,0,sizeof(*creation));
    if(!address || address>UINT32_MAX-3 || !(creation->output=bridge_object_output(NULL,address,4)))
        return TSFP_OBJECT_BAD_ADDRESS;
    if(!tsfp_guest_objects_prepare(&bridge_objects,kind,flags,&creation->ticket)) return TSFP_OBJECT_NO_MEMORY;
    return 0;
}
static uint32_t bridge_object_finish(bridge_object_creation *creation,HANDLE host,uint32_t status) {
    if((int32_t)status<0 || !host || host==INVALID_HANDLE_VALUE) {
        /* Native creation failures must leave no host reference. A provider
         * violating that contract cannot be silently treated as success. */
        if(host && host!=INVALID_HANDLE_VALUE && !CloseHandle(host)) abort();
        if(!tsfp_guest_objects_cancel(&bridge_objects,creation->ticket)) abort();
        creation->ticket=0;return (int32_t)status<0?status:TSFP_OBJECT_HOST_FAILURE;
    }
    if(!tsfp_guest_objects_commit(&bridge_objects,creation->ticket,(uintptr_t)host)) abort();
    bridge_object_word(creation->output,creation->ticket);creation->ticket=0;return status;
}
static void bridge_object_thread_exit(void) {
    if(!tsfp_guest_scope_abort(&bridge_object_scopes)) abort();
    if(bridge_thread_key) {
        uint64_t key=bridge_thread_key;bridge_thread_key=0;
        if(!tsfp_guest_objects_dereference(&bridge_objects,key)) abort();
    }
}
void xbox_bridge_thread_objects_exit(void) { bridge_object_thread_exit(); }
