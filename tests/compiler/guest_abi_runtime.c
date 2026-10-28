/* Original contract driver: diagnostics may report but must not alter cells. */
#include "recomp_types.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
static void normal_return(void) { g_esp+=4; }
static void damaged_return(void) { g_ebx=9;g_esp+=12; }
static int macros(void) {
    g_ebx=1;g_esi=2;g_edi=3;g_esp=100;
    RECOMP_ABI_CALL(0x10,normal_return);
    RECOMP_ABI_CALL_POP(0x20,normal_return,0);
    RECOMP_ABI_CALL(0x30,damaged_return);
    g_ebx=1;
    RECOMP_ABI_CALL_POP(0x40,damaged_return,0);
    return g_ebx!=9 || g_esi!=2 || g_edi!=3 || g_esp!=132;
}
#ifdef RECOMP_ABI_CHECK
static void emit(unsigned id) {
    g_ebx=id+1;g_esi=id+2;g_edi=id+3;g_esp=0x104;
    for(unsigned i=0;i<80;++i) {
        recomp_abi_violation_log(i,0,0,0,0x100);
        recomp_abi_violation_log(i,0,0,0,0x100);
        recomp_abi_pop_violation_log(i,0,0,0,0x100,8);
        recomp_abi_pop_violation_log(i,0,0,0,0x100,8);
    }
}
#if defined(_WIN32)
static DWORD WINAPI worker(void *arg) { emit((unsigned)(uintptr_t)arg);return 0; }
#else
static void *worker(void *arg) { emit((unsigned)(uintptr_t)arg);return NULL; }
#endif
static int parallel(void) {
#if defined(_WIN32)
    HANDLE threads[8];
    for(unsigned i=0;i<8;++i) {
        threads[i]=CreateThread(NULL,0,worker,(void*)(uintptr_t)i,0,NULL);
        if(!threads[i])return 1;
    }
    if(WaitForMultipleObjects(8,threads,TRUE,10000)!=WAIT_OBJECT_0)return 2;
    for(unsigned i=0;i<8;++i)CloseHandle(threads[i]);
#else
    pthread_t threads[8];
    for(unsigned i=0;i<8;++i)
        if(pthread_create(&threads[i],NULL,worker,(void*)(uintptr_t)i))return 1;
    for(unsigned i=0;i<8;++i)if(pthread_join(threads[i],NULL))return 2;
#endif
    return 0;
}
#endif
int main(int argc,char **argv) {
    if(argc!=2)return 99;
    if(!strcmp(argv[1],"macros"))return macros();
#ifdef RECOMP_ABI_CHECK
    for(unsigned i=0;i<ICALL_TRACE_SIZE;++i)g_icall_trace[i]=0x1000+i;
    g_icall_trace_idx=1; /* Previous four cross the ring's zero boundary. */
    if(!strcmp(argv[1],"limits")) { emit(1);emit(1);return 0; }
    if(!strcmp(argv[1],"threads"))return parallel();
    if(!strcmp(argv[1],"values")) {
        g_ebx=5;g_esi=6;g_edi=7;g_esp=100;
        recomp_abi_violation_log(0x50,1,2,3,100);
        recomp_abi_violation_log(0x50,1,2,3,100);
        recomp_abi_pop_violation_log(0x51,5,6,7,88,4);
        recomp_abi_pop_violation_log(0x52,5,6,7,100,0);
        /* Guest addition wraps even when cleanup crosses signed INT_MAX. */
        recomp_abi_pop_violation_log(0x53,5,6,7,100,0xfffffffcu);
        recomp_abi_pop_violation_log(0x54,5,6,7,0x80000064u,0x7ffffffcu);
        return g_ebx!=5 || g_esi!=6 || g_edi!=7 || g_esp!=100;
    }
#endif
    return 98;
}
