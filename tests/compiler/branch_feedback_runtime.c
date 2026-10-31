#define _CRT_SECURE_NO_WARNINGS
#include "tsfp_branch_feedback.h"
#include <stdio.h>
#include <string.h>
#ifdef RECOMP_ICALL_FEEDBACK
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif
static unsigned address_reads,flag_reads,order;
static uint32_t read_address(void) { ++address_reads;return RECOMP_ICALL_FB_BASE; }
static unsigned read_flags(void) { ++flag_reads;return 1; }
static uint32_t read_site(void) { order=order*10+1;return 0xabcdefu; }
static uint32_t read_target(void) { order=order*10+2;return 0xfedcbau; }
static void observations(unsigned thread) {
    for(unsigned i=0;i<20000;++i) {
        RECOMP_ICALL_OBSERVE(RECOMP_ICALL_FB_BASE+(i%4096),(thread&1u)+1u);
        RECOMP_ICALL_OBSERVE_SITE(0x800000u+(i%128),thread+1u);
        tsfp_feedback_guard(thread&1u);
    }
}
#ifdef _WIN32
static DWORD WINAPI worker(LPVOID value) { observations((unsigned)(uintptr_t)value);return 0; }
#else
static void *worker(void *value) { observations((unsigned)(uintptr_t)value);return NULL; }
#endif

int main(int argc,char **argv) {
    if(argc!=3)return 10;
    if(strcmp(argv[1],"exit")==0) {
        recomp_icall_feedback_init();recomp_icall_feedback_init();
        RECOMP_ICALL_OBSERVE(RECOMP_ICALL_FB_BASE+7u,3);
        return 0;
    }
    if(strcmp(argv[1],"threads")==0) {
#ifdef _WIN32
        HANDLE threads[8];
        for(unsigned i=0;i<8;++i) { threads[i]=CreateThread(NULL,0,worker,(LPVOID)(uintptr_t)i,0,NULL);if(!threads[i])return 11; }
        if(WaitForMultipleObjects(8,threads,TRUE,30000)!=WAIT_OBJECT_0)return 12;
        for(unsigned i=0;i<8;++i)CloseHandle(threads[i]);
#else
        pthread_t threads[8];
        for(unsigned i=0;i<8;++i)if(pthread_create(&threads[i],NULL,worker,(void *)(uintptr_t)i))return 11;
        for(unsigned i=0;i<8;++i)if(pthread_join(threads[i],NULL))return 12;
#endif
        if(g_icall_guard_hits!=80000 || g_icall_guard_misses!=80000)return 13;
    } else if(strcmp(argv[1],"capacity")==0) {
        for(uint32_t i=1;i<=32770;++i)recomp_icall_observe_site(i,i^UINT32_C(0x80000000));
    } else {
        RECOMP_ICALL_OBSERVE(read_address(),read_flags());
        RECOMP_ICALL_OBSERVE(RECOMP_ICALL_FB_BASE,2);
        RECOMP_ICALL_OBSERVE(RECOMP_ICALL_FB_BASE-1u,read_flags());
        RECOMP_ICALL_OBSERVE(RECOMP_ICALL_FB_BASE+RECOMP_ICALL_FB_SIZE,read_flags());
        RECOMP_ICALL_OBSERVE(RECOMP_ICALL_FB_BASE+RECOMP_ICALL_FB_SIZE-1u,2);
        RECOMP_ICALL_OBSERVE(UINT64_C(0x100010001),1);
        if(address_reads!=1 || flag_reads!=1)return 14;
        RECOMP_ICALL_OBSERVE_SITE(read_site(),read_target());
        if(order!=12)return 15;
        for(uint32_t i=1;i<=7;++i) {
            RECOMP_ICALL_OBSERVE_SITE(0x4444u,i);
            RECOMP_ICALL_OBSERVE_SITE(0x4444u,i);
        }
        RECOMP_ICALL_OBSERVE_SITE(0,123);
        RECOMP_ICALL_OBSERVE_SITE(0xfffffff0u,0);
        RECOMP_ICALL_OBSERVE_SITE(0xfffffff0u,UINT32_MAX);
        for(uint32_t i=1;i<=128;++i)RECOMP_ICALL_OBSERVE_SITE(0x1001u*i,0x9000u+i);
        g_icall_guard_hits=UINT64_MAX;g_icall_guard_misses=9;
        tsfp_feedback_guard(1);tsfp_feedback_guard(0);
        if(g_icall_guard_hits!=0 || g_icall_guard_misses!=10)return 16;
        recomp_icall_feedback_dump(NULL);recomp_icall_feedback_dump("");
    }
    recomp_icall_feedback_dump(argv[2]);
    puts("recorder completed");
    return 0;
}
#else
int main(void) {
    RECOMP_ICALL_OBSERVE(unknown_address(),unknown_flags);
    RECOMP_ICALL_OBSERVE_SITE(unknown_site(),unknown_target());
    RECOMP_ICALL_FEEDBACK_INIT();RECOMP_ICALL_FEEDBACK_DUMP();
    puts("disabled recorder completed");return 0;
}
#endif
