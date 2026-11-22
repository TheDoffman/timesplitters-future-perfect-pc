#ifndef TSFP_GUEST_DISPATCH_SDL_H
#define TSFP_GUEST_DISPATCH_SDL_H
#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Guest TLS, memory/kernel setup and crash stack belong in entry, not on this
 * owner thread. The owner stays responsive even when the guest busy-polls. */
typedef struct {int (*entry)(void *);void *context;SDL_AtomicInt done;int result,returned;} TsfpGuestDispatch;
typedef struct {unsigned serial;Uint64 active_until;} TsfpDispatchActivity;
/* Keep a burst of synchronous GPU calls moving without a millisecond sleep
 * between each one. After two idle timeout quanta, return to bounded waits. */
static int tsfp_dispatch_is_idle(TsfpDispatchActivity *activity,unsigned serial,Uint64 now)
{
    if(serial!=activity->serial){activity->serial=serial;activity->active_until=now+2*SDL_NS_PER_MS;return 0;}
    return now>=activity->active_until;
}
static void tsfp_guest_dispatch_done(void *opaque)
{TsfpGuestDispatch *job=opaque;SDL_SetAtomicInt(&job->done,1);}
static void *tsfp_guest_dispatch_entry(void *opaque)
{
    TsfpGuestDispatch *job=opaque;
    pthread_cleanup_push(tsfp_guest_dispatch_done,job);
    job->result=job->entry(job->context);job->returned=1;
    pthread_cleanup_pop(1);return NULL;
}
static int tsfp_native_dispatch_guest(int (*entry)(void *),void *context,unsigned (*progress)(void))
{
    if(!entry || !SDL_IsMainThread())return 2;
    if(!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        fprintf(stderr,"[native-rhi] SDL dispatch initialization failed: %s\n",SDL_GetError());return 2;
    }
    TsfpGuestDispatch job={entry,context,{0},2,0};pthread_t thread;pthread_attr_t attributes;
    int error=pthread_attr_init(&attributes);
    if(!error) {
        error=pthread_attr_setstacksize(&attributes,16u*1024u*1024u);
        if(!error)error=pthread_create(&thread,&attributes,tsfp_guest_dispatch_entry,&job);
        pthread_attr_destroy(&attributes);
    }
    if(error){fprintf(stderr,"[native-rhi] guest worker creation failed: %s\n",strerror(error));return 2;}
    fprintf(stderr,"[native-rhi] guest worker with continuous main-thread SDL dispatch\n");
    TsfpDispatchActivity activity={progress?progress():0,SDL_GetTicksNS()+2*SDL_NS_PER_MS};
    while(!SDL_GetAtomicInt(&job.done)) {
        if(!progress){SDL_WaitEventTimeout(NULL,1);continue;}
        SDL_PumpEvents(); /* Does not remove queued keyboard/window input. */
        if(tsfp_dispatch_is_idle(&activity,progress(),SDL_GetTicksNS()))SDL_WaitEventTimeout(NULL,1);
    }
    void *status=NULL;error=pthread_join(thread,&status);
    if(error){fprintf(stderr,"[native-rhi] guest worker join failed: %s\n",strerror(error));return 2;}
    if(job.returned)return job.result;
    return status==PTHREAD_CANCELED?2:(int)(intptr_t)status;
}
#endif
