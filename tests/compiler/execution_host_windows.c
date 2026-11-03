/* Original real-host binding test. No game or personal files are accessed. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define RECOMP_TLS __declspec(thread)
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(2);}}while(0)
#include "guest_execution_host.inc"
#include "guest_kernel_execution.inc"
static volatile LONG in_guest,arrived,done;
static HANDLE start_work;
static DWORD WINAPI worker(LPVOID arg) {
    unsigned i;(void)arg;InterlockedIncrement(&arrived);
    CHECK(WaitForSingleObject(start_work,5000)==WAIT_OBJECT_0);
    for(i=0;i<100;++i) {
        tsfp_guest_execution_snapshot saved;
        xbox_GuestLockEnter();xbox_GuestLockEnter();xbox_GuestLiftedEnter();
        CHECK(InterlockedIncrement(&in_guest)==1);
        CHECK(InterlockedDecrement(&in_guest)==0);
        CHECK(tsfp_guest_execution_suspend(&tsfp_kernel_execution_ops,&saved));
        CHECK(tsfp_guest_execution_restore(&tsfp_kernel_execution_ops,&saved));
        xbox_GuestLiftedExit();
    }
    InterlockedIncrement(&done);return 0;
}
static DWORD WINAPI gated_worker(LPVOID arg) {
    (void)arg;InterlockedIncrement(&arrived);
    xbox_GuestLiftedEnter();InterlockedIncrement(&done);xbox_GuestLiftedLeave();return 0;
}
int main(void) {
    HANDLE threads[4];unsigned i;ULONGLONG start;DWORD_PTR original;
    CHECK(!_putenv_s("RECOMP_GUEST_LOCK","1"));CHECK(!_putenv_s("RECOMP_GUEST_CONCURRENCY","1"));
    CHECK(!_putenv_s("RECOMP_DISPATCH_LOCK","1"));CHECK(!_putenv_s("RECOMP_GUEST_ONE_CPU","1"));
    xbox_GuestLockInit();CHECK(execution_host_park_on && execution_host_cpu);
    original=SetThreadAffinityMask(GetCurrentThread(),execution_host_cpu);CHECK(original);
    CHECK(SetThreadAffinityMask(GetCurrentThread(),original));
    start_work=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(start_work);
    for(i=0;i<4;++i){threads[i]=CreateThread(NULL,0,worker,NULL,0,NULL);CHECK(threads[i]);}
    SetEvent(start_work);CHECK(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);
    CHECK(done==4 && arrived==4 && !in_guest && !execution_host_running && !execution_host_park_running);
    CHECK(execution_host_peak==1 && !execution_host_overlaps);
    for(i=0;i<4;++i)CloseHandle(threads[i]);CloseHandle(start_work);
    done=arrived=0;xbox_DispatchParkOthers();xbox_DispatchParkOthers();
    threads[0]=CreateThread(NULL,0,gated_worker,NULL,0,NULL);CHECK(threads[0]);start=GetTickCount64();
    while(!execution_host_read(&arrived) && GetTickCount64()-start<1000)SwitchToThread();
    CHECK(arrived==1 && !done);xbox_DispatchReleaseOthers();
    CHECK(execution_host_read(&execution_host_park_owner)==(LONG)GetCurrentThreadId());
    xbox_DispatchReleaseOthers();CHECK(WaitForSingleObject(threads[0],5000)==WAIT_OBJECT_0);
    CHECK(done==1 && !execution_host_park_running);CloseHandle(threads[0]);
    xbox_GuestConcurrencyReport();puts("PASS: native Windows execution host");return 0;
}
