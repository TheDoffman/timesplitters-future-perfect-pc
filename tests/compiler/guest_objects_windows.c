/* Actual Windows event wait held by a project-owned operation lease. */
#include <windows.h>
#include "guest_objects.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s (OS %lu)\n",__LINE__,#x,GetLastError());exit(1); } } while(0)
static tsfp_guest_objects store;
static tsfp_guest_handle_record handles[4];
static tsfp_guest_object_record objects[4];
static volatile LONG closes;
static HANDLE ready;
static uint32_t token;
static int close_host(void *ctx,uintptr_t host) {
    CHECK(ctx==&store);
    CHECK(CloseHandle((HANDLE)host));
    CHECK(InterlockedIncrement(&closes)==1);
    return 1;
}
static DWORD WINAPI wait_thread(void *ignored) {
    tsfp_guest_object_lease lease={0};(void)ignored;
    CHECK(tsfp_guest_objects_acquire(&store,token,&lease));
    CHECK(SetEvent(ready));
    CHECK(WaitForSingleObject((HANDLE)lease.host,5000)==WAIT_OBJECT_0);
    CHECK(!InterlockedCompareExchange(&closes,0,0));
    CHECK(tsfp_guest_objects_release(&lease));return 0;
}
int main(void) {
    HANDLE event,thread;uint32_t duplicate;DWORD flags;
    tsfp_guest_object_lease signal={0},rejected={0};
    CHECK(tsfp_guest_objects_init(&store,handles,objects,4,close_host,NULL,&store));
    ready=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(ready);
    event=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(event);
    CHECK(tsfp_guest_objects_adopt(&store,(uintptr_t)event,2,0,&token));
    CHECK(tsfp_guest_objects_duplicate(&store,token,0,&duplicate) && token!=duplicate);
    CHECK(tsfp_guest_objects_acquire(&store,duplicate,&signal));
    thread=CreateThread(NULL,0,wait_thread,NULL,0,NULL);CHECK(thread);
    CHECK(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0);
    CHECK(tsfp_guest_objects_close(&store,token));CHECK(tsfp_guest_objects_close(&store,duplicate));
    CHECK(!tsfp_guest_objects_acquire(&store,duplicate,&rejected));
    CHECK(GetHandleInformation(event,&flags) && !InterlockedCompareExchange(&closes,0,0));
    CHECK(SetEvent((HANDLE)signal.host));CHECK(tsfp_guest_objects_release(&signal));
    CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    CHECK(InterlockedCompareExchange(&closes,0,0)==1);
    CHECK(!GetHandleInformation(event,&flags) && GetLastError()==ERROR_INVALID_HANDLE);
    CHECK(CloseHandle(thread));CHECK(CloseHandle(ready));
    CHECK(!tsfp_guest_objects_collect(&store));
    puts("PASS: actual Windows wait and signal survive token close; final lease closes host event once");
    return 0;
}
