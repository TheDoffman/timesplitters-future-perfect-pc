/* Shared bounded disk-read queue. Renderer callbacks only run from pump().
 * Submit, cancel and pump belong to the single renderer thread. Owners must
 * cancel before destruction/content changes. The worker never dereferences an
 * owner or creates graphics resources. */
#ifndef TSFP_TEXTURE_PACK_STREAM_H
#define TSFP_TEXTURE_PACK_STREAM_H
#include "texture_pack_io.h"
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static SRWLOCK tsfp_stream_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE tsfp_stream_changed = CONDITION_VARIABLE_INIT;
static HANDLE tsfp_stream_thread;
static void tsfp_stream_enter(void) { AcquireSRWLockExclusive(&tsfp_stream_lock); }
static void tsfp_stream_leave(void) { ReleaseSRWLockExclusive(&tsfp_stream_lock); }
static void tsfp_stream_signal(void) { WakeAllConditionVariable(&tsfp_stream_changed); }
static void tsfp_stream_wait(void) { SleepConditionVariableSRW(&tsfp_stream_changed,&tsfp_stream_lock,INFINITE,0); }
#define TSFP_STREAM_THREAD_RESULT DWORD WINAPI
#else
#include <pthread.h>
static pthread_mutex_t tsfp_stream_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tsfp_stream_changed = PTHREAD_COND_INITIALIZER;
static pthread_t tsfp_stream_thread;
static void tsfp_stream_enter(void) { pthread_mutex_lock(&tsfp_stream_lock); }
static void tsfp_stream_leave(void) { pthread_mutex_unlock(&tsfp_stream_lock); }
static void tsfp_stream_signal(void) { pthread_cond_broadcast(&tsfp_stream_changed); }
static void tsfp_stream_wait(void) { pthread_cond_wait(&tsfp_stream_changed,&tsfp_stream_lock); }
#define TSFP_STREAM_THREAD_RESULT void *
#endif

#ifndef TSFP_STREAM_READ
#define TSFP_STREAM_READ_DEFAULT
#define TSFP_STREAM_READ tsfp_pack_read
#endif
#define TSFP_STREAM_SLOTS 64
/* At most two ready payloads plus one in-flight read. tsfp_pack_read bounds
 * each allocation at 24 MiB, so staging stays at or below 72 MiB. */
#define TSFP_STREAM_READY_LIMIT 2
typedef void (*TsfpStreamApply)(void *owner,uint64_t key,const unsigned char *data,const TsfpDDS *dds,TsfpTextureTrace *trace);
enum { TSFP_STREAM_EMPTY, TSFP_STREAM_QUEUED, TSFP_STREAM_READING, TSFP_STREAM_READY };
typedef struct {
    int state;
    void *owner;
    uint64_t key, order;
    uint32_t width,height;
    char path[4096];
    unsigned char *data;
    TsfpDDS dds;
    TsfpTextureTrace trace;
    TsfpStreamApply apply;
} TsfpStreamJob;
static TsfpStreamJob tsfp_stream_jobs[TSFP_STREAM_SLOTS];
static uint64_t tsfp_stream_order;
static int tsfp_stream_initialized,tsfp_stream_started,tsfp_stream_stopping;

static TSFP_STREAM_THREAD_RESULT tsfp_stream_worker(void *unused)
{
    (void)unused;
    tsfp_stream_enter();
    while(!tsfp_stream_stopping) {
        TsfpStreamJob *job=NULL;
        unsigned ready=0;
        for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++) {
            TsfpStreamJob *candidate=&tsfp_stream_jobs[i];
            if(candidate->state==TSFP_STREAM_READY)ready++;
            if(candidate->state==TSFP_STREAM_QUEUED && (!job || candidate->order<job->order))job=candidate;
        }
        if(!job || ready>=TSFP_STREAM_READY_LIMIT) {tsfp_stream_wait();continue;}
        job->state=TSFP_STREAM_READING;
        /* Cancellation leaves a reading slot intact until this read ends. */
        tsfp_stream_leave();
        TsfpDDS dds={0};
#ifdef TSFP_STREAM_READ_DEFAULT
        unsigned char *data=tsfp_pack_read_traced(job->path,job->width,job->height,TSFP_PACK_RESIDENT_BUDGET,&dds,&job->trace);
#else
        unsigned char *data=TSFP_STREAM_READ(job->path,job->width,job->height,TSFP_PACK_RESIDENT_BUDGET,&dds);
#endif
        tsfp_stream_enter();
        if(!job->owner || tsfp_stream_stopping) {
            memset(job,0,sizeof *job);
            tsfp_stream_leave();free(data);tsfp_stream_enter();
        } else {
            job->data=data;job->dds=dds;job->state=TSFP_STREAM_READY;
        }
    }
    tsfp_stream_leave();
    return 0;
}

/* Process teardown may wait for the one current disk read. Normal rendering,
 * mode changes and resource cancellation never wait for disk completion. */
static void tsfp_stream_shutdown(void)
{
    tsfp_stream_enter();
    if(!tsfp_stream_started) {tsfp_stream_leave();return;}
    tsfp_stream_stopping=1;tsfp_stream_signal();tsfp_stream_leave();
#ifdef _WIN32
    WaitForSingleObject(tsfp_stream_thread,INFINITE);CloseHandle(tsfp_stream_thread);
#else
    pthread_join(tsfp_stream_thread,NULL);
#endif
    tsfp_stream_enter();
    tsfp_stream_started=0;
    for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++) {
        free(tsfp_stream_jobs[i].data);memset(&tsfp_stream_jobs[i],0,sizeof tsfp_stream_jobs[i]);
    }
    tsfp_stream_leave();
}

/* Called with the queue lock held. Failure/explicit opt-out preserves the
 * synchronous loader; callers distinguish this from a temporarily full queue. */
static void tsfp_stream_start(void)
{
    if(tsfp_stream_initialized)return;
    tsfp_stream_initialized=1;
    const char *setting=getenv("TSFP_TEXTURE_STREAMING");
    if(setting && !strcmp(setting,"0"))return;
    if(atexit(tsfp_stream_shutdown))return;
#ifdef _WIN32
    tsfp_stream_thread=CreateThread(NULL,0,tsfp_stream_worker,NULL,0,NULL);
    tsfp_stream_started=tsfp_stream_thread!=NULL;
#else
    tsfp_stream_started=pthread_create(&tsfp_stream_thread,NULL,tsfp_stream_worker,NULL)==0;
#endif
}

/* 1 queued/already queued, 0 retry later, -1 use the synchronous fallback. */
static int tsfp_stream_submit_traced(void *owner,uint64_t key,const char *path,
                              uint32_t width,uint32_t height,TsfpStreamApply apply,const TsfpTextureTrace *trace)
{
    if(!owner || !apply || !path || strlen(path)>=sizeof tsfp_stream_jobs[0].path)return -1;
    tsfp_stream_enter();tsfp_stream_start();
    if(!tsfp_stream_started || tsfp_stream_stopping) {tsfp_stream_leave();return -1;}
    TsfpStreamJob *empty=NULL;
    for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++) {
        TsfpStreamJob *job=&tsfp_stream_jobs[i];
        if(job->state!=TSFP_STREAM_EMPTY && job->owner==owner) {
            int same=job->key==key;tsfp_stream_leave();return same;
        }
        if(job->state==TSFP_STREAM_EMPTY && !empty)empty=job;
    }
    if(!empty) {tsfp_stream_leave();return 0;}
    if(trace)empty->trace=*trace;
    empty->owner=owner;empty->key=key;empty->order=++tsfp_stream_order;
    empty->width=width;empty->height=height;empty->apply=apply;
    strcpy(empty->path,path);empty->state=TSFP_STREAM_QUEUED;
    tsfp_stream_signal();tsfp_stream_leave();return 1;
}

static int tsfp_stream_submit(void *owner,uint64_t key,const char *path,
                              uint32_t width,uint32_t height,TsfpStreamApply apply)
{
    return tsfp_stream_submit_traced(owner,key,path,width,height,apply,NULL);
}

static void tsfp_stream_cancel(void *owner)
{
    tsfp_stream_enter();
    for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++) {
        TsfpStreamJob *job=&tsfp_stream_jobs[i];
        if(job->state==TSFP_STREAM_EMPTY || job->owner!=owner)continue;
        if(job->state==TSFP_STREAM_READING)job->owner=NULL;
        else {free(job->data);memset(job,0,sizeof *job);}
    }
    tsfp_stream_signal();tsfp_stream_leave();
}

/* Called once per completed renderer frame. At most one successful payload
 * is uploaded per frame; failed/missing reads are cheap completion callbacks.
 * Callback execution/freeing happens outside the queue lock on this caller. */
static void tsfp_stream_pump(void)
{
    for(unsigned handled=0;handled<TSFP_STREAM_SLOTS;handled++) {
        TsfpStreamJob result={0};
        tsfp_stream_enter();
        TsfpStreamJob *ready=NULL;
        for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++) {
            TsfpStreamJob *job=&tsfp_stream_jobs[i];
            if(job->state==TSFP_STREAM_READY && (!ready || job->order<ready->order))ready=job;
        }
        if(ready) {result=*ready;memset(ready,0,sizeof *ready);tsfp_stream_signal();}
        tsfp_stream_leave();
        if(!ready)return;
        int had_data=result.data!=NULL;
        result.apply(result.owner,result.key,result.data,&result.dds,&result.trace);
        free(result.data);
        if(had_data)return;
    }
}
#endif
