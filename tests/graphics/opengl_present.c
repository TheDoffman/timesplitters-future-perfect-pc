#ifdef NDEBUG
#undef NDEBUG
#endif
#include "opengl_present.h"
#include <assert.h>
#include <stdio.h>
typedef struct {int sets,swaps,interval,set_ok,swap_ok;} Driver;
static int set(void *context,int interval,const char **reason)
{Driver *d=context;d->sets++;d->interval=interval;*reason="mock swap control unsupported";return d->set_ok;}
static int swap(void *context,const char **reason)
{Driver *d=context;d->swaps++;*reason="mock swap failure";return d->swap_ok;}
int main(void)
{
    TsfpGlPresentState state={0};Driver d={0};d.swap_ok=1;int saved_preference=0;
    assert(tsfp_gl_present_driver(&state,&d,saved_preference,set,swap)==0);
    assert(d.sets==1 && d.swaps==1 && d.interval==0 && saved_preference==0);
    assert(tsfp_gl_present_driver(&state,&d,0,set,swap)==0);
    assert(d.sets==1 && d.swaps==2);
    assert(tsfp_gl_present_driver(&state,&d,1,set,swap)==0);
    assert(d.sets==2 && d.swaps==3 && d.interval==1 && saved_preference==0);
    d.set_ok=1;
    assert(tsfp_gl_present_driver(&state,&d,0,set,swap)==0);
    assert(d.sets==3 && d.swaps==4 && d.interval==0);
    d.swap_ok=0;
    assert(tsfp_gl_present_driver(&state,&d,0,set,swap)==-1);
    assert(tsfp_gl_present_driver(&state,&d,0,set,swap)==-1);
    assert(state.swap_failed && d.sets==3 && d.swaps==6);
    d.swap_ok=1;assert(tsfp_gl_present_driver(&state,&d,0,set,swap)==0);assert(!state.swap_failed);
    d.swap_ok=0;assert(tsfp_gl_present_driver(&state,&d,0,set,swap)==-1);
    TsfpGlPresentState recreated={0};d.swap_ok=1;
    assert(tsfp_gl_present_driver(&recreated,&d,0,set,swap)==0 && d.sets==4);
    assert(tsfp_gl_present_driver(NULL,&d,0,set,swap)==-1);
    puts("PASS: optional swap control, real swap failures, retry and preference preservation");return 0;
}
