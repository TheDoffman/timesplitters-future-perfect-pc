/* Original native RTL integration. Uses standard OS mutex/error facilities. */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#include "guest_kernel_rtl.h"
#include "host_critical.h"
#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#include <signal.h>
#endif
static tsfp_host_critical *kernel_critical;
static int kernel_trace_all,kernel_watch_enabled;
static uint32_t kernel_watch_address;
static int kernel_option(const char *name,char *output,size_t capacity) {
#ifdef _WIN32
    DWORD n=GetEnvironmentVariableA(name,output,(DWORD)capacity);
    if(n && n<capacity)return 1;
#else
    const char *value=getenv(name);size_t n;
    if(value){n=strlen(value);if(n<capacity){memcpy(output,value,n+1);return 1;}}
#endif
    output[0]=0;return 0;
}
static void kernel_critical_create(void) {
    char option[40],*end;unsigned long long watch;int single=kernel_option("RECOMP_CS_MODE",option,sizeof(option)) && !strcmp(option,"single");
    kernel_trace_all=kernel_option("RECOMP_CS_TRACE_CRT",option,sizeof(option)) && !strcmp(option,"all");
    if(kernel_option("RECOMP_CS_WATCH",option,sizeof(option))) {
        watch=strtoull(option,&end,0);
        if(*option && !*end && watch && watch<=UINT32_MAX){kernel_watch_enabled=1;kernel_watch_address=(uint32_t)watch;}
    }
    kernel_critical=tsfp_host_critical_create(single);
    if(single && kernel_critical)fprintf(stderr,"[RTL-CS] single diagnostic mode: shared recursive lock\n");
}
#ifdef _WIN32
static INIT_ONCE kernel_critical_once=INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK kernel_critical_once_run(PINIT_ONCE once,PVOID parameter,PVOID *context) {
    (void)once;(void)parameter;(void)context;kernel_critical_create();return TRUE;
}
static void kernel_critical_prepare(void){(void)InitOnceExecuteOnce(&kernel_critical_once,kernel_critical_once_run,NULL,NULL);}
#else
static pthread_once_t kernel_critical_once=PTHREAD_ONCE_INIT;
static void kernel_critical_prepare(void){if(pthread_once(&kernel_critical_once,kernel_critical_create))tsfp_kernel_rtl_fault(TSFP_CRITICAL_HOST_FAILURE);}
#endif
static void kernel_critical_stack(const tsfp_guest_vm *vm,const tsfp_kernel_rtl_site *site) {
    uint32_t i,shown=0;
    if(!site || !site->stack || site->code_begin>=site->code_end)return;
    for(i=0;i<256 && shown<10;++i) {
        uint64_t address=(uint64_t)site->stack+4*i;const unsigned char *p;uint32_t value;
        if(address>UINT32_MAX-3)break;
        p=vm->resolve(vm->host,(uint32_t)address,4,0);if(!p)break;
        value=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
        if(value>=site->code_begin && value<site->code_end){fprintf(stderr,"[RTL-CS] stack candidate %08x\n",value);++shown;}
    }
}
static void kernel_critical_trace(const tsfp_guest_vm *vm,uint32_t address,enum tsfp_kernel_critical_action action,const tsfp_kernel_rtl_site *site) {
    int watching=kernel_watch_enabled && kernel_watch_address==address;
    if(watching || kernel_trace_all) {
        fprintf(stderr,"[RTL-CS] operation%u guest%08x site%08x\n",(unsigned)action,address,site?site->caller:0);
        if(watching)kernel_critical_stack(vm,site);
    }
}
enum tsfp_critical_result tsfp_kernel_rtl_critical(const tsfp_guest_vm *vm,uint32_t address,enum tsfp_kernel_critical_action action,const tsfp_kernel_rtl_site *site) {
    void *identity;tsfp_critical_registry *registry;enum tsfp_critical_result result;tsfp_critical_view view;
    if((unsigned)action>(unsigned)TSFP_KERNEL_CRITICAL_TRY || !vm || !vm->resolve || !address)return TSFP_CRITICAL_INVALID;
    /* Identity only: compatibility policy never dereferences a presumed native
     * CRITICAL_SECTION or modifies an unknown guest field layout. */
    identity=vm->resolve(vm->host,address,1,0);if(!identity)return TSFP_CRITICAL_INVALID;
    kernel_critical_prepare();registry=tsfp_host_critical_shared(kernel_critical);if(!registry)return TSFP_CRITICAL_NO_MEMORY;
    kernel_critical_trace(vm,address,action,site);
    if(action==TSFP_KERNEL_CRITICAL_INIT)return tsfp_critical_initialize(registry,(uintptr_t)identity);
    if(action==TSFP_KERNEL_CRITICAL_LEAVE)return tsfp_critical_leave(registry,(uintptr_t)identity);
    result=tsfp_critical_enter(registry,(uintptr_t)identity,site?site->caller:0,1);
    if(result!=TSFP_CRITICAL_BUSY || action==TSFP_KERNEL_CRITICAL_TRY)return result;
    if(tsfp_critical_snapshot(registry,(uintptr_t)identity,&view) && view.busy<=16) {
        fprintf(stderr,"[RTL-CS] wait guest%08x owner%llu site%08x depth%u\n",address,(unsigned long long)view.owner,view.site,view.depth);
        kernel_critical_stack(vm,site);fflush(stderr);
    }
    return tsfp_critical_enter(registry,(uintptr_t)identity,site?site->caller:0,0);
}
static uint32_t kernel_error_fallback(void *context,uint32_t status) {
    (void)context;
#ifdef _WIN32
    typedef ULONG (WINAPI *native_error)(LONG);
    HMODULE module=GetModuleHandleW(L"ntdll.dll");FARPROC raw=module?GetProcAddress(module,"RtlNtStatusToDosError"):NULL;
    native_error function=NULL;
    _Static_assert(sizeof(function)==sizeof(raw),"Windows function pointer ABI");
    if(raw){memcpy(&function,&raw,sizeof(function));return (uint32_t)function((LONG)status);}
#else
    xbox_log(XBOX_LOG_WARN,XBOX_LOG_RTL,"RtlNtStatusToDosError unmapped %08x",status);
#endif
    return 317;
}
uint32_t tsfp_kernel_rtl_error(uint32_t status){return tsfp_rtl_status_error(status,kernel_error_fallback,NULL);}
void tsfp_kernel_rtl_rip(const tsfp_guest_vm *vm,uint32_t api,uint32_t expression,uint32_t message) {
    char a[256],b[256],c[512];
    (void)tsfp_rtl_text(vm,api,a,sizeof(a));(void)tsfp_rtl_text(vm,expression,b,sizeof(b));(void)tsfp_rtl_text(vm,message,c,sizeof(c));
    xbox_log(XBOX_LOG_ERROR,XBOX_LOG_RTL,"RtlRip: %s - %s: %s",a,b,c);
#ifdef _DEBUG
#ifdef _WIN32
    DebugBreak();
#else
    (void)raise(SIGTRAP);
#endif
#endif
}
void tsfp_kernel_rtl_fault(enum tsfp_critical_result result) {
    fprintf(stderr,"[RTL-CS] fatal host synchronization failure %u\n",(unsigned)result);fflush(stderr);exit(EXIT_FAILURE);
}
