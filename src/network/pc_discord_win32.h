/* Windows transport and worker primitives for the shared Discord protocol. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE DiscordPipe;
#define BAD_PIPE INVALID_HANDLE_VALUE
#define THREAD_RESULT DWORD WINAPI
#define THREAD_RETURN 0
static SRWLOCK lock=SRWLOCK_INIT;
static HANDLE worker,stop_event;
static volatile LONG active;
static int wait_stop(unsigned ms){return WaitForSingleObject(stop_event,ms)!=WAIT_TIMEOUT;}
static uint64_t ticks(void){return GetTickCount64();}
static unsigned process_id(void){return GetCurrentProcessId();}
static void pipe_close(DiscordPipe p){CloseHandle(p);}
static int pipe_available(DiscordPipe p,uint32_t *n){DWORD count=0;int ok=PeekNamedPipe(p,NULL,0,NULL,&count,NULL)!=0;*n=(uint32_t)count;return ok;}
static void sample_lock(void){AcquireSRWLockShared(&lock);}
static void sample_unlock(void){ReleaseSRWLockShared(&lock);}
static int submit_lock(void){return InterlockedCompareExchange(&active,0,0) && TryAcquireSRWLockExclusive(&lock);}
static void submit_unlock(void){ReleaseSRWLockExclusive(&lock);}
/* On timeout, cancel and drain the operation before its stack storage dies. */
static int transfer(HANDLE pipe,void *buffer,DWORD bytes,int writing)
{
    DWORD done=0;OVERLAPPED ov={0};BOOL ok;
    ov.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!ov.hEvent) return 0;
    ok=writing?WriteFile(pipe,buffer,bytes,&done,&ov):ReadFile(pipe,buffer,bytes,&done,&ov);
    if(!ok && GetLastError()==ERROR_IO_PENDING) {
        HANDLE waits[2]={stop_event,ov.hEvent};
        if(WaitForMultipleObjects(2,waits,FALSE,250)!=WAIT_OBJECT_0+1) {
            CancelIoEx(pipe,&ov);GetOverlappedResult(pipe,&ov,&done,TRUE);ok=FALSE;
        } else ok=GetOverlappedResult(pipe,&ov,&done,FALSE);
    }
    CloseHandle(ov.hEvent);
    return ok && done==bytes;
}

static HANDLE open_pipe(void)
{
    for(unsigned i=0;i<10;i++) {
        wchar_t name[64];HANDLE pipe;
#ifdef TSFP_DISCORD_TEST_PIPE
        swprintf(name,64,L"\\\\.\\pipe\\tsfp-discord-test");
#else
        swprintf(name,64,L"\\\\.\\pipe\\discord-ipc-%u",i);
#endif
        pipe=CreateFileW(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);
        if(pipe!=INVALID_HANDLE_VALUE) return pipe;
    }
    return INVALID_HANDLE_VALUE;
}

static int start_worker(LPTHREAD_START_ROUTINE run){
    if(worker)return 0;stop_event=CreateEventW(NULL,TRUE,FALSE,NULL);if(!stop_event)return 0;
    worker=CreateThread(NULL,0,run,NULL,0,NULL);
    if(!worker){CloseHandle(stop_event);stop_event=NULL;return 0;}InterlockedExchange(&active,1);return 1;
}
static void stop_worker(void){if(!worker)return;InterlockedExchange(&active,0);SetEvent(stop_event);
    WaitForSingleObject(worker,INFINITE);CloseHandle(worker);CloseHandle(stop_event);worker=stop_event=NULL;}
