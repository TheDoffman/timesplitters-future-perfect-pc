#ifndef TSFP_OPENGL_PRESENT_H
#define TSFP_OPENGL_PRESENT_H
#include <stdio.h>
/* Driver swap control is optional. Preserve the requested preference, report
 * a refusal once per change and let the actual swap decide presentation status. */
typedef struct {int attempted,requested,swap_failed;} TsfpGlPresentState;
typedef int (*TsfpGlSetInterval)(void *,int,const char **);
typedef int (*TsfpGlSwap)(void *,const char **);
static int tsfp_gl_present_driver(TsfpGlPresentState *state,void *context,int interval,
                                  TsfpGlSetInterval set_interval,TsfpGlSwap swap)
{
    if(!state || !context || !set_interval || !swap)return -1;
    interval=interval?1:0;
    if(!state->attempted || state->requested!=interval) {
        const char *reason=NULL;state->attempted=1;state->requested=interval;
        if(!set_interval(context,interval,&reason)) {
            fprintf(stderr,"[RHI] opengl: swap interval %d unavailable (%s); using driver presentation behavior\n",interval,reason?reason:"driver refused request");
            fflush(stderr);
        }
    }
    const char *reason=NULL;
    if(!swap(context,&reason)) {
        if(!state->swap_failed){fprintf(stderr,"[RHI] ERROR: opengl presentation failed (%s)\n",reason?reason:"driver swap failed");fflush(stderr);}
        state->swap_failed=1;return -1;
    }
    state->swap_failed=0;return 0;
}
#endif
