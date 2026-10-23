/* Exercise production idle-wait selection with deterministic host adapters. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef void *HANDLE;
typedef int32_t LONG;
typedef long long LONGLONG;
typedef struct { LONGLONG QuadPart; } LARGE_INTEGER;
#define FALSE 0
#define TIMER_ALL_ACCESS 1
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(1); } } while (0)
static HANDLE g_nv2a_ack_wake=(HANDLE)(uintptr_t)1;
static LONG g_ack_activity;
static long long counter=100000;
static unsigned timer_sets, multi_waits, single_waits, native_waits, sleeps;
static int arm_ok=1;
static void Sleep(unsigned ms) { CHECK(ms<=1); ++sleeps; }
static HANDLE CreateWaitableTimerExW(void *a,void *b,unsigned c,unsigned d)
{ (void)a;(void)b;(void)c;(void)d;return (HANDLE)(uintptr_t)2; }
static HANDLE CreateWaitableTimerW(void *a,int b,void *c)
{(void)a;(void)b;(void)c;return (HANDLE)(uintptr_t)2;}
static int QueryPerformanceFrequency(LARGE_INTEGER *f) {f->QuadPart=1000000;return 1;}
static int QueryPerformanceCounter(LARGE_INTEGER *f) {f->QuadPart=counter;return 1;}
static int SetWaitableTimer(HANDLE h,LARGE_INTEGER *due,int p,void *a,void *b,int c)
{ (void)p;(void)a;(void)b;(void)c;CHECK(h==(HANDLE)(uintptr_t)2 && due->QuadPart==-5000);++timer_sets;return arm_ok; }
static unsigned WaitForSingleObject(HANDLE h,unsigned ms)
{ CHECK(h==(HANDLE)(uintptr_t)2 && ms==50);++single_waits;return 0; }
static unsigned WaitForMultipleObjects(unsigned n,const HANDLE *h,int all,unsigned ms)
{ CHECK(n==2 && h[0]==g_nv2a_ack_wake && h[1]==(HANDLE)(uintptr_t)2 && !all && ms==50);++multi_waits;return 0; }
#ifdef TEST_NATIVE_WAIT
#undef _WIN32
static unsigned w32_WaitForEventMicroseconds(HANDLE h,uint32_t us)
{CHECK(h==g_nv2a_ack_wake && us==500);++native_waits;return 0;}
#else
#ifndef _WIN32
#define _WIN32
#endif
#endif
/* ACTUAL_WAIT_FUNCTION */
int main(void)
{
    nv2a_ack_wait();
#ifdef TEST_NATIVE_WAIT
    CHECK(native_waits==1 && timer_sets==0 && multi_waits==0);
#else
    CHECK(multi_waits==1 && timer_sets==1 && single_waits==0 && native_waits==0);
#endif
    /* No event: retain the original timer fallback, including arm failure. */
    g_nv2a_ack_wake=NULL;
    nv2a_ack_wait();CHECK(single_waits==1);
    arm_ok=0;nv2a_ack_wait();CHECK(sleeps==1);
    /* Busy service remains bounded by the original one-millisecond window. */
    g_ack_activity++;nv2a_ack_wait();CHECK(sleeps==2);
    counter+=500;nv2a_ack_wait();CHECK(sleeps==3);
    counter+=500;arm_ok=1;nv2a_ack_wait();CHECK(single_waits==2);
    puts("PASS: interruptible wait, fallback and original busy window");
    return 0;
}
