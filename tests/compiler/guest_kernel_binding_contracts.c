/* Executes the proposed adapters against real Windows handles and threads.
 * Guest-memory mapping/allocation are bounded fixtures, not a game instance. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "guest_virtual_memory.h"
#include "guest_lifetime.h"
#ifdef TSFP_REAL_THREAD_MEMORY
#include "guest_thread_memory.h"
#define BINDING_RAM_BYTES (40*1024*1024)
static tsfp_thread_memory real_threads;
static tsfp_allocation_record real_records[2048];
#else
#define BINDING_RAM_BYTES (2*1024*1024)
#endif
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"binding line %d: %s\n",__LINE__,#c);exit(1); } } while(0)
#define RECOMP_TLS __declspec(thread)
#define XBOX_THREAD_MODE_INLINE 0
#define XBOX_THREAD_MODE_SPAWN 1
#define XBOX_KERNEL_DATA_BASE 0x3000u
#define KDATA_THREAD_OBJ_TYPE 0
#define KDATA_EVENT_OBJ_TYPE 4
#define KDATA_MUTANT_OBJ_TYPE 8
#define KDATA_SEMAPHORE_OBJ_TYPE 12
#define KDATA_TIMER_OBJ_TYPE 16
#define KDATA_FILE_OBJ_TYPE 20
__declspec(align(16)) static unsigned char ram[BINDING_RAM_BYTES];
static RECOMP_TLS uint32_t g_esp,g_fs_base,g_eax,g_seh_ebp,g_thread_stack_top;
static RECOMP_TLS int g_is_spawned_thread;
static int g_thread_call_count,g_thread_mode=XBOX_THREAD_MODE_SPAWN;
#ifndef TSFP_REAL_THREAD_MEMORY
static volatile LONG heap_next=0x10000;
#endif
static volatile LONG allocations,frees,stack_live,spawns;
static int fail_allocate,fail_host_create,fail_descriptor;
static tsfp_allocations allocation_state;
static HANDLE entered,finish_worker;
static volatile LONG explicit_exit,worker_checked;
static uint64_t worker_key;
static uint32_t worker_body;
typedef void (*recomp_func_t)(void);
#define BRIDGE_MEM32(a) (*(uint32_t *)(ram+(a)))
#define STACK_ARG(n) BRIDGE_MEM32(g_esp+4+4*(n))
static tsfp_guest_lifetime_capture capture_hook;
static tsfp_guest_lifetime_restore restore_hook;
int tsfp_guest_lifetime_hooks(tsfp_guest_lifetime_capture capture,tsfp_guest_lifetime_restore restore) {
    CHECK(capture && restore);capture_hook=capture;restore_hook=restore;return 1;
}
static void *resolve(void *context,uint32_t address,uint32_t bytes,int writable) {
    (void)context;CHECK(writable==1);
    return address && address<=sizeof(ram) && bytes<=sizeof(ram)-address?ram+address:NULL;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *memory) {
    memset(memory,0,sizeof(*memory));memory->allocations=&allocation_state;memory->resolve=resolve;
}
static uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) {
#ifdef TSFP_REAL_THREAD_MEMORY
    uint32_t base=fail_allocate?0:tsfp_heap_allocate(&allocation_state,bytes,alignment);
    if(base) InterlockedIncrement(&allocations);
    return base;
#else
    LONG size=(LONG)((bytes+15)&~15u),base;
    CHECK(alignment<=16 || alignment==4096);
    if(fail_allocate) return 0;
    base=InterlockedExchangeAdd(&heap_next,size);
    CHECK(base>=0x10000 && (uint32_t)base+size<=sizeof(ram));
    InterlockedIncrement(&allocations);return (uint32_t)base;
#endif
}
#ifndef TSFP_REAL_THREAD_MEMORY
int tsfp_heap_release(tsfp_allocations *state,uint32_t address) {
    CHECK(state==&allocation_state && address>=0x10000 && address<(uint32_t)heap_next);
    InterlockedIncrement(&frees);return 1;
}
#endif
static void xbox_HeapFree(uint32_t address) {
    CHECK(tsfp_heap_release(&allocation_state,address));
#ifdef TSFP_REAL_THREAD_MEMORY
    InterlockedIncrement(&frees);
#endif
}
static uint32_t xbox_AllocThreadStack(void) {
#ifdef TSFP_REAL_THREAD_MEMORY
    uint32_t top=fail_allocate?0:tsfp_thread_stack_acquire(&real_threads);
    if(top) { InterlockedIncrement(&allocations);InterlockedIncrement(&stack_live); }
    return top;
#else
    uint32_t base=xbox_HeapAlloc(16384,16);
    if(base) InterlockedIncrement(&stack_live);
    return base?base+16384-16:0;
#endif
}
static void xbox_FreeThreadStack(uint32_t top) {
#ifdef TSFP_REAL_THREAD_MEMORY
    CHECK(top && InterlockedDecrement(&stack_live)>=0);
    CHECK(tsfp_thread_stack_release(&real_threads,top));InterlockedIncrement(&frees);
#else
    CHECK(top && InterlockedDecrement(&stack_live)>=0);xbox_HeapFree(top+16-16384);
#endif
}
static uint32_t xbox_AllocThreadTib(void) {
#ifdef TSFP_REAL_THREAD_MEMORY
    uint32_t tib=fail_allocate?0:tsfp_thread_tib_create(&real_threads);
    if(tib) InterlockedIncrement(&allocations);
    return tib;
#else
    uint32_t tib=xbox_HeapAlloc(0x300,16);
    if(tib) {
        memset(ram+tib,0x7c,0x300);BRIDGE_MEM32(tib+0x28)=tib+0x100;
        BRIDGE_MEM32(tib+0x100+0x12c)=tib;
    }
    return tib;
#endif
}
uint32_t xbox_CurrentThreadObject(void) {
#ifdef TSFP_REAL_THREAD_MEMORY
    return tsfp_thread_current_object(&real_threads,g_fs_base);
#else
    return BRIDGE_MEM32(g_fs_base+0x28);
#endif
}
static void xbox_GuestLockEnter(void) { }
static void xbox_GuestLiftedEnter(void) { }
static void xbox_GuestLiftedExit(void) { }
static void xbox_NameCurrentThread(const wchar_t *name) { CHECK(name); }
static void worker(void);
static recomp_func_t recomp_lookup(uint32_t address) { return address==0x1234?worker:NULL; }
static recomp_func_t recomp_lookup_manual(uint32_t address) { (void)address;return NULL; }
static HANDLE create_thread(LPSECURITY_ATTRIBUTES attributes,SIZE_T stack,
    LPTHREAD_START_ROUTINE entry,LPVOID context,DWORD flags,LPDWORD id) {
    InterlockedIncrement(&spawns);
    if(fail_host_create) return NULL;
    return CreateThread(attributes,stack,entry,context,flags,id);
}
static int bridge_device_free_body(uint32_t body,uint32_t kind,uint32_t flags) {
    /* Device execution is covered by the separate complete host fixture. */
    (void)body;(void)kind;(void)flags;CHECK(0);return -1;
}
#include "guest_kernel_objects.inc"
static void *create_descriptor(size_t count,size_t bytes) {
    return fail_descriptor?NULL:calloc(count,bytes);
}
#define CreateThread create_thread
#define calloc create_descriptor
#include "guest_kernel_threads.inc"
#undef calloc
#undef CreateThread

/*PS_TERMINATE_BINDING*/

static void worker(void) {
    tsfp_guest_object_scope scope;const tsfp_guest_object_lease *lease;
    uint32_t body;uint64_t mark;
    CHECK(g_is_spawned_thread && g_thread_stack_top && g_fs_base);
    CHECK(BRIDGE_MEM32(g_esp+4)==0x11 && BRIDGE_MEM32(g_esp+8)==0x22);
    CHECK(tsfp_guest_scope_enter(&bridge_object_scopes,&scope,&bridge_objects));
    mark=capture_hook();CHECK(mark);
    lease=bridge_object_borrow(UINT32_MAX-1);CHECK(lease && lease->key==bridge_thread_key);
    CHECK(tsfp_guest_object_reference(&bridge_object_services,UINT32_MAX-1,
        XBOX_KERNEL_DATA_BASE+KDATA_THREAD_OBJ_TYPE,g_fs_base+0x40)==0);
    body=BRIDGE_MEM32(g_fs_base+0x40);CHECK(body==xbox_CurrentThreadObject());
    CHECK(ram[body]==6 && ram[body+2]==0x44 && ram[body+0x80]==0x7c);
    CHECK(BRIDGE_MEM32(body+8)==body+8 && BRIDGE_MEM32(body+12)==body+8);
    worker_key=lease->key;worker_body=body;
    InterlockedIncrement(&worker_checked);CHECK(SetEvent(entered));
    CHECK(WaitForSingleObject(finish_worker,5000)==WAIT_OBJECT_0);
    CHECK(lease->host && bridge_resolve_handle(UINT32_MAX-1)==(HANDLE)lease->host);
    CHECK(restore_hook(mark));
    if(explicit_exit) {
        BRIDGE_MEM32(g_esp+4)=0;
        bridge_PsTerminateSystemThread();CHECK(0);
    }
    CHECK(tsfp_guest_scope_leave(&bridge_object_scopes,&scope));
}
static void run_worker(int terminate,int suspended) {
    tsfp_guest_object_scope scope;const tsfp_guest_object_lease *lease;
    uint32_t token;uint64_t key;HANDLE observation=NULL;LONG previous=worker_checked;
    explicit_exit=terminate;ResetEvent(entered);ResetEvent(finish_worker);
    CHECK(bridge_create_thread(0x4000,0x4004,0x1234,0x11,0x22,suspended)==0);
    token=BRIDGE_MEM32(0x4000);CHECK(token && BRIDGE_MEM32(0x4004));
    CHECK(tsfp_guest_scope_enter(&bridge_object_scopes,&scope,&bridge_objects));
    lease=bridge_object_borrow(token);CHECK(lease && lease->kind==TSFP_KIND_THREAD);key=lease->key;
    CHECK(DuplicateHandle(GetCurrentProcess(),(HANDLE)lease->host,GetCurrentProcess(),&observation,0,FALSE,DUPLICATE_SAME_ACCESS));
    if(suspended) {
        CHECK(WaitForSingleObject(entered,30)==WAIT_TIMEOUT);
        CHECK(ResumeThread((HANDLE)lease->host)==1);
    }
    CHECK(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);
    CHECK(worker_checked==previous+1 && worker_key==key && BRIDGE_MEM32(0x4004)==BRIDGE_MEM32(worker_body+0x12c));
    CHECK(tsfp_guest_object_reference(&bridge_object_services,token,0,0x4008)==0);
    CHECK(BRIDGE_MEM32(0x4008)==worker_body);
    CHECK(tsfp_guest_object_close(&bridge_object_services,token)==0);
    CHECK(tsfp_guest_scope_leave(&bridge_object_scopes,&scope));
    CHECK(SetEvent(finish_worker));CHECK(WaitForSingleObject(observation,5000)==WAIT_OBJECT_0);
    CHECK(!stack_live && tsfp_guest_object_dereference_body(&bridge_object_services,worker_body));
    CHECK(tsfp_guest_object_dereference_body(&bridge_object_services,worker_body));
    CHECK(!tsfp_guest_objects_retain(&bridge_objects,key));CHECK(CloseHandle(observation));
}
static DWORD WINAPI auxiliary_worker(LPVOID ignored) {
    uint64_t key;uint32_t body;
    (void)ignored;CHECK(!bridge_thread_key);g_fs_base=xbox_AllocThreadTib();CHECK(g_fs_base);
    CHECK(tsfp_guest_object_reference(&bridge_object_services,UINT32_MAX-1,0,g_fs_base+0x40)==0);
    key=bridge_thread_key;body=BRIDGE_MEM32(g_fs_base+0x40);
    CHECK(key && body==xbox_CurrentThreadObject());
    CHECK(tsfp_guest_object_dereference_body(&bridge_object_services,body));
    xbox_bridge_thread_objects_exit();CHECK(!bridge_thread_key);
    CHECK(!tsfp_guest_objects_retain(&bridge_objects,key));return 0;
}
int main(void) {
    bridge_object_creation creation;tsfp_guest_object_scope frame;uint32_t token;
    HANDLE event,auxiliary;LONG before_spawns,before_alloc,before_free;uint64_t main_key;uint32_t main_body;
    g_fs_base=0x1000;g_esp=0x9000;
#ifdef TSFP_REAL_THREAD_MEMORY
    {
        tsfp_allocation_config config={0};tsfp_image_info info={0};
        config.ram=ram;config.ram_size=sizeof(ram);config.heap_begin=0xf80000;config.reserve_begin=sizeof(ram);
        CHECK(tsfp_allocations_init(&allocation_state,&config,real_records,2048,NULL,0));
        info.base=0x10000;info.image_end=0x650000;info.tls_directory=0x11000;
        info.tls_start=0x20000;info.tls_bytes=16;
        CHECK(tsfp_thread_memory_init(&real_threads,&allocation_state,&info));
        g_esp=0xf7fff0;
    }
#else
    BRIDGE_MEM32(0x1028)=0x2000;memset(ram+0x2000,0x7c,0x200);
#endif
    main_body=xbox_CurrentThreadObject();ram[main_body+0x80]=0x7c;
    bridge_object_init();main_key=bridge_thread_key;
    CHECK(main_key && bridge_process_key && xbox_CurrentThreadObject()==main_body);
    CHECK(tsfp_guest_object_reference(&bridge_object_services,UINT32_MAX-1,0,0x4000)==0);
    CHECK(BRIDGE_MEM32(0x4000)==main_body && ram[main_body+0x80]==0x7c);
    CHECK(tsfp_guest_object_dereference_body(&bridge_object_services,main_body));
    before_spawns=spawns;before_alloc=allocations;
    CHECK(bridge_create_thread(sizeof(ram)-2,0,0x1234,0x11,0x22,0)==TSFP_OBJECT_BAD_ADDRESS);
    CHECK(bridge_create_thread(0x4000,0,0x55,0x11,0x22,0)==TSFP_OBJECT_BAD_PARAMETER);
    CHECK(spawns==before_spawns && allocations==before_alloc);
    fail_descriptor=1;CHECK(bridge_create_thread(0x4000,0,0x1234,0x11,0x22,0)==TSFP_OBJECT_NO_MEMORY);fail_descriptor=0;
    CHECK(spawns==before_spawns && allocations==before_alloc);
    fail_allocate=1;CHECK(bridge_create_thread(0x4000,0,0x1234,0x11,0x22,0)==TSFP_OBJECT_NO_MEMORY);fail_allocate=0;
    before_alloc=allocations;before_free=frees;fail_host_create=1;
    CHECK(bridge_create_thread(0x4000,0,0x1234,0x11,0x22,0)==TSFP_OBJECT_NO_MEMORY);fail_host_create=0;
    CHECK(allocations-before_alloc==2 && frees-before_free==2 && !stack_live);
    entered=CreateEventW(NULL,TRUE,FALSE,NULL);finish_worker=CreateEventW(NULL,TRUE,FALSE,NULL);
    CHECK(entered && finish_worker);
    run_worker(0,0);run_worker(1,0);run_worker(0,1);
    auxiliary=CreateThread(NULL,0,auxiliary_worker,NULL,0,NULL);CHECK(auxiliary);
    CHECK(WaitForSingleObject(auxiliary,5000)==WAIT_OBJECT_0);CHECK(CloseHandle(auxiliary));
    CHECK(bridge_thread_key==main_key && !bridge_object_scopes.current);
    CHECK(bridge_object_prepare(&creation,0x4000,TSFP_KIND_EVENT_MANUAL,TSFP_KERNEL_ASYNC_FILE)==0);
    event=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(event);
    CHECK(bridge_object_finish(&creation,event,0)==0);token=BRIDGE_MEM32(0x4000);
    CHECK(xbox_bridge_signal_event(token) && WaitForSingleObject(event,0)==WAIT_OBJECT_0);
    CHECK(tsfp_guest_scope_enter(&bridge_object_scopes,&frame,&bridge_objects));
    CHECK(bridge_handle_is_async(token));CHECK(tsfp_guest_scope_leave(&bridge_object_scopes,&frame));
    CHECK(tsfp_guest_object_close(&bridge_object_services,token)==0 && !xbox_bridge_signal_event(token));
    CloseHandle(entered);CloseHandle(finish_worker);bridge_object_thread_exit();
    CHECK(tsfp_guest_objects_dereference(&bridge_objects,bridge_process_key));
    CHECK(!tsfp_guest_objects_collect(&bridge_objects));
#ifdef TSFP_REAL_THREAD_MEMORY
    CHECK(allocation_state.heap.count==4); /* Three worker TIBs plus one auxiliary TIB persist. */
    tsfp_thread_memory_stop(&real_threads);tsfp_allocations_stop(&allocation_state);
#endif
    puts("PASS: real thread publication, pseudo identity, suspended/normal/explicit exit, event signaling and failed creation");return 0;
}
