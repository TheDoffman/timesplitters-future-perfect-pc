/* Native desktop dispatch must not hold an event lock or enter recursively. */
#include "platform/win32_compat.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
static pthread_t owner;
static HANDLE target, second, nested;
static int mode, pumps, queries;
static void pause_ms(unsigned value){struct timespec t={0,(long)value*1000000};nanosleep(&t,NULL);}
static int service(DWORD budget)
{
    if(!pthread_equal(pthread_self(),owner))return 0;
    if(!budget){queries++;return 1;}
    assert(budget==1);pumps++;
    if(mode==1)assert(SetEvent(target));
    if(mode==2){assert(WaitForSingleObject(nested,1)==WAIT_TIMEOUT);assert(SetEvent(target));}
    if(mode==3)assert(SetEvent(pumps==1?target:second));
    pause_ms(budget);return 1;
}
static void reset(int next){mode=next;pumps=queries=0;}
static void *foreign(void *arg){DWORD *result=arg;*result=WaitForSingleObject(target,1000);return NULL;}
static DWORD WINAPI thread_body(void *arg){assert(WaitForSingleObject(arg,1000)==WAIT_OBJECT_0);return 42;}
int main(void)
{
    owner=pthread_self();target=CreateEventA(NULL,FALSE,FALSE,NULL);assert(target);
    w32_SetHostWaitPump(service);reset(1);
    assert(WaitForSingleObject(target,0)==WAIT_TIMEOUT && !pumps && !queries);
    assert(WaitForSingleObject(target,INFINITE)==WAIT_OBJECT_0 && pumps==1);
    assert(WaitForSingleObject(target,0)==WAIT_TIMEOUT); /* Auto-reset consumed once. */
    nested=CreateEventA(NULL,FALSE,FALSE,NULL);reset(2);
    assert(WaitForSingleObject(target,INFINITE)==WAIT_OBJECT_0 && pumps==1 && queries==1);
    CloseHandle(nested);
    reset(0);ULONGLONG start=GetTickCount64();
    assert(WaitForSingleObject(target,25)==WAIT_TIMEOUT);
    ULONGLONG elapsed=GetTickCount64()-start;assert(elapsed>=20 && elapsed<1000 && pumps>0);
    reset(1);DWORD result=WAIT_FAILED;pthread_t worker;
    assert(!pthread_create(&worker,NULL,foreign,&result));pause_ms(5);assert(SetEvent(target));
    assert(!pthread_join(worker,NULL));assert(result==WAIT_OBJECT_0 && !pumps && !queries);
    second=CreateEventA(NULL,FALSE,FALSE,NULL);HANDLE any[2]={target,second};
    reset(3);assert(WaitForMultipleObjects(2,any,FALSE,1000)==WAIT_OBJECT_0);
    CloseHandle(target);CloseHandle(second);
    target=CreateEventA(NULL,TRUE,FALSE,NULL);second=CreateEventA(NULL,TRUE,FALSE,NULL);
    HANDLE all[2]={target,second};reset(3);
    assert(WaitForMultipleObjects(2,all,TRUE,1000)==WAIT_OBJECT_0 && pumps==2);
    assert(WaitForSingleObject(target,0)==WAIT_OBJECT_0 && WaitForSingleObject(second,0)==WAIT_OBJECT_0);
    CloseHandle(target);CloseHandle(second);
    target=CreateEventA(NULL,FALSE,FALSE,NULL);reset(1);
    HANDLE thread=CreateThread(NULL,0,thread_body,target,0,NULL);assert(thread);
    assert(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0);
    DWORD code=0;assert(GetExitCodeThread(thread,&code) && code==42);CloseHandle(thread);CloseHandle(target);
    w32_SetHostWaitPump(NULL);target=CreateEventA(NULL,FALSE,FALSE,NULL);reset(1);
    assert(WaitForSingleObject(target,1)==WAIT_TIMEOUT && !pumps && !queries);CloseHandle(target);
    puts("PASS: owner dispatch, unlocked callback, recursion guard, deadlines, foreign waits, auto/manual reset, multiple and thread waits");
    return 0;
}
