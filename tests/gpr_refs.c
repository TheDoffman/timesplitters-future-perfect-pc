/* Live cells across nested calls, callbacks, aliasing and concurrent threads. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %d %s\n",__LINE__,#x);exit(2);}}while(0)
uint32_t *bad_cell(uint32_t *p);
#define RECOMP_GENERATED_CODE
#if MODE != 0
#define RECOMP_GPR_REFS_SUPPORTED 1
#endif
#define RECOMP_USE_GPR_REFS (MODE != 5)
#if MODE == 2
#define RECOMP_GPR_ANCHORS(site) 0
#elif MODE == 3
#define RECOMP_GPR_ANCHORS(site) ((site)==0x1000u)
#elif MODE == 4
#define RECOMP_GPR_ANCHORS(site) ((site)==0x1100u)
#else
#define RECOMP_GPR_ANCHORS(site) 1
#endif
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
static RECOMP_TLS unsigned owner,traces,stores;
static RECOMP_TLS uint32_t bad_value;
uint32_t *bad_cell(uint32_t *p) {bad_value=*p;return &bad_value;}
static RECOMP_TLS RecompXmm bad_xmm;
RecompXmm *bad_xmm_cell(RecompXmm *p) {bad_xmm=*p;return &bad_xmm;}
static void trace_entry(uint32_t site) {
    ++traces;
    if(site==0x1100u) {
        uint32_t data=MEM32(g_esp+4);
        CHECK(MEM32(g_esp+8)==g_eax); /* parent writes are already globally visible */
        if(data+4<g_esp || data+4>=g_esp+12)CHECK(MEM32(data+4)==g_ecx);
        for(unsigned lane=0;lane<4;lane++)CHECK(g_xmm0.u[lane]==MEM32(data+32+lane*4));
        g_xmm0.u[2]^=3;g_xmm1.u[3]^=0x100;
        g_ebx^=0xabcdef00u+owner;g_edi^=0x80000000u;
#ifdef _WIN32
        SwitchToThread();
#else
        sched_yield();
#endif
    }
}
static void observe_store(uint32_t site,uint32_t address) {
    ++stores;
    if(site==0x100au)CHECK(g_ecx==MEM32(address));
    if(site==0x1108u)CHECK(g_edx==MEM32(address));
}
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,site) trace_entry(site)
#undef RECOMP_AFTER_STORE32
#define RECOMP_AFTER_STORE32(site,address) observe_store(site,address)
void parent(void);void child(void);
/* GENERATED_FUNCTIONS */

static uint64_t results[4];
static uint64_t hash_word(uint64_t h,uint32_t word) {return (h^word)*1099511628211ull;}
#ifdef _WIN32
static HANDLE start_event;
static volatile LONG ready;
static void barrier(void) {InterlockedIncrement(&ready);CHECK(WaitForSingleObject(start_event,10000)==WAIT_OBJECT_0);}
#else
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static unsigned ready,released;
static void barrier(void) {pthread_mutex_lock(&gate);++ready;pthread_cond_broadcast(&changed);while(!released)pthread_cond_wait(&changed,&gate);pthread_mutex_unlock(&gate);}
#endif
static void work(unsigned which) {
    owner=which;barrier();uint32_t random=17+which;uint64_t hash=1469598103934665603ull;
    uint32_t stack=0x9000+which*0x1000;
    for(unsigned step=0;step<2000;step++) {
        random=random*1664525u+1013904223u;
        uint32_t data=step%7==0?stack-8:step%7==1?stack+4:0x1000+which*0x1000+(step%4)*32;
        MEM32(data)=random;MEM32(data+4)=~random;MEM32(data+8)=0;MEM32(data+12)=1;
        MEM32(stack)=0x12345678;MEM32(stack+4)=data;MEM32(stack+8)=random^0xfedcba98u;
        g_eax=random;g_ecx=~random;g_edx=random+7;g_esp=stack;g_ebx=random^0x55555555;
        g_esi=random+99;g_edi=random^0x33333333;g_ebp=0x88776655;g_seh_ebp=g_ebp;
        RecompXmm *xs[]={&g_xmm0,&g_xmm1,&g_xmm2,&g_xmm3,&g_xmm4,&g_xmm5,&g_xmm6,&g_xmm7};
        for(unsigned r=0;r<8;r++)for(unsigned lane=0;lane<4;lane++)
            xs[r]->u[lane]=0x3f800000u+r*0x20000u+lane*0x1000u+which*0x100u;
        for(unsigned lane=0;lane<4;lane++)MEM32(data+16+lane*4)=0x3f000000u+(step&63)*0x1000u+lane*0x100u;
        parent();CHECK(g_esp==stack+4);
        uint32_t regs[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp};
        for(unsigned i=0;i<9;i++)hash=hash_word(hash,regs[i]);
        for(unsigned r=0;r<8;r++)for(unsigned lane=0;lane<4;lane++)hash=hash_word(hash,xs[r]->u[lane]);
        for(unsigned i=0;i<64;i+=4)hash=hash_word(hash,MEM32(data+i));
        for(unsigned i=0;i<48;i+=4)hash=hash_word(hash,MEM32(stack-16+i));
    }
    CHECK(traces==4000 && stores>=8000);results[which]=hash;
}
#ifdef _WIN32
static DWORD WINAPI entry(LPVOID p) {work((unsigned)(uintptr_t)p);return 0;}
#else
static void *entry(void *p) {work((unsigned)(uintptr_t)p);return NULL;}
#endif
int main(void) {
    void *memory=calloc(1,0x10000);CHECK(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
#ifdef _WIN32
    HANDLE threads[4];start_event=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(start_event);
    for(unsigned i=0;i<4;i++){threads[i]=CreateThread(NULL,0,entry,(void *)(uintptr_t)i,0,NULL);CHECK(threads[i]);}
    while(InterlockedCompareExchange(&ready,0,0)<4)Sleep(0);
    CHECK(SetEvent(start_event));CHECK(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);
    for(unsigned i=0;i<4;i++)CloseHandle(threads[i]);CloseHandle(start_event);
#else
    pthread_t threads[4];for(unsigned i=0;i<4;i++)CHECK(!pthread_create(&threads[i],NULL,entry,(void *)(uintptr_t)i));
    pthread_mutex_lock(&gate);while(ready<4)pthread_cond_wait(&changed,&gate);released=1;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
    for(unsigned i=0;i<4;i++)CHECK(!pthread_join(threads[i],NULL));
#endif
    for(unsigned i=0;i<4;i++)printf("%u:%016llx\n",i,(unsigned long long)results[i]);
    free(memory);return 0;
}
