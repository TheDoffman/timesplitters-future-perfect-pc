/* Actual Arm body is extracted by test_frame_clock.py; no game assets needed. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define BOOL int
#define FALSE 0
#define TRUE 1
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
typedef long LONG;
typedef long long LONGLONG;
typedef struct { LONGLONG QuadPart; } LARGE_INTEGER;
static volatile LONG g_flip_gate_vblanks=1;
static void *g_flip_gate_event=(void *)1;
static LONGLONG g_flip_gate_period=1000,g_flip_gate_last_vblank,now;
static LONGLONG g_flip_gate_previous;
static int g_flip_gate_strict=1,signalled=1,waits;
static LONGLONG (*g_flip_gate_idle)(LONGLONG,LONGLONG);
static int flip_gate_divisor(void){return 1;}
static LONG InterlockedCompareExchange(volatile LONG *p,LONG a,LONG b)
{(void)a;(void)b;return *p;}
static void QueryPerformanceCounter(LARGE_INTEGER *p){p->QuadPart=now;}
static void ResetEvent(void *event){(void)event;signalled=0;}
static int WaitForSingleObject(void *event,unsigned timeout)
{
    (void)event;
    if(signalled){signalled=0;return WAIT_OBJECT_0;}
    if(!timeout)return WAIT_TIMEOUT;
    ++waits;now=(now/1000+1)*1000;g_flip_gate_last_vblank=now;
    return WAIT_OBJECT_0;
}
static BOOL flip_gate_lend(void){return FALSE;}
#include "arm.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s (now=%lld waits=%d)\n",__LINE__,#x,now,waits);exit(1);}}while(0)
int main(int argc,char **argv)
{
    if(argc>1) {
        /* Start natively: Arm has never run. A native frame observes the
           current real vblank without waiting or consuming its signal. */
        now=1000+atoi(argv[1]);g_flip_gate_last_vblank=1000;signalled=1;
        xbox_Nv2aFlipGateNativeFrame();
        CHECK(waits==0 && signalled==1 && now==1000+atoi(argv[1]));
        /* The original clock has now waited for the next ISR. Its Swap
           must consume that real signal, not discard it for a second wait. */
        now=2100;g_flip_gate_last_vblank=2000;signalled=1;
        xbox_Nv2aFlipGateArm();CHECK(now==2100 && waits==0);
        now=2200;xbox_Nv2aFlipGateArm();CHECK(now==3000 && waits==1);
        puts("PASS: native-start handoff consumes real vblank; following frame remains paced");
        return 0;
    }
    /* Initial cheap frame must wait; the original 60-Hz clock is unchanged. */
    xbox_Nv2aFlipGateArm();CHECK(now==1000 && waits==1);
    /* 18.3ms CPU frame, using 1000 units per 16.67ms. A real vblank arrived
       while working. Do not discard that event and add another whole tick. */
    now=2100;g_flip_gate_last_vblank=2000;signalled=1;
    xbox_Nv2aFlipGateArm();CHECK(now==2100 && waits==1);
    /* Following cheap frame still waits, so late recovery is not uncapping. */
    now=2200;xbox_Nv2aFlipGateArm();CHECK(now==3000 && waits==2);
    /* No signal means no bypass, even when the CPU frame is late. */
    now=4100;signalled=0;xbox_Nv2aFlipGateArm();CHECK(now==5000 && waits==3);
    puts("PASS: cheap/late frames, real-vblank requirement, no retired idle hook");
    return 0;
}
